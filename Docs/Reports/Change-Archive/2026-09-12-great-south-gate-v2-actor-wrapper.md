# Great South Gate V2 actor wrapper

## Intent

Make the imported Great South Gate V2 and its intact façade infill selectable and movable as one landmark in the isolated preview level.

## Changed behavior

- Added the saved root Actor `GuangzhouLandmark_GreatSouthGate_Zhengnanmen_V2` to `GreatSouthGate_Zhengnanmen_HighFidelity_Preview`.
- Attached 12,388 model actors under that root with `KEEP_WORLD` rules: 12,195 original imported scene mesh actors, 192 solid infill panels, and the imported scene root.
- Kept `DirectionalLight` and `SkyLight` outside the wrapper as level-owned preview lighting.
- Preserved the 112 shared Nanite mesh assets; the wrapper is a hierarchy/selection root, not a destructive mesh merge.

## Validation

- `python -m py_compile Scripts/WrapGreatSouthGateZhengnanmenV2.py Scripts/ValidateGreatSouthGateZhengnanmenV2Wrapper.py Scripts/ValidateGreatSouthGateZhengnanmenHighFidelity.py` — PASS.
- `$env:UE_ENGINE_ROOT='C:\Git\UE_5.5'; python Scripts\remote_run.py Scripts\WrapGreatSouthGateZhengnanmenV2.py` — PASS; saved the preview map and recorded matching before/after transform digests.
- `$env:UE_ENGINE_ROOT='C:\Git\UE_5.5'; python Scripts\remote_run.py Scripts\ValidateGreatSouthGateZhengnanmenV2Wrapper.py` — PASS; 12,388 direct model-child links, 2 lights outside, and 0.0 measured reload drift for location, rotation, and scale.
- `$env:UE_ENGINE_ROOT='C:\Git\UE_5.5'; python Scripts\remote_run.py Scripts\ValidateGreatSouthGateZhengnanmenIntactEnhancement.py` — PASS; 192 infill actors and 3 floor-core material overrides.
- `$env:UE_ENGINE_ROOT='C:\Git\UE_5.5'; python Scripts\remote_run.py Scripts\ValidateGreatSouthGateZhengnanmenHighFidelity.py` — PASS; 112 meshes, 12,195 original scene actors, and expected bounds.

## Evidence

- Wrapper manifest: `Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-wrapper.json`
- Wrapper reload validation: `Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-wrapper-validation.json`
- Preview level: `Content/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview.umap`
- Diagram: [2026-09-12-great-south-gate-v2-actor-wrapper.svg](2026-09-12-great-south-gate-v2-actor-wrapper.svg)
