# Workflow ownership

- `ci.yml` is the **single broad pull-request qualification**: four OS/architecture builds and CTest suites, Linux ASan/UBSan, and Linux D-Bus contracts. The complete matrix is deferred on Draft PRs and starts when a PR is ready for review; the lightweight workflow-policy job also runs on Draft PRs.
- macOS ARM64 remains qualified. No macOS Intel runner is scheduled in any CI workflow; this does not claim that Intel binaries stop working, only that Intel CI coverage is intentionally retired.
- `scalar-source-validation.yml` retains exact-head feature-branch and manual qualification, but does **not** automatically repeat a full build matrix for every unrelated PR. The generic CI matrix already exercises ScalarSource in its ordinary CTest selection.
- `pugl-integration.yml` qualifies focused Pugl/native-view lifecycle changes on macOS PRs.
- `advanced-rendering-docs-validation.yml` compiles focused rendering documentation contracts on relevant changes.
- `main-smoke.yml` runs the Core unit suite after a merge to `main`.
- `package-contract.yml` builds installable packages and exercises installed-package consumers.
- `wasm.yml` validates the WebAssembly build and browser integration.

## Shared C/C++ compiler cache

- The CMake/Ninja-based host builds above use `.github/actions/setup-cpp-cache/action.yml`, which pins the sccache setup action and the sccache executable version. The WebAssembly/Emscripten workflow remains separate until its compiler-wrapper compatibility is qualified.
- The action enables CMake C/C++ compiler launchers and GitHub Actions-backed sccache. Cache keys depend on the compiler, input contents, build flags and environment. A cold/missing cache **compiles normally**; the cache never bypasses a build, any CTest checks or required release qualification.
- The existing CPM source cache remains in place: it caches downloaded Pugl/Skia dependencies, not compiled NativeUI object files. Both caches serve different purposes.
- General C++ compilation permits three parallel compiler processes. The sanitizer qualification is separately limited to two. Do not increase concurrency on memory-sensitive builds without measured runner evidence.
- The sccache action reports compilation/cache statistics as a post-run step. Compare both run wall time and cache hit rate over a cold/warm pair before making speedup claims.
- Existing PRs benefit on their next qualification against the updated `main` after this change merges; already-running workflows cannot change retroactively.

Register tests and their labels in CMake. Workflows select test groups through CTest without naming individual tests.
