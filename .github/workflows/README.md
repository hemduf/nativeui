# Workflow ownership

- `ci.yml` builds supported desktop platforms and runs CTest checks except package contracts.
- `package-contract.yml` builds an installable package and runs CTest package-consumer checks.
- `wasm.yml` validates the WebAssembly build and browser integration.

Register tests and their labels in CMake. Workflows select test groups through CTest, without naming individual tests.
