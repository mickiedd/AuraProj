# Landmark guide implementation plan

Intent: turn the requested single-panel landmark selection and BehaviorU face/run interaction into an implementation-ready project plan.

Changed behavior: documentation only; no runtime behavior changed. The proposed design adds stable entrance markers, a server-validated route, a player-owned BehaviorU sequence, interruptible normal character movement, and one dedicated WebUI list panel.

Validation: inspected current HUD, player autorun/input, BehaviorU enemy agent, action result forwarding, worker/game-thread queue boundaries, landmark placement scripts and records, and packaging configuration. Reviewed five daily contracts for surfaces, authority, steps, tests, timeouts, evidence, dependencies and deferrals. Runtime/asset/navigation checks and proposed tests are pending implementation.

Plan: [Implementation contract](../../Plans/Landmark-Guide-System-Implementation-Plan-2026-09-09.md).

Illustration: [Before and proposed after](2026-09-09-landmark-guide-plan.svg).
