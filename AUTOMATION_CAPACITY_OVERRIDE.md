# NativeUI automation capacity override

This file records the current user-directed runtime capacity policy for the GitHub-only scheduler. GitHub live state and `#250` remain authoritative.

**Mandatory companion policy:** read [`AUTOMATION_FLOW_OVERRIDE.md`](AUTOMATION_FLOW_OVERRIDE.md) after `AUTOMATION.md`. It is the authoritative narrow override for same-cycle handoff timing, provisional review locks, reviewer-fix fast-path scheduling, closeout-biased capacity, pre-closeout main-composition checkpoints, and the live Integration queue. Where older automation text requires waiting for the next Scheduler generation solely to assign one of those handoffs or encourages source utilization at the expense of closeout, `AUTOMATION_FLOW_OVERRIDE.md` wins.

- Delivery workers: **W1, W2, W3, W4**.
- Maximum simultaneous product `source-changing` lanes: **4**.
- **Four source lanes is a safety ceiling, not a utilization target.** It is valid and often desirable to run fewer source-changing lanes when worker capacity is better spent closing, reviewing, qualifying, reconciling, or merging near-complete work.
- Integration and Reporter remain separate roles and do not consume Delivery source capacity.
- All four Delivery workers are interchangeable; affinities are tie-breakers only.
- Mandatory peer review consumes Delivery worker execution capacity even though read-only review is not itself a source-changing lane.
- When all four Delivery workers would otherwise source-edit and a mandatory peer review is ready, reserve one reviewer: effective allocation becomes **3 source + 1 review**.
- If a reviewer enters an authorized `REVIEW_FIX`, the correction counts as a source lane: effective allocation becomes **3 source + 1 review-fix source = 4**. The same-cycle fast path in `AUTOMATION_FLOW_OVERRIDE.md` is an authorized assignment mechanism when its claim/independence rules are satisfied.
- A head-valid mid-cycle `CLAIMED` review event is a provisional review lock and consumes reviewer capacity until Scheduler normalizes it or proves it stale.
- Exact-head-green closeout work (composition checkpoint, completeness matrix, self review) outranks starting a lower-value new source batch of comparable product priority.
- A safe main-composition reconciliation is source-changing capacity when it changes the PR head. Do not repeatedly synchronize every branch on every `main` change; perform the composition-driven checkpoint defined by `AUTOMATION_FLOW_OVERRIDE.md`.
- T122 documentation remains a non-source fallback only when no higher-value legal closeout, source work, mandatory review, second review, qualification, or merge is waiting.

`AUTOMATION.md` and `AUTOMATION_STATUS.md` are reconciled to four workers/four source lanes. The older `AGENTS.md` generic default of three implementation lanes is superseded **for the scheduled automation only** by this explicit capacity policy. All non-capacity development/review rules from `AGENTS.md` remain mandatory.

This policy does not weaken dependency, status-coherence, exact-head backpressure, review independence, CI, qualification, privacy, or merge requirements.