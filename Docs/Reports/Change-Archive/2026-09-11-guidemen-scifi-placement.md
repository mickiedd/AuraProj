# Guidemen building placement in Scifi Desert

## Intent

Place the imported `/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen` mesh as a building in the Scifi Desert showcase level.

## Changed behavior

- Added one `StaticMeshActor` to `/Game/Scifi_desert_city/Level/L_showcase_level`.
- Actor label: `GuangzhouLandmark_Guidemen`.
- Actor tag: `ImportedGuidemenBuilding`.
- Mesh reference: `/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen.SM_Guidemen`.
- Placement is on the western edge, between the existing Guangzhou landmark gate actors, grounded to the terrain at `Z=99.9993 cm`.
- Saved actor location is `(-140400.0, 110295.0, 99.9993) cm`; bounds are `(-141930.0, 109703.0, -138870.0, 111097.0, 99.9993, 1997.0) cm`.
- The placement script is idempotent for the actor tag and requires the level save to succeed before writing its manifest.
- Gameplay collision remains deferred from the GLB import and is not introduced by this placement.

## Validation

- `python -c "...compile PlaceGuidemenInScifiDesert.py and ValidateGuidemenPlacement.py..."` — passed.
- `python Scripts/remote_run.py Scripts/PlaceGuidemenInScifiDesert.py` — passed; map save succeeded after the dedicated server was shut down.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenPlacement.py` — passed after save/reload: exactly one tagged actor, correct mesh, manifest-matching transform and bounds, terrain contact, and no XY overlap with other static mesh actors.
- Unreal map check — `0 Error(s), 0 Warning(s)` in the editor log.
- Dedicated server processes for this project were stopped with explicit user authorization; the Unreal Editor remained open.

## Evidence

- Placement manifest: `Saved/RawModelImport/guidemen-placement.json`.
- Validation manifest: `Saved/RawModelImport/guidemen-placement-validation.json`.
- Saved level: `Content/Scifi_desert_city/Level/L_showcase_level.umap`.
- [Visual summary diagram](2026-09-11-guidemen-scifi-placement.svg).
