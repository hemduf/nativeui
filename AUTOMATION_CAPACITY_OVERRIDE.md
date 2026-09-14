# NativeUI automation capacity override

This file records the current user-directed runtime capacity override for the GitHub-only scheduler. GitHub live state and `#250` remain authoritative.

- Delivery workers: **W1, W2, W3, W4**.
- Maximum simultaneous product `source-changing` lanes: **4**.
- Integration and Reporter remain separate roles and do not consume Delivery source capacity.
- All four Delivery workers are interchangeable; existing affinities are tie-breakers only.
- T122 documentation remains the preferred non-source fallback when a Delivery worker is waiting on CI/review/final qualification and no higher-value legal work exists.
- Existing references to three Delivery workers / three source lanes in `AUTOMATION.md` are superseded by this override until that document is reconciled.

This override does not weaken dependency, status-coherence, exact-head backpressure, review, CI, qualification, or merge requirements.
