# Workflow ownership

- `main-smoke.yml` runs the Core unit suite after a merge to `main`.
- `pugl-integration.yml` qualifies focused Pugl/native-view lifecycle changes on macOS PRs.
- `ci.yml` builds supported desktop platforms and runs CTest checks except package contracts, nightly and on demand.
- `package-contract.yml` builds an installable package and runs CTest package-consumer checks.
- `wasm.yml` validates the WebAssembly build and browser integration.
- `qualified-merge.yml` executes explicitly authorized, evidence-backed merge requests using the ordinary SHA-guarded pull-request API. It never checks out PR code or changes branch protections. See [the evidence and recovery contract](../automation/README.md).
- `automation-validation.yml` tests the merge executor and validates its workflow syntax on the exact PR head, with read-only permissions.

Register tests and their labels in CMake. Workflows select test groups through CTest, without naming individual tests. The standalone automation contract suite uses Node's built-in test runner and does not modify NativeUI's CTest coverage.
