# Great West Gate visual wrapper and rotation fix

## Intent

Apply the Guidemen fix pattern to the Great West Gate: inspect the level representation, make the building behave as one selectable actor, and preserve the correct rotation and placement in the Scifi Desert level.

## Findings

- The visual capture showed the Great West Gate correctly aligned with the existing landmark row at yaw `0°`.
- Live inspection found 20 top-level Great West Gate mesh actors with no parent actor.
- The issue was structural rather than a rotation error: the parts were siblings, making the building difficult to move, rotate, or select as one unit.

## Changed behavior

- Added one static root actor labeled `GuangzhouLandmark_GreatWestGate`.
- Root tags: `ImportedGreatWestGate`, `GreatWestGateRoot`, `GuangzhouLandmarkPark`.
- Attached all 20 existing Great West Gate mesh actors beneath the root with `KEEP_WORLD` rules.
- Preserved root rotation `(roll=0°, pitch=0°, yaw=0°)`, location `(-140400.0, 101366.0, 828.2557) cm`, bounds `4005.55 × 4005.23 × 1456.51 cm`, and terrain contact at `Z≈100 cm`.
- Updated the placement validator to validate the wrapped hierarchy and parent links.

## Visual and automated validation

- Before capture: `Saved/RawModelImport/great_west_gate_visual_before.png`.
- After capture: `Saved/RawModelImport/great_west_gate_visual_after.png`; both were inspected from the same deterministic Unreal editor camera and visually matched.
- `python Scripts/remote_run.py Scripts/WrapGreatWestGateActor.py` — passed; 20 parts attached under one root.
- `python Scripts/remote_run.py Scripts/ValidateGreatWestGateActorWrapper.py` — passed after save/reload; root, tags, 20 parent links, yaw, location, and bounds verified.
- `python Scripts/remote_run.py Scripts/ValidateGreatWestGatePlacement.py` — passed; all 20 mesh references, material slots, grounding, footprint, hierarchy, and overlap checks passed.
- Python compilation and `git diff --check` passed.

## Evidence

- Wrapper manifest: `Saved/RawModelImport/great-west-gate-wrapper.json`.
- Wrapper validation: `Saved/RawModelImport/great-west-gate-wrapper-validation.json`.
- [Visual summary diagram](2026-09-11-great-west-gate-visual-wrapper-fix.svg).
