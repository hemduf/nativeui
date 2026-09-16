# NativeUI automation capacity override — retired

**Status:** retired on 2026-09-16.

The previous multi-worker capacity rules are no longer active.

Current automated capacity is defined by [`AUTOMATION.md`](AUTOMATION.md):

```text
implementation/source-changing lanes: 1
independent review: on demand for the frozen candidate
reporting/watchdog: read-only only
scheduler: disabled
persistent reporter state: disabled
fallback work: disabled
```

This serialized limit is intentional. The goal is to improve completed merge throughput and reduce stale branches, duplicated diagnosis, repeated qualification heads and review handoff failures.

Do not use historical W1–W4 assignments, occupancy targets or fallback utilization rules to start work. Any future capacity increase requires an explicit change to `AUTOMATION.md` after the serialized pilot has produced enough evidence to justify parallel source work.