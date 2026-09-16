# Dadongmen V4 Deep AAA pass

Date: 2026-09-15  
Scope: Great East Gate / Dadongmen V4 only

## Intent

The earlier Dadongmen V4 material and door-detail passes were still reading as flat and under-detailed in review. This pass adds a rollback-safe `_DeepAAA` material family and a real additive hero-detail mesh so the gate gains both shader depth and silhouette-level geometry.

## Before / after

Before: the imported wall and roof relied on the prior `_AAA` family, which still showed broad tonal flattening, repeated surface response, a dark door read, and detached source vegetation shards. The gate had 60 previously authored `Detail_Dadongmen_Door_*` NoCollision components.

After: `BP_Dadongmen_V4` keeps its source/open-door ownership and existing 60 detail components, while rebinding Dadongmen source components to new siblings and mounting one `DeepAAA_DetailGeometry` NoCollision component. The additive mesh contains actual beveled/chamfered stone courses, dressed arch voussoirs and jamb blocks, parapet merlons, tunnel paving, timber posts/braces/dougong, roof-tile ribs and caps, open-door planks/braces/studs/iron straps/hinge barrels, plus anchored replacement vegetation. The source vegetation material is masked to zero opacity to remove detached shards.

## Materials

Ten new materials were authored under the Dadongmen V4 material folder:

- `M_Dadongmen_Stone_DeepAAA`
- `M_Dadongmen_Plaster_DeepAAA`
- `M_Dadongmen_Wood_DeepAAA`
- `M_Dadongmen_RoofClay_DeepAAA`
- `M_Dadongmen_Dirt_DeepAAA`
- `M_Dadongmen_Water_DeepAAA`
- `M_Dadongmen_Vegetation_DeepAAA` (masked cull for the detached source shards)
- `M_Dadongmen_VegetationLeaf_DeepAAA` (mapped attached replacement using the existing 4K Dirt channel set)
- `M_Dadongmen_DoorWood_DeepAAA`
- `M_Dadongmen_Iron_DeepAAA`

The mapped graphs combine source 4K base colour, normal, roughness, metallic, AO, and height channels with authored macro breakup, micro variation, AO/cavity grime, directional streaking, edge-wear, strengthened normal layers, and meaningful Height/BumpOffset depth. Roughness, metallic, AO, specular, clearcoat, two-sided, Nanite, masked vegetation, and blue-green translucent water behavior are set per material rather than using one shared tint treatment. The new palette preserves albedo range so masonry is not mid-grey crushed and the roof retains visible cool tile relief.

## Geometry and Blueprint

The generated FBX is `Saved/RawModelImport/V4/DeepAAA/Dadongmen_V4_DeepAAA.fbx` and imports to `Meshes/DeepAAA/Dadongmen_V4_DeepAAA`.

- 22,462 vertices and 92,248 triangles.
- 32 authored primitive groups, including 266 beveled stone courses, 270 dressed arch voussoirs, 192 side-return blocks, 44 tunnel paving stones, 18 timber posts, 36 dougong steps, 68 roof-tile ribs, 20 door planks, 42 door hardware elements, and 40 anchored vegetation pieces.
- UV0 intent is preserved on the source assemblies; source collision and placement are unchanged.
- The new hero-detail component is `NoCollision`, so it adds visual relief without changing gameplay collision.
- The established Dadongmen component frame and relative roll `-90` are preserved.

## Validation and captures

The focused validator is [ValidateDadongmenDeepAAA.py](../../../Scripts/ValidateDadongmenDeepAAA.py); it does not use the stale `ValidateV4Buildings.py` authority.

Validation commands:

```text
python -m py_compile Scripts/GenerateDadongmenDeepAAA.py Scripts/ApplyDadongmenDeepAAA.py Scripts/ValidateDadongmenDeepAAA.py Scripts/CaptureDadongmenDeepAAA.py Scripts/CaptureDadongmenDeepFront.py Scripts/CaptureDadongmenDeepClose.py Scripts/CaptureDadongmenDeepRear.py
python Scripts/remote_run.py Scripts/ApplyDadongmenDeepAAA.py
python Scripts/remote_run.py Scripts/ValidateDadongmenDeepAAA.py
python Scripts/remote_run.py Scripts/CaptureDadongmenDeepFront.py
python Scripts/remote_run.py Scripts/CaptureDadongmenDeepClose.py
python Scripts/remote_run.py Scripts/CaptureDadongmenDeepRear.py
```

Final live results:

- Apply: `DADONGMEN_DEEP_AAA_APPLIED`, 10 materials, 22,462 vertices, 92,248 triangles, 123 Blueprint material/detail rebind operations.
- Validator: `DADONGMEN_DEEP_AAA_VALIDATION_PASS`, 10 material graphs, 62 Blueprint components checked, 60 prior detail components preserved, 20 rollback assets found.
- The validator also confirms the mapped `VegetationLeaf_DeepAAA` slot, source vegetation cull, one UV0 channel on the detail mesh, Nanite/two-sided intent, Height/BumpOffset graph evidence, and unchanged UV/collision/placement flags.
- The refreshed 1600x1000 front, close, and rear captures were visually inspected for relief, door readability, roof tone, arch definition, floating geometry, and obvious intersections.

Visual evidence: [before hero](../../../Saved/RawModelImport/V4/Dadongmen_V4-scene-hero.png), [after front](../../../Saved/RawModelImport/V4/DeepAAA/Dadongmen_V4-deep-front.png), [after close](../../../Saved/RawModelImport/V4/DeepAAA/Dadongmen_V4-deep-close.png), and [after rear](../../../Saved/RawModelImport/V4/DeepAAA/Dadongmen_V4-deep-rear.png).

The machine-readable apply and validation records are [Dadongmen_V4-deep-aaa.json](../../../Saved/RawModelImport/V4/Dadongmen_V4-deep-aaa.json) and [Dadongmen_V4-deep-aaa-validation.json](../../../Saved/RawModelImport/V4/Dadongmen_V4-deep-aaa-validation.json). The visual flow is summarized in [the companion diagram](2026-09-15-dadongmen-deep-aaa.svg).

## Rollback and scope

All new work uses `_DeepAAA` names. Existing base, `_AAA`, `_ReferenceTuned`, open-door, and prior detail-layer assets remain available. The preview level was used for capture but was not saved or structurally changed by this pass. No other Guangzhou gate, shared map, or parallel-session file was intentionally modified.
