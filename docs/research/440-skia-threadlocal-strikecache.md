# T440 — Skia thread-local strike cache evaluation (research)

**Status: research complete; terminal decision recorded.**

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
| **B** | experimental skia-builder archive built with the flag ON | default (`true`) | Built in the skia-builder phase; published as the `experiment/tls-strikecache-m153` prerelease (provenance below). |
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

## Environment limits

- Cross-platform CI Release evidence was produced headlessly on GitHub-hosted
  macOS ARM64 (`macos-15`), Linux x64 (`ubuntu-24.04`) and Windows x64
  (`windows-latest`, MSVC 19.51, `/MD`) runners. Local validation used
  macOS ARM64 only.
- Headless CPU raster (`HeadlessRenderer`) only; no native
  compositor/GPU presentation latency is measured.
- No local GN/depot_tools: variant B was built by the skia-builder CI. No Skia
  build happens in this repository, and no production builder/package or
  dependency pin changes.
- The temporary cross-platform measurement workflow lived on the research
  branch only and was removed before merge; its historical copy is at commit
  `366f8bcb73a7...` (the commit that produced the final campaign). The workflow
  used a `[t440-run]`-guarded push trigger because `workflow_dispatch` requires
  the file on the default branch.
- macOS runner hardware varied between campaigns (absolute medians varied by
  roughly 2x while compiler metadata stayed identical), so cross-campaign
  absolute values are not comparable; only within-job A/B pairs are.

## Evidence

### Variant B provenance (skia-builder phase)

| Item | Value |
| --- | --- |
| Builder branch | `experiment/tls-strikecache-m153` at `9a05575`, based on the pinned builder commit `f21749b1` |
| GN delta vs production package | `skia_enable_threadlocal_strikecache = true` in `RELEASE_GN_ARGS` |
| Build run | skia-builder `Build Skia` [run 36201387398](https://github.com/hemduf/skia-builder/actions/runs/36201387398), `skip_release=true` |
| Production impact | none: `chrome/m153` tag/release untouched (tag still `f21749b1`); Linux arm64 job failed in a cold-cache `tools/git-sync-deps` sync and its rerun succeeded |
| Published archive set | prerelease `experiment/tls-strikecache-m153` (not consumed by any pin) |
| macOS universal SHA256 | `702c06a9f5f378a6d8d83a3dcb3cae610c348bb53df9ac90989524d207cab4ed` |
| Linux x64 SHA256 | `76dc54ef8d8d8cee260d40275d4437aed08f864bf11ec388392b92a93a3ce034` |
| Windows x64 gpu-md SHA256 | `43d06d61b6edf67fcc3e3a621cea391602540076d4f63fabb4ae8f7f00c9935a` |
| Milestone | `chrome/m153` (same builder commit as the pinned archive) |

Local provenance check before publication: a probe linked against the extracted
macOS archive reports `runtime_threadlocal_default=true` with `--variant B`, and
the probe rejects a variant label that does not match the archive.

### Measurement campaigns

All runs used the frozen protocol (A1 B1 A2 B2, `--threads 1,2,4,8`, two
complete 5+30 runs per variant per platform) with
`--skia-archive-sha256` supplied for both variants.

| NativeUI run | Commit | Platforms | Purpose |
| --- | --- | --- | --- |
| [36204371658](https://github.com/hemduf/nativeui/actions/runs/36204371658) | `e0cf018` | macOS, Linux | first campaign; Windows failed on MinGW portability |
| [36205522658](https://github.com/hemduf/nativeui/actions/runs/36205522658) | `b1acf4d` | macOS, Linux | after MINOR-finding corrections |
| [36206377715](https://github.com/hemduf/nativeui/actions/runs/36206377715) | `5d4d7c8` | macOS, Linux, sanitizers | first clean sanitizer campaign |
| [36209142850](https://github.com/hemduf/nativeui/actions/runs/36209142850) | `366f8bc` | macOS, Linux, Windows, sanitizers | final same-commit cross-platform campaign |

### Final campaign results (commit `366f8bc`)

Deltas are improvement percentages `1 - B/A` for latency metrics and
`throughput_B/throughput_A - 1` for throughput; `r1`/`r2` are the two complete
runs in the same job/machine.

**macOS ARM64**

| workload | K | median Δ r1/r2 | p95 Δ r1/r2 | throughput Δ r1/r2 |
| --- | --- | --- | --- | --- |
| text_layout_paint | 1 | +2.3 / +1.1 | +14.0 / −2.3 | +2.3 / +1.1 |
| text_layout_paint | 2 | +32.1 / −15.1 | +28.0 / −29.4 | +47.3 / −13.1 |
| text_layout_paint | 4 | +51.3 / +10.2 | +51.4 / +9.8 | +105.5 / +11.4 |
| text_layout_paint | 8 | +46.2 / +13.8 | +39.3 / +11.1 | +85.9 / +16.0 |
| font_size_churn | 1 | −5.5 / +6.9 | +1.5 / +0.5 | −5.2 / +7.4 |
| font_size_churn | 2 | +20.0 / −4.1 | +27.3 / +2.9 | +25.0 / −3.9 |
| font_size_churn | 4 | +26.5 / +6.8 | +41.6 / +6.5 | +36.0 / +7.3 |
| font_size_churn | 8 | +31.6 / +22.4 | +33.6 / +17.5 | +46.3 / +28.8 |

**Linux x64**

| workload | K | median Δ r1/r2 | p95 Δ r1/r2 | throughput Δ r1/r2 |
| --- | --- | --- | --- | --- |
| text_layout_paint | 1 | +0.1 / +1.4 | −1.3 / +3.6 | +0.1 / +1.4 |
| text_layout_paint | 2 | +4.2 / +7.8 | +3.7 / +7.7 | +4.4 / +8.5 |
| text_layout_paint | 4 | +2.6 / +3.3 | +2.9 / +2.5 | +2.7 / +3.4 |
| text_layout_paint | 8 | +0.9 / +1.2 | +1.3 / +1.0 | +1.0 / +1.2 |
| font_size_churn | 1 | +0.1 / +0.9 | −2.4 / +0.4 | +0.1 / +0.9 |
| font_size_churn | 2 | −3.1 / −3.2 | −2.4 / −1.5 | −3.0 / −3.1 |
| font_size_churn | 4 | −3.5 / +1.5 | −3.6 / +2.7 | −3.4 / +1.5 |
| font_size_churn | 8 | −1.9 / −1.1 | −1.3 / −0.3 | −1.9 / −1.1 |

**Windows x64 (MSVC 19.51, `/MD`)**

| workload | K | median Δ r1/r2 | p95 Δ r1/r2 | throughput Δ r1/r2 |
| --- | --- | --- | --- | --- |
| text_layout_paint | 1 | +11.1 / −0.8 | +58.2 / −11.5 | +12.4 / −0.8 |
| text_layout_paint | 2 | −18.2 / −27.5 | −45.1 / −25.9 | −15.4 / −21.6 |
| text_layout_paint | 4 | +70.3 / +56.1 | +65.1 / +58.1 | +236.7 / +128.0 |
| text_layout_paint | 8 | +70.0 / +70.1 | +69.8 / +68.6 | +232.9 / +234.6 |
| font_size_churn | 1 | +21.5 / +0.4 | +35.6 / +4.7 | +27.4 / +0.4 |
| font_size_churn | 2 | +44.8 / −2.6 | +38.9 / +11.5 | +81.2 / −2.5 |
| font_size_churn | 4 | +54.5 / +54.9 | +53.0 / +54.3 | +119.6 / +121.5 |
| font_size_churn | 8 | +62.5 / +64.7 | +66.7 / +60.2 | +166.4 / +183.6 |

### macOS reproduction across campaigns

| Campaign commit | Threshold 1 | Threshold 2 | K=1 precondition |
| --- | --- | --- | --- |
| `e0cf018` | no (K=8 p95 +17.4/+23.8) | pass | fail (+10.0/+6.4 text, +31.4/+15.1 churn) |
| `b1acf4d` | pass (text K=8 +31.5/+31.1, p95 +29.2/+29.6) | pass | fail (text pair 2 +5.8%) |
| `5d4d7c8` | pass (churn K=8 +29.2/+53.8, p95 +46.3/+51.1) | pass | fail (text +13.3%, churn +11.2%) |
| `366f8bc` | no (churn K=8 p95 +33.6/+17.5) | pass | fail (churn +5.5%) |

Linux never satisfied threshold 1 or 2 in any campaign (all deltas within ±8%
and some small `font_size_churn` regressions); the K=1 precondition always held.

### Sanitizers

- Linux ASan+UBSan (`-fsanitize=address,undefined -fno-sanitize=vptr`, the vptr
  check disabled because the pinned Skia archives have no RTTI): probe self-test
  and lifecycle/staleness checks pass; no memory-safety or UB findings.
- LSan with `detect_leaks=1` on variant B reports
  `SUMMARY: AddressSanitizer: 861984 byte(s) leaked in 1083 allocation(s)`.
  Every reported allocation is an indirect leak rooted in
  `SkStrikeCache::internalCreateStrike`, `SkStrike::addGlyphAndDigest`,
  `SkGlyphDigest` hash storage and `SkTypeface` scaler contexts created on the
  worker threads; the thread-local caches become unreachable when those threads
  exit. This is upstream `static thread_local auto* cache = new SkStrikeCache`
  retention, not a NativeUI allocation, and it is the lifecycle hazard this
  ticket was required to measure for plugin hosts.

## Pre-registered threshold evaluation

| Platform | Threshold 1 (two runs) | Threshold 2 + K=1 precondition | Threshold 3 memory | Threshold 4 isolation/staleness | Threshold 5 sanitizers |
| --- | --- | --- | --- | --- | --- |
| Windows x64 | **pass** (text K=4/8 and churn K=4/8, +53..70%) | **pass** (K=8 throughput +128..235%, precondition clean) | pass (max 119,367 B < 2 MiB) | pass | ASan/UBSan clean |
| macOS arm64 | fail (2 of 4 campaigns passed; final campaign K=8 p95 +17.5% in run 2) | throughput passes; precondition fails (3 of 4 campaigns) | pass (max 150,154 B < 2 MiB) | pass | ASan/UBSan clean; LSan retention finding |
| Linux x64 | fail | fail | pass (max 130,250 B < 2 MiB) | pass | ASan/UBSan clean |

Interpretation used for this decision: the qualification thresholds are
evaluated **per platform**, because a shared builder flag changes every
published archive and the additional rule forbids extrapolating one platform's
evidence to another. Under that reading only Windows satisfies the gates, and a
shared default changes Linux/macOS without qualifying evidence. The `ADOPT`
condition is therefore not met for a common builder configuration. The same
rule means the Windows result cannot be used to qualify the other two.

## Terminal decision

**REJECT — no change to the production Skia builder/package, no NativeUI
dependency-pin change, no default enablement.**

Rationale:

1. The tested flag is a single builder-level switch that would apply to every
   published archive. Only Windows x64 satisfies the pre-registered
   qualification gates reproducibly (thresholds 1 and 2, the K=1 precondition,
   bounded memory, clean isolation staleness, clean sanitizers). Linux x64 shows
   no qualifying improvement in any campaign, including small `font_size_churn`
   regressions. macOS ARM64 shows large K>=4 gains but fails the threshold-1
   latency pair in half of the campaigns and fails the threshold-2 single-thread
   precondition in three of four campaigns.
2. The upstream per-thread caches are never freed at thread exit. LSan measured
   861,984 bytes retained in 1,083 allocations after the instrumented variant-B
   worker threads stopped. Enabling the flag by default would trade a
   platform-specific throughput gain for process-lifetime memory retention in
   plugin hosts that create/destroy rendering threads, which conflicts with the
   ticket's isolation/lifecycle requirements.
3. No public API or installed header exposes the build detail, and no
   NativeUI-owned mutable renderer-specific `thread_local`/global state was
   introduced. The isolation contract test enforces that separation.
4. `ADOPT` would also build on a repeated evidence pattern that is not present
   on all three required platforms; per the pre-registered rules a terminal
   `NEEDS_MORE_EVIDENCE` is not applicable any more because the variant-B
   archive, cross-platform runs and sanitizer evidence now exist.

Positive-but-not-qualifying signal for a possible future ticket: Windows and
macOS K>=4/K=8 show real contention relief (up to ~2.3x aggregate throughput on
Windows). If the project wants that benefit, a separate ticket would need to
scope platform-conditional builder enablement and thread-lifetime management for
the per-thread caches; per the issue's scope-change rule this research ticket
does not create or perform that implementation.

## Decision record

- Terminal decision: `REJECT` (recorded on issue #440).
- Variant C was exercised only for early local smoke checks; it never carried
  decision weight, as pre-registered.
- The pre-registered thresholds and protocol were not changed after any
  measurement; this document only adds results and the decision.
- Final CI campaign: NativeUI run 36209142850 at commit `366f8bc` (macOS,
  Linux, Windows, sanitizers) on top of reproduction campaigns `e0cf018`,
  `b1acf4d` and `5d4d7c8`.
- The temporary measurement workflow was removed from the branch before merge;
  its historical copy is at commit `366f8bc` and the procedure is reproducible
  from this document.


