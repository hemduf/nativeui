# NativeUI automation status

**Updated:** 2026-09-23

This is a read-only planning snapshot. Live GitHub issues, PRs and workflow runs take precedence. See [`AUTOMATION.md`](AUTOMATION.md) and [`CI_POLICY.md`](CI_POLICY.md) for the current local-first process.

## Delivery capacity

```text
independent source-changing lanes: at most 2
simultaneous local builds: 1, serial
review: may overlap another independent ticket
ordinary PR merge gate: local Mac tests and applicable review
post-merge integration: Main Smoke, path-scoped checks, nightly CI
release gate: frozen SHA, full CI, T042 and T052
scheduler/reporter assignment state: retired
```

T095 / #179 has merged into `main` as the latest completed P0 effects ticket. T096 / #180 is the next Ready P0 effects ticket; T089 / #173 is independently Ready. Re-fetch GitHub before selecting another ticket.

A red integration run requires a priority regression issue and pauses affected-area merges. An independent Ready ticket may continue local development while diagnosis or remote qualification proceeds.
