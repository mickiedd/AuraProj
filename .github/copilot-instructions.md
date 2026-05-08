# Aura Project Agent Instructions

## Multiplayer Authority Rules

- This project is a multiplayer game with **server authority**.
- The dedicated server is the source of truth for gameplay state.
- All replication-sensitive behavior must execute on the server first, then replicate/broadcast to clients.
- Required flow for gameplay actions: client request -> server validate/apply -> server replicate to relevant clients.
- Do not apply authoritative gameplay state on clients first.
- Client-side prediction is allowed only for cosmetic responsiveness and must reconcile to server state.
- In this project, each Blueprint may have a sibling `.snapshot.json` export next to its package file; use this JSON to analyze Blueprint logic with LLM agents.
- Each Behavior Tree may also have a sibling `.snapshot.json` next to its `.uasset`; LLM agents should look for `snapshotType: AuraBehaviorTreeSnapshot` and read `analysis.rootNode`, `analysis.blackboardAssetPath`, and `packageFiles`.

## Server-First Gameplay Scope

Treat these as server-authoritative by default unless a specific system explicitly documents otherwise:

- Damage and healing
- Attribute/stat changes
- Ability activation outcomes and cooldowns
- Inventory/equipment changes
- Actor spawn/despawn and ownership
- Match/game-state transitions
- Any replicated variable that affects gameplay outcomes
