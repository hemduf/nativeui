# T440 — Skia thread-local strike cache evaluation (research)

**Status: methodology frozen; measurements pending.**

This document is the pre-registered methodology for evaluating Skia M153's
`skia_enable_threadlocal_strikecache` build option
(`SK_ENABLE_THREADLOCAL_STRIKECACHE`) for NativeUI text workloads (issue #440).
The thresholds below are frozen **before** any measurement, so the later
terminal decision (`ADOPT` / `REJECT` / `NEEDS_MORE_EVIDENCE`) cannot be
reinterpreted after seeing results. Changing a threshold or the measurement
protocol invalidates this record and requires an explicit superseding revision.

## Objective

Decide whether NativeUI should ship Skia packages built with the thread-local
strike cache enabled. `ADOPT` only creates a separate Skia-builder/package
implementation ticket; this research ticket never changes the production
builder, dependency pins, or public API.

## Upstream semantics (verified against Skia `chrome/m153`)

The pinned skia-builder archive is built from skia-builder commit
`f21749b18c14976415ec30d068c3a13e8456c3a1` at tag `chrome/m153`. Relevant
upstream behavior in `src/core/SkStrikeCache.cpp` at that revision:

- `gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental` is an
  exported `SK_API bool` process-global; it defaults to `false` unless Skia was
  compiled with `SK_ENABLE_THREADLOCAL_STRIKECACHE`, in which case it defaults
  to `true`.
- Without the build flag, `GlobalStrikeCache()` returns a process-global
  heap-allocated `static` cache unless the runtime global is set true, in which
  case it returns a per-thread heap-allocated `static thread_local` cache.
- With the build flag, the per-thread path is unconditional and the runtime
  global has no effect.
- Per-cache budget defaults: 2 MiB (`SK_DEFAULT_FONT_CACHE_LIMIT`) and 2048
  entries (`SK_DEFAULT_FONT_CACHE_COUNT_LIMIT`).
- Per-thread caches are never freed at thread exit.
- `SkGraphics::GetFontCacheUsed()`, `GetFontCacheCountUsed()`,
  `GetFontCacheLimit()`, `GetFontCacheCountLimit()` and `PurgeFontCache()` act
  on the process-global cache in the OFF build and only on the calling thread's
  cache in the ON build.
- Strike cache keys are content descriptors (typeface + size + matrix), not
  NativeUI instance identity.

## Variants

| Variant | Skia archive | Runtime selector | Notes |
| --- | --- | --- | --- |
| **A** | pinned `chrome/m153` archive (flag OFF) | default (`false`) | Baseline. |
| **B** | experimental skia-builder archive built with the flag ON | default (`true`) | Not buildable in this ticket; a later skia-builder phase produces the archive. |
| **C** | pinned `chrome/m153` archive (flag OFF) | forced `true` before any text operation | Exploratory only. Never sole `ADOPT` evidence. |

Variant C exists so the pinned archive can provide an early signal without
building Skia; it is still an experimental, upstream-acknowledged switch and
does not change the terminal evidence requirement for variant B.

## Measurement protocol

`tests/t051/t440_strikecache_probe.cpp` (executable
`nativeui_t440_strikecache_probe`, Release-only, headless, linked against the
same `NativeUI::Core`/Skia as the frozen T051 benchmarks) implements:

- **Frozen T051 sampling**: 5 discarded warmup samples, 30 measured samples;
  median = average of sorted samples 14/15 (zero-based); p95 = sorted sample 28;
  `summarize_samples()` is reused unchanged from
  `tests/benchmarks/t051_benchmark_harness.hpp`. No retry, calibration,
  outlier removal or sample extension.
- **Workload `text_layout_paint`**: 50 operations per sample; each operation is
  invalidate-layout + measure + paint of the T051 `paint_text` tree shape
  (4 groups x 2 grids x 10 text leaves) at a fixed 11 px font size on a
  1024x768 headless raster surface.
- **Workload `font_size_churn`**: 24 operations per sample, cycling 24 distinct
  font sizes (11..34 px); each operation changes the size and repaints the same
  text-heavy tree, forcing strike creation per size.
- **Thread counts**: 1, 2, 4, 8 by default (`--threads <list>`). Each worker
  thread exclusively owns its own `UI` + `HeadlessRenderer` and touches it only
  on that thread. Threads persist across the whole run so a thread-local cache
  is not artificially cold per sample.
- **Aggregation**: per-thread medians/p95 are recorded, plus an aggregate wall
  value per sample (`wall_elapsed / (threads * operations_per_sample)`) and the
  derived aggregate throughput `1e9 / aggregate_median_ns_per_op` ops/s.
- **Per-thread memory**: `SkGraphics::GetFontCacheUsed()`,
  `GetFontCacheCountUsed()`, `GetFontCacheLimit()`,
  `GetFontCacheCountLimit()` are sampled on each rendering thread after its
  batch.
  - Each run records its actual `font_cache_scope`
    (`process-global` in the OFF build, `per-thread` when the thread-local
    selector is active, including variant C).
  - Raw per-thread evidence stays in
    `font_cache_used_after_bytes` / `font_cache_count_used_after` plus the
    `*_largest_thread_*` maxima.
  - The only cross-scope comparable aggregates are
    `font_cache_used_after_total_bytes` and `font_cache_count_used_total`: they
    are the process-global cache value under `process-global` scope and the sum
    of the disjoint per-thread caches under `per-thread` scope.
  - **Never sum `font_cache_used_after_bytes` across threads for comparison**:
    under `process-global` scope every entry repeats the same shared cache, so
    the sum would be the thread count times the real value. Raw per-thread
    arrays may only be compared inside one scope (and only the totals across
    A/B/C).
- **Process memory**: RSS/footprint deltas per run from platform APIs under
  `#ifdef` (macOS `task_info`/`TASK_VM_INFO`; Linux `/proc/self/statm`; Windows
  `GetProcessMemoryInfo`). T051's operator-new interception is deliberately not
  used: Skia strike caches allocate through `malloc`, which operator new cannot
  observe.
- **Result schema**: `t440-strikecache-v2`, written with `--json <path>`.
  Provenance fields: NativeUI commit SHA, `skia_archive_sha256` (see below),
  variant, runtime selector default and effective values, Skia milestone,
  compiler/build/OS/architecture metadata. The frozen T051 schema/workload ids
  are not reused or altered.

### Isolation, staleness and lifecycle checks

- **`multi_instance_lifecycle`**: two independently owned
  `UI` + `HeadlessRenderer` instances are created, rendered, pixel-checksummed
  and destroyed in alternating orders for 50 cycles. The two simultaneously
  alive instances render **different** content (11 px vs 17 px text), so a
  cache-contamination failure that returned the other instance's
  content-correct strikes would be detected. The surviving instance must still
  render a checksum identical to its own single-instance content control, and
  the two content controls must differ (otherwise the check fails as vacuous).
- **`cache_staleness_after_destruction`**: cold single-instance controls are
  rendered for both contents, then every renderer is destroyed, the font cache
  is purged on the rendering thread, and a fresh instance re-renders the
  **alternate** content. The re-render must equal the alternate cold control
  (identical fixed-content reuse cannot satisfy the check), the two controls
  must differ, and the purge must strictly reduce a non-empty cache
  (`before_purge > 0` and `after_purge == 0` or `after_purge < before_purge`).
  A no-op purge on an empty cache fails the check.

## Provenance requirements

- Record the SHA256 of every Skia archive consumed. The variant A pin is the
  skia-builder `chrome/m153` `skia-build-mac-universal-gpu-release.zip`
  (`SHA256=bbf23943d044a70b07bab3f935dc8bf725fac8f77593f2bd3f9c77cdb16bb640`
  as recorded in `cmake/Dependencies.cmake`). A variant B archive must be built
  at the same skia-builder commit/milestone and its SHA256 recorded before use.
- Decision-grade runs must pass `--skia-archive-sha256 <64 hex chars>` so the
  result document self-identifies the consumed archive. The probe normalizes the
  value to lowercase, records it in `skia_archive_sha256`, and records an empty
  string when the argument is omitted; runs without it are smoke/exploratory
  evidence only.
- Machine-readable probe JSON results stay **outside** the repository. Only the
  probe, its contracts, and this report are committed. Result files may be
  attached to the issue/PR as evidence, without local absolute paths.
- A measurement run is only comparable when variant, archive SHA256, NativeUI
  commit, compiler major, build type, OS/architecture and thread count match.

## Privacy rules

- No personal or local-machine/account information in tests, logs, artifacts or
  committed documents: no user names, host names, home directories or absolute
  local paths.
- The probe writes only build metadata (commit SHA, compiler id/version, OS,
  architecture) and numeric measurements.

## Pre-registered decision thresholds

`B` (compiled ON) vs `A` (pinned OFF) qualifies for `ADOPT` only if **all** of
the following hold:

1. **Text workload improvement**: on `text_layout_paint` or `font_size_churn`
   at any measured thread count, median improvement
   `1 - median_B / median_A >= 0.15` **and** p95 improvement
   `1 - p95_B / p95_A >= 0.20`, reproduced in **two complete independent
   5+30 runs**; or
2. **Concurrent throughput**: aggregate throughput gain
   `throughput_B(K) / throughput_A(K) - 1 >= 0.25` for at least one
   K in {4, 8}, with no single-thread median regression
   `median_B(K=1) / median_A(K=1) - 1 > 0.05` on either text workload.
3. **Memory bounded**: every rendering thread's raw
   `font_cache_used_after_bytes` entry is within the recorded
   `font_cache_limit_bytes` (2 MiB default) and the largest single-thread
   utilization/slack is reported via `font_cache_used_after_largest_thread_bytes`.
   Cross-variant memory comparisons use only the scope-aware
   `font_cache_used_after_total_bytes`. Process RSS/footprint deltas are
   reported as supporting evidence.
4. **Isolation/staleness clean**: `multi_instance_lifecycle` and
   `cache_staleness_after_destruction` both pass.
5. **No ASan/LSan findings** in the measurement runs.

Additional rules:

- Exact boundary values (15%, 20%, 25%, 5%) are non-qualifying: the
  improvements must be strictly greater than the thresholds.
- Variant C results are exploratory and cannot by themselves justify `ADOPT`.
- macOS measurements do not qualify Windows/Linux; the same Skia milestone must
  be validated on all three platforms before default enablement.
- `ADOPT` creates a separate Skia-builder/package implementation ticket; the
  default NativeUI dependency remains unchanged until that ticket lands.

### Terminal decision matrix

- `ADOPT` — thresholds 1 or 2 satisfied two-run reproducible, plus 3–5 clean,
  plus required platform evidence.
- `REJECT` — variant B shows no reproducible improvement (within run-to-run
  noise) or a regression without compensating throughput/memory benefit.
- `NEEDS_MORE_EVIDENCE` — variant B archive unavailable, cross-platform
  evidence missing, sanitizer evidence missing, or memory/isolation checks
  inconclusive.

## Environment limits for this phase

- macOS ARM64 local measurements only.
- Headless CPU raster (`HeadlessRenderer`) only; no native
  compositor/GPU presentation latency is measured.
- No local GN/depot_tools: the variant B archive is delegated to a later
  skia-builder phase; no Skia build happens in this repository.
- Windows/Linux Release runs and the sanitizer pass are still pending and are
  required before a terminal `ADOPT`.

## Evidence log

| Run | Variant | Command | Result document | Result |
| --- | --- | --- | --- | --- |
| pending | A | pending | outside repository | pending |
| pending | B | pending (skia-builder phase) | outside repository | pending |
| pending | C | pending | outside repository | pending |

## Decision record

No terminal decision yet. The decision, its thresholds, and the two-run
reproduction evidence will be recorded here and in issue #440 before the
research ticket closes.
