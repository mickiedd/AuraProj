# Guidemen realism upgrade

## Intent

Upgrade the placed Guidemen building with the user-supplied `Guidemen_UE5_RealismUpgrade.zip` package while preserving its existing Scifi Desert placement.

## Changed behavior

- Imported all ten aligned GLB pieces into `/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/RealismUpgrade`.
- Preserved embedded PBR material assignments and enabled Nanite on the eight smaller detail meshes.
- Imported the two high-density façade meshes at full source geometry with Nanite build disabled because Unreal estimated approximately `14.5 GB` for the Nanite/static-mesh compile, above the editor's available build budget. This avoided another editor restart while retaining the realism geometry.
- Replaced the previous single `GuangzhouLandmark_Guidemen` actor with ten aligned static-mesh actors using the tag `ImportedGuidemenBuilding` and group tag `GuidemenRealismUpgrade`.
- Kept the original transform, terrain contact, and footprint: location `(-140400.0, 110295.0, 99.9993) cm`, bounds `(-141930.0, 109703.0, -138870.0, 111097.0, 99.9993, 1997.0) cm`.
- Gameplay collision remains deferred from the earlier import and is not introduced by this upgrade.

## Validation

- Package manifest and README inspected; all ten source GLBs and the reference image were present.
- ZIP SHA-256 verified and recorded as `2d3142fa249e6e44ef6af226f2aca8c7f64985f1de92bc02ddd7b71e7e35b47d`.
- `python -c "...compile ImportGuidemenRealismUpgrade.py, ReplaceGuidemenWithRealismUpgrade.py, and ValidateGuidemenRealismUpgrade.py..."` — passed.
- `python Scripts/remote_run.py Scripts/ImportGuidemenRealismUpgrade.py` — passed; 10/10 meshes imported with assigned materials; 8 Nanite-enabled and 2 intentionally non-Nanite due memory budget.
- `python Scripts/remote_run.py Scripts/ReplaceGuidemenWithRealismUpgrade.py` — passed; old actor replaced, map saved.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenRealismUpgrade.py` — passed after save/reload: 10 actors, correct asset folder, manifest-matching bounds, grounded placement, and no external XY overlap.
- `git diff --check` — passed; only existing line-ending warnings were reported.

## Evidence

- Import report: `Saved/RawModelImport/GuidemenRealismUpgrade.json`.
- Placement report: `Saved/RawModelImport/guidemen-realism-placement.json`.
- Validation report: `Saved/RawModelImport/guidemen-realism-validation.json`.
- Saved level: `Content/Scifi_desert_city/Level/L_showcase_level.umap`.
- [Visual summary diagram](2026-09-11-guidemen-realism-upgrade.svg).

## Known limitation

The full-resolution front and back façade meshes are not Nanite-enabled in this editor configuration. If Nanite is mandatory for those two pieces, the next step is an isolated mesh-optimization or higher-memory build workflow; the current placed upgrade remains saved and usable.
