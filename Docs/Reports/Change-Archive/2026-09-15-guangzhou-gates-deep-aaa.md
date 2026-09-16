# Guangzhou V4 gates deep AAA pass

Date: 2026-09-15  
Targets: `BP_Wuxianmen_V4` and `BP_Zhengximen_V4`  
Variant: `_AAADeep`

## Intent

The previous `_AAA_Redone` / `_AAARedone` pass changed materials and bindings but left the gate meshes geometry-only unchanged. This pass adds rollback-safe material siblings and a dedicated Nanite-compatible hero-detail mesh for each target, while preserving the existing Blueprint assembly, source UV0, collision intent, placement, and all earlier material variants.

## Changed behavior

- Rebuilt 8 Wuxianmen and 7 Zhengximen `_AAADeep` materials with six authored 4K channels each: BaseColor, Normal, Roughness, Metallic, AO, and Height.
- Added macro/micro texture-coordinate scales, procedural macro breakup, AO/cavity grime, stretched streaking, Fresnel edge wear, calibrated PBR channels, meaningful Height/BumpOffset depth, and reinforced macro-plus-micro normals.
- Kept every new material opaque, two-sided, and `used_with_nanite`.
- Added real geometry rather than relying only on maps:
  - Wuxianmen: 16,436 vertices / 70,848 triangles; 568 beveled boxes, 48 arch voussoirs, 48 forged door studs, 28 roof-tile ribs, 22 bracket pins, 14 rounded eave posts, and 2 eave caps.
  - Zhengximen: 24,604 vertices / 109,136 triangles; 892 beveled boxes, 52 arch voussoirs, 32 forged door studs, 30 lower and 30 upper roof-tile ribs, 20 bracket pins, 14 rounded eave posts, and 4 eave caps.
- Mounted each detail mesh as `DeepAAA_DetailGeometry` under the target Blueprint with `NoCollision`; Wuxianmen keeps its source-facing -90-degree roll and Zhengximen keeps identity rotation.
- Rebound the source gate components to `_AAADeep` materials without changing the source mesh assets or the existing ownership structure.
- Deterministic capture scripts now wait for the asynchronous HighResShot commit and never rotate existing preview lights; preview maps are not saved.

UE 5.5's Python bindings do not expose `MaterialExpressionBlendAngleCorrectedNormals` with a usable constructor. The new graphs therefore use the portable additive macro-plus-micro normal reinforcement path and record that limitation in the persisted reports.

## Validation

Focused live-editor validators passed:

```text
GUANGZHOU_DEEP_AAA_VALIDATION_PASS {"gate":"Wuxianmen","materials_checked":8,"textures_checked":48,"detail_vertices":16436,"detail_triangles":70848,"blueprint_components_checked":10,"source_components_checked":8,"uv0_channels":1}
GUANGZHOU_DEEP_AAA_VALIDATION_PASS {"gate":"Zhengximen","materials_checked":7,"textures_checked":42,"detail_vertices":24604,"detail_triangles":109136,"blueprint_components_checked":3,"source_components_checked":1,"uv0_channels":1}
```

The validators also asserted all new material graphs use six maps, have at least 30 expressions, use Height/BumpOffset, have two normal layers, preserve source UV0/collision/placement flags, bind the correct detail mesh, and retain the appropriate `_AAA`, `_ReferenceTuned`, and `_AAA_Redone` / `_AAARedone` rollback assets.

## Visual evidence

All captures are 1600x1000 PNGs from the isolated V4 preview levels, with front, close, and rear views for each gate. The close views visibly show bevels, masonry courses, arch voussoirs, plank-and-stud doors, timber brackets, and roof-tile relief.

- Preserved baseline comparison set: Wuxianmen [front](../../../Saved/RawModelImport/V4/Wuxianmen_V4-front.png), [close](../../../Saved/RawModelImport/V4/Wuxianmen_V4-close.png), [rear](../../../Saved/RawModelImport/V4/Wuxianmen_V4-rear.png); Zhengximen [front](../../../Saved/RawModelImport/V4/Zhengximen_V4-front.png), [close](../../../Saved/RawModelImport/V4/Zhengximen_V4-close.png), [rear](../../../Saved/RawModelImport/V4/Zhengximen_V4-rear.png).
- Wuxianmen: [front](../../../Saved/RawModelImport/V4/DeepAAA/Wuxianmen_V4-deep-front.png), [close](../../../Saved/RawModelImport/V4/DeepAAA/Wuxianmen_V4-deep-close.png), [rear](../../../Saved/RawModelImport/V4/DeepAAA/Wuxianmen_V4-deep-rear.png)
- Zhengximen: [front](../../../Saved/RawModelImport/V4/DeepAAA/Zhengximen_V4-deep-front.png), [close](../../../Saved/RawModelImport/V4/DeepAAA/Zhengximen_V4-deep-close.png), [rear](../../../Saved/RawModelImport/V4/DeepAAA/Zhengximen_V4-deep-rear.png)

## Reproduction surfaces

- [geometry generator](../../../Scripts/GenerateGuangzhouLandmarkDeepGeometry.py)
- [material/Blueprint application](../../../Scripts/ApplyGuangzhouLandmarkDeepAAA.py)
- [shared focused validator](../../../Scripts/ValidateGuangzhouLandmarkDeepAAA.py)
- [shared deterministic capture harness](../../../Scripts/CaptureGuangzhouLandmarkDeepAAA.py)
- [independent local packet review](../../../Scripts/ReviewGuangzhouDeepAAAPacket.py)

The persisted reports are [Wuxianmen](../../../Saved/RawModelImport/V4/Wuxianmen_V4-deep-aaa.json), [Wuxianmen validation](../../../Saved/RawModelImport/V4/Wuxianmen_V4-deep-aaa-validation.json), [Zhengximen](../../../Saved/RawModelImport/V4/Zhengximen_V4-deep-aaa.json), and [Zhengximen validation](../../../Saved/RawModelImport/V4/Zhengximen_V4-deep-aaa-validation.json).

No existing map or level was saved or modified on disk by this pass. Existing `_AAA`, `_ReferenceTuned`, `_AAA_Redone`, and `_AAARedone` assets remain available for rollback.

[Visual summary diagram](2026-09-15-guangzhou-gates-deep-aaa.svg)
