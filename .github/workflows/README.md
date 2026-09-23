# Workflow ownership

GitHub Actions cadence and recovery rules are defined in [`CI_POLICY.md`](../../CI_POLICY.md).

- `Main Smoke` runs after each merge to `main` and can be manually dispatched.
- `CI` runs the full Linux/macOS/Windows/ARM64 and sanitizer matrix nightly or on demand.
- Dedicated subsystem workflows run after a matching merge to `main` and can be dispatched when available.
- `T042 Lifecycle Stress` runs weekly or on demand. `T052 v0.1 Release Gate` runs only for a frozen release candidate with an explicit benchmark baseline SHA.

Ticket PRs are reviewed and merged from local Mac validation. Remote checks report integration and release health; they are not required PR checks.
