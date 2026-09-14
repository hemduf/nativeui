# NativeUI automation capacity override

This file records the current user-directed runtime capacity policy for the GitHub-only scheduler. GitHub live state and `#250` remain authoritative.

- Delivery workers: **W1, W2, W3, W4**.
- Maximum simultaneous product `source-changing` lanes: **4**.
- Integration and Reporter remain separate roles and do not consume Delivery source capacity.
- All four Delivery workers are interchangeable; affinities are tie-breakers only.
- Mandatory peer review consumes Delivery worker execution capacity even though read-only review is not itself a source-changing lane.
- When all four Delivery workers would otherwise source-edit and a mandatory peer review is ready, reserve one reviewer: effective allocation becomes **3 source + 1 review**.
- If that reviewer enters explicitly assigned `REVIEW_FIX`, the correction counts as a source lane: effective allocation becomes **3 source + 1 review-fix source = 4**.
- T122 documentation remains a non-source fallback only when no higher-value legal source work or mandatory review is waiting.

`AUTOMATION.md` and `AUTOMATION_STATUS.md` are now reconciled to four workers/four source lanes. The older `AGENTS.md` generic default of three implementation lanes is superseded **for the scheduled automation only** by this explicit capacity policy. All non-capacity development/review rules from `AGENTS.md` remain mandatory.

This policy does not weaken dependency, status-coherence, exact-head backpressure, review independence, CI, qualification, privacy, or merge requirements.