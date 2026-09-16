# NativeUI automation flow override — retired

**Status:** retired on 2026-09-16.

The hourly Scheduler / Delivery-worker / Reporter orchestration described by the previous version of this file is no longer active and must not be used to assign or advance work.

The canonical automation model is now [`AUTOMATION.md`](AUTOMATION.md):

- GitHub is the only durable source of truth;
- one implementation/source-changing lane is active at a time;
- independent review starts only for a frozen candidate head;
- merge happens when the exact-head review/CI/composition gate is green;
- reporting is stateless and read-only;
- scheduler generations, reservations, worker events and automatic fallback work are disabled.

Historical cycle issues and scheduler snapshots are archival evidence only. They have no authority over current issue, PR, review or CI state.

Do not restore any rule from the retired flow without an explicit documentation change to `AUTOMATION.md` backed by measured evidence from the serialized pilot.