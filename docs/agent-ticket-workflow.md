# Agent Ticket Workflow

This workflow has moved to `docs/agent-ticket-workflow-v2.md`.

Use the v2 protocol for coordinated multi-agent work:

- `docs/agent-ticket-workflow-v2.md`
- `docs/agent-communication-protocol-v2.md`
- `docs/coordinator-work-loop-v2.md`

Mia is the coordinator/dispatcher unless the user explicitly overrides.
Workers should use direct Codex messages for live coordination and the local
`vulkan-agent-comms` board only for compact recovery state, dispatch records,
durable decisions, merge results, and fallback inbox delivery.
