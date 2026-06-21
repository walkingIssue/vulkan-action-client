# Migration Checklist: File-Polling Protocol to Direct Messaging v2

1. Archive the current `coordinator.md`, `status/`, `inbox/`, and `claims/` files under `archive/2026-06-20-v1/`.
2. Install the revised `agent-ticket-workflow-v2.md` as the tracked project workflow.
3. Install `coordinator-work-loop-v2.md` as Mia's operating prompt/work loop.
4. Install `agent-communication-protocol-v2.md` beside the workflow and link it from agent instructions.
5. Replace the local board README with `vulkan-agent-comms-README-v2.md`.
6. Create one compact status snapshot per agent and one compact coordinator state file.
7. Rename `claims/` to `dispatch/`, or treat existing claim files as archived and create coordinator-owned dispatch records for new work.
8. Keep `inbox/` only as direct-message failure fallback.
9. Assign one isolated checkout/worktree and build root to each worker.
10. Have Mia batch the next ready ticket plans into one start commit on `main`.
11. Dispatch at least two disjoint lanes concurrently and declare their owned/watched public surfaces.
12. Remove all-agent ACK, clean-kickoff ACK, four-minute claim windows, heartbeat polling, and repeated parked-status requirements.
13. Make Mia the default merge driver; issue a worker merge lease only as an exception.
14. Pilot one Class A public-surface notice and one Class B proposal to verify conflict-only replies.
15. After the pilot, measure dispatch-to-start latency, active lane concurrency, peer messages per ticket, merge-gate wait, and full-suite runs.
