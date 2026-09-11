# Guidemen embedded material repair

Follow-up to the 2026-09-10 Guidemen import. Independent source inspection confirmed that the GLB contains 15 valid embedded PNG images, while the first Interchange save produced only the nine shared stone/wood/roof maps and logged six plaque/inscription no-mip warnings.

[Visual summary](2026-09-11-guidemen-material-repair.svg)

## Changed behavior

- Added `Scripts/ExtractGuidemenTextures.py` to extract all 15 embedded PNGs with hash-checked, non-overwriting output under `Saved/RawModelImport/GuidemenTextures`.
- Added `Scripts/RepairGuidemenMaterials.py` to rebuild the five texture-backed material graphs with explicit BaseColor, Normal, and ORM connections.
- Reused the nine valid Interchange texture assets and imported the six missing plaque/inscription maps; removed only the nine exact duplicate shared-map assets created by the initial repair attempt.
- Set BaseColor to sRGB, Normal and ORM to linear sampling, Normal textures to `TC_NORMALMAP`, and ORM textures to `TC_MASKS`. ORM channels remain R=AO, G=Roughness, B=Metallic.

## Validation

- GLB source parse: 15 valid PNG images, 8 materials, 10 meshes, 10 primitives.
- `python Scripts/remote_run.py Scripts/RepairGuidemenMaterials.py` — passed; repaired five materials and reported the nine reused plus six added texture assets.
- `python Scripts/remote_run.py Scripts/ValidateGuidemenImport.py` — passed; 15 saved textures have the expected color-space/compression settings, all eight mesh slots are assigned, and Nanite remains enabled.
- Final output remains `/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen` with measured bounds `3060.0 x 1394.0 x 1897.0 cm`.

## Deferred

Simple gameplay collision authoring and level placement remain separate follow-ups.
