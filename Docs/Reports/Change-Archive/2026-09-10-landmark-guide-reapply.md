# Landmark Guide reapply

Intent: restore the previously implemented Landmark Guide slice after the source files were absent from the current checkout.

Changed behavior: restored the world registry and grounded fallback, native landmark panel, controller face-then-run state machine, cancellation guards, BehaviorU XML contract, authoring script, and contract tests. Existing map, asset, and unrelated working-tree changes were preserved.

Validation: the fresh `Aura` Win64 Development target links successfully; UHT and all reintroduced Landmark source files compile; the XML contract parses with all four expected methods and `BT_RUNNING` results. The `AuraEditor` final link and live editor automation are pending because the user-owned UnrealEditor process currently holds the BehaviorU runtime DLL. The prior map smoke evidence and implementation packet remain available for independent review.

Plan/status record: [Landmark guide status](../../Plans/Landmark-Guide-System-Implementation-Plan-2026-09-09.md).

Validation packet: `C:/Users/mickie/.codex/visualizations/2026/09/10/landmark-guide-reapply/implementation-validation-packet.md`.

Diagram: [Reapply flow and validation boundary](2026-09-10-landmark-guide-reapply.svg).
