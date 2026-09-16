# Wuxianmen AAA material regeneration — 2026-09-15

## Intent

Regenerate the Wuxianmen V4 material set to an AAA-oriented PBR presentation while preserving the authored eight-part assembly, Blueprint, geometry, UVs, placement, and collision.

## Changed behavior

- Added eight rollback-safe `M_*_AAA_Redone` materials under `/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Materials/`.
- Rebuilt each graph from the six-map 4K package: BaseColor, Normal, Roughness, Metallic, AO, and Height.
- Replaced the earlier dark multiply treatment with a controlled albedo lift/tint, low-repeat macro UV variation, height-driven BumpOffset, micro normal detail, and calibrated roughness/AO response.
- Reduced GrayBrick macro tiling to `0.38 × 0.46` to improve masonry readability while retaining the source texture detail.
- Rebound all 40 imported mesh assets across NaniteHigh and LOD0–LOD3, plus all eight unique Wuxianmen Blueprint static-mesh component templates; complex-as-simple collision was preserved.
- Retained the earlier `_AAA` and `_ReferenceTuned` siblings for rollback. No authored geometry, UVs, placement, or collision shape changes were made.

## Validation

- `python -m py_compile Scripts/RebuildWuxianmenAAAMaterials.py Scripts/ValidateWuxianmenAAARedone.py` — passed.
- `python Scripts/remote_run.py Scripts/RebuildWuxianmenAAAMaterials.py` — passed with 8 regenerated materials, 40 mesh assets rebound, and 8 Blueprint components rebound.
- `python Scripts/remote_run.py Scripts/ValidateWuxianmenAAARedone.py` — passed: 8 materials, 48 texture references, 40 meshes, 8 Blueprint components; all six map channels are used by the graphs.
- `Scripts/CaptureWuxianmenAAARedone.py` and its close-capture wrapper — completed after the persisted rebind; front and close PNGs were visually inspected.

## Evidence

- Regeneration manifest: `Saved/RawModelImport/V4/Wuxianmen_V4-aaa-redone.json`.
- Focused validation report: `Saved/RawModelImport/V4/Wuxianmen_V4-aaa-redone-validation.json`.
- Visual captures: `Saved/RawModelImport/V4/Wuxianmen_V4-front.png` and `Wuxianmen_V4-close.png`.
- [Visual summary diagram](2026-09-15-wuxianmen-aaa-material-regeneration.svg).

The standalone preview still uses its existing flat ground/horizon and source geometry/placement; this material-only pass does not address structural or layout findings from the earlier V4 review.
