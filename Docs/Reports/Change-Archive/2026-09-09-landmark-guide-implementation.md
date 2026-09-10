# Landmark guide implementation

Intent: begin the landmark guide plan with a usable catalog, a single panel, and safe player movement toward authored destinations.

Changed behavior: added a stable world registry and tag fallback, a native UMG list panel, player-controller face-then-run guidance, Stop/Close controls, input cancellation, and the staged BehaviorU action contract. Unavailable or unprojectable destinations remain disabled.

Validation: editor and game targets build successfully; Unreal automation reports 2/2 landmark contract tests passed; a final NullRHI showcase launch logs both discovered tagged landmarks as available and native panel mounting. The current map uses bounded tag fallback entrances; explicit marker authoring is still required for production-safe approach tuning. Rendered screenshot capture was unavailable because the computer-use surface exposed no Unreal native app.

Plan/status record: [Landmark guide status](../../Plans/Landmark-Guide-System-Implementation-Plan-2026-09-09.md).

Diagram: [Implementation flow and gates](2026-09-09-landmark-guide-implementation.svg).
