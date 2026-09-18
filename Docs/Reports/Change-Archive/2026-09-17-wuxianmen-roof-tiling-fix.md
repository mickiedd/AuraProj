# Wuxianmen V5 Core roof tiling repair

Date: 2026-09-17.

The Wuxianmen Core roof material was sampling the full `0..1` UV range of `Roof_BaseColor_4K`. The upper 2048 rows contain the authored gray tile sheet, while the lower 2048 rows are black padding, which made roof and ridge surfaces read as oversized dark patches.

[Archived diagram](2026-09-17-wuxianmen-roof-tiling-fix.svg)

## Changed behavior

- Added the private material `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/Materials/M_RoofTile_ReferenceTuned_RoofTilingFix`.
- Applied one `TextureCoordinate` contract to roof base color, normal, and roughness: coordinate 0, U tiling `1.0`, V tiling `0.5`; the original UV channels and mesh vertices were not edited.
- Rebound only `HISM_002_lower_tile_1_077_GEN_VARIABLE` and `HISM_004_ridge_000010_GEN_VARIABLE` in `BP_Wuxianmen_V5_4K_Core`. The prior tuned material remains available for rollback.

## Validation

- `python Scripts/remote_run.py Scripts/ValidateV5WuxianmenRoofTiling.py` — PASS; 7 HISM components, 5,103 instances, bounds `3646.65 × 2429.23 × 1700.17 cm`, transform hashes preserved, transient actor destroyed, map not saved.
- `python Scripts/remote_run.py Scripts/ValidateV5ReferenceTuning.py` — PASS for Guidemen, Wuxianmen Core, and Wuxianmen FullPBR.
- `python -m py_compile Scripts/SnapshotV5WuxianmenRoofTiling.py Scripts/FixV5WuxianmenRoofTiling.py Scripts/ValidateV5WuxianmenRoofTiling.py Scripts/CaptureV5WuxianmenRoofTiling.py Scripts/CaptureV5WuxianmenRoofTilingSingle.py` — PASS.
- `git diff --check` — PASS.
- Source pixel audit — PASS: the roof base-color map’s lower half is fully black, and the fixed material samples only the authored upper half.

## Visual evidence

- Before: [existing Core hero capture](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reference-tuning-hero.png)
- After: [Core roof-close capture](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-roof-tiling-after-roof_close.png) and [Core hero capture](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-roof-tiling-after-hero.png)
- Capture metadata: [roof-tiling captures](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-roof-tiling-captures-20260917.json)

This pass fixes the roof texture-map tiling at the material boundary. It does not remodel the roof, retile architectural geometry, or certify cooked-build performance.
