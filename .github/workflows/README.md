# Workflow ownership

- `main-smoke.yml` runs the Core unit suite after a merge to `main`.
- `pugl-integration.yml` qualifies focused Pugl/native-view lifecycle changes on macOS PRs.
- `ci.yml` builds macOS Intel/ARM64, Windows x64 and Linux x64/ARM64 and runs CTest checks except package contracts, on ready pull requests, nightly and on demand. Draft pull requests defer this full matrix; marking them ready launches qualification. Windows GPU checks use the pinned, checksum-verified Mesa runtime already used by focused qualification workflows.
- `package-contract.yml` builds an installable package and runs CTest package-consumer checks.
- `wasm.yml` validates the WebAssembly build and browser integration.

The canonical T184 branch, `feature/offscreen-subtree-raster-cache`, also triggers
package and WebAssembly qualification on matching pushes. This keeps exact-head
qualification available to GitHub-only workers without requiring a local
workflow-dispatch transport. Their dependency pins and test selections are the
same as for `main`.

Register tests and their labels in CMake. Workflows select test groups through CTest, without naming individual tests.
