# NativeUI performance benchmark contract

T051 defines the benchmark **measurement and comparison contract** used by later release gates. It is a same-environment regression detector, not a cross-machine performance claim.

## Build and run

The benchmark project is intentionally Release-only:

```bash
cmake -S tests/t051 -B build-t051 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_SOURCE_DIR="$PWD" \
  -DNATIVEUI_BENCHMARK_COMMIT_SHA="$(git rev-parse HEAD)"
cmake --build build-t051 --parallel
ctest --test-dir build-t051 --output-on-failure
./build-t051/nativeui_benchmarks --self-test
./build-t051/nativeui_benchmarks --json t051-results.json
```

`--filter <name>` selects an exact benchmark name or prefix. Normal benchmark execution only writes the requested result document; it never changes an approved baseline.

## Sampling protocol

Every timed workload uses the frozen T051 protocol:

- 5 warmup samples, discarded;
- 30 measured samples;
- the workload's fixed operation batch count on every sample;
- `std::chrono::steady_clock` around only the measured operation batch;
- per-sample elapsed time divided by the exact operation count;
- median = average of sorted samples 14 and 15 (zero-based);
- p95 = sorted sample 28;
- min/max are diagnostic only;
- no dynamic calibration, extra outlier removal, or automatic sample extension.

Fixture reset documented as outside the timed region is performed by `before_sample`. Logging, JSON serialization and baseline I/O are outside measured operations.

The Release CI job executes the complete fixed protocol twice consecutively on the **same runner** and archives both result documents. This pair is reproducibility evidence only: CI verifies comparison metadata is identical and reports per-workload median/p95/allocation deltas without silently widening thresholds or rewriting a baseline. If a workload is pathologically noisy on the same runner, it requires an explicit ticket/spec decision rather than runtime calibration.

## Fixed schema

Schema version 1 result records contain:

- schema/workload versions and benchmark name;
- node/action/viewport workload metadata;
- operations per sample plus 5/30 sample counts;
- median, p95, min and max ns/op;
- recurring allocations/op and bytes/op when benchmark-only instrumentation is available;
- compiler ID/full version/major version, build type, OS, architecture and NativeUI commit SHA.

Workload/schema changes that invalidate numeric comparison require an explicit schema or workload-version increment and review rationale.

## Allocation instrumentation

Allocation interception is compiled only into the standalone T051 benchmark/test executables. It does not alter `NativeUI::Core`, public headers, consumer allocators or production lifetime rules. Counting is enabled only around explicit measured operation scopes after warmup/reset.

The Release workload-contract test exercises both a known non-allocating scope and a known allocating scope. Toolchains where interception cannot be trusted must emit `allocation_metrics_available=false`; allocation regression decisions are then unavailable rather than guessed.

## Comparison policy

`tests/benchmarks/t051_benchmark_comparison.hpp` is the single comparison entry point for release-gate code. It rejects a comparison before threshold evaluation unless baseline and both independent candidate runs match on:

- schema version;
- workload version and benchmark name;
- OS and architecture;
- compiler ID and major version;
- Release build configuration;
- operations-per-sample batch count.

Commit SHA and compiler patch/minor version remain diagnostic metadata and intentionally do not prevent a same-environment base/head comparison.

A timing regression is blocking only when **both** median is more than 15% above baseline and p95 is more than 20% above baseline, and that joint regression is reproduced in **two complete independent 5+30 runs**. Exact 15%/20% boundaries are non-blocking.

When allocation metrics are comparable, allocation count **or** bytes/op more than 10% above baseline is blocking only after the same two-run confirmation. A workload explicitly designated zero-recurring-allocation instead blocks on any recurring allocation.

## Baseline ownership

T051 does not commit machine-specific nanosecond values as universal repository constants. CI produces immutable JSON result artifacts keyed by exact commit/run metadata. T052 selects and records the approved v0.1 baseline artifact on its controlled environment; later gates compare against that approved artifact using the T051 metadata and threshold rules. Baseline replacement requires explicit review rationale and never happens from the benchmark executable itself.

## `idle_invalidation`

`idle_invalidation` is a correctness gate, not a wall-clock timing benchmark. NativeUI `Tick` events do not carry wall-clock timestamps, so T051 defines one deterministic scheduler checkpoint as 10 ms of logical idle time and dispatches exactly 1,000 unchanged checkpoints, representing 10 seconds of logical idle. After the initial settled frame, framework-requested invalidation count must remain exactly zero. There is no tolerance or noisy-rerun waiver for this gate.

## Scope boundary

These are headless/core microbenchmarks. They intentionally exclude native compositor/window-server presentation latency, user-facing FPS claims, comparisons with other frameworks and audio/DSP workloads. T067 may add versioned virtual-list workloads later; T052/T071 consume T051 artifacts for release qualification.
