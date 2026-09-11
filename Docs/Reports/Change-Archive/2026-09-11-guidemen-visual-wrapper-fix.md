# Guidemen visual wrapper and rotation fix

## Intent

Inspect the newly upgraded Guidemen visually and fix the level representation so the building behaves as one actor while retaining the correct orientation relative to the Scifi Desert gate row.

## Findings

- The visual capture showed the Guidemen geometry aligned with the adjacent gate row.
- Live actor inspection showed all Guidemen parts and adjacent `ImportedGreatWestGate` parts at yaw `0°`; changing the yaw would have introduced a new visual mismatch.
- The actual issue was structural: the ten mesh pieces were separate top-level `StaticMeshActor` siblings.
- A first attachment attempt was rejected because Unreal will not attach static mesh components to a movable parent root. The parent root was corrected to `Static` and the attachment completed.

## Changed behavior

- Added one level root actor labeled `GuangzhouLandmark_Guidemen`.
- Root tags: `ImportedGuidemenBuilding`, `GuidemenRealismUpgrade`, `GuidemenRealismUpgradeRoot`, `GuangzhouLandmarkPark`.
- Attached all ten existing Guidemen mesh actors beneath the root with `KEEP_WORLD` rules, preserving the current visuals and transform.
- Confirmed root rotation is `(roll=0°, pitch=0°, yaw=0°)`, matching the adjacent gate row.
- Preserved location `(-140400.0, 110295.0, 99.9993) cm`, bounds `3060 × 1394 × 1897 cm`, and terrain contact.

## Visual and automated validation

- Before capture: `Saved/RawModelImport/guidemen_visual_before.png`.
- After capture: `Saved/RawModelImport/guidemen_visual_after.png`; inspected and visually matched the pre-wrapper placement.
- `python Scripts/remote_run.py Scripts/WrapGuidemenUpgradeActor.py` — passed; 10 parts attached under one root.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenActorWrapper.py` — passed; root, tags, 10 parent links, yaw, location, and bounds verified.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenRealismUpgrade.py` — passed after updating it to validate the wrapped hierarchy.
- Python compilation for wrapper, wrapper validator, realism validator, and capture script — passed.
- The level was saved after the wrapper change.

## Evidence

- Wrapper manifest: `Saved/RawModelImport/guidemen-wrapper.json`.
- Wrapper validation: `Saved/RawModelImport/guidemen-wrapper-validation.json`.
- [Visual summary diagram](2026-09-11-guidemen-visual-wrapper-fix.svg).
