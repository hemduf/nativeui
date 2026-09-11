# Workflow ownership

GitHub Actions cadence and authoring rules are defined in [`CI_POLICY.md`](../../CI_POLICY.md).

Key rule: normal CI is the broad integration safety net; dedicated workflows must stay path-scoped to the subsystem they own, while heavyweight lifecycle/release qualification runs only for a frozen final candidate.
