# 文明門 / Wenmingmen — interpretive 3D reconstruction for Unreal Engine 5

## Read this before importing

This is an **original, procedural interpretation of the provided concept board**, NOT a historically authenticated 1880–1900 survey, scan, or perfect architectural reproduction. Architectural dimensions and placement are design approximations. The user's requested **100,000,000 triangles have NOT been produced**: the refined deliverable has **2,381,538 actual triangles** (see `asset_manifest.json`) and the original editable source has 160,096. The subdivision adds ~millimeter-scale relief to modelled stone and roof surfaces; it does not add 100 million unique sculpted details. Material textures are procedurally authored, not photo-scanned or verified against original gate surfaces.

## Files

- `Wenmingmen_HighDetail.glb`: **main self-contained deliverable**, actual 2,381,538 triangles, seven main material meshes and one character decal, embedded glTF 2.0 PBR textures. Import the **single GLB** where your UE5 build supports glTF/GLB via Interchange.
- `Wenmingmen_Interpretive.glb`: smaller base model, actual 160,096 triangles; practical for preview, manual retopology, optional external processing.
- `textures/`: original 4K stone/roof/timber BaseColor+Normal+ORM maps; 2K limestone and plaster; 1K metal/foliage/water. Separate optional plaque decal PNG. Textures are already embedded inside both GLBs except the plaque in the base GLB.
- `source/build_wenmingmen.py`: editable procedural **source model** for each architecture component. Running it regenerates the lower-detail model and the textures; it requires Python with numpy, scipy, Pillow, trimesh. `source/create_high_detail.py` reads the lower-detail GLB, subdivides selected mesh/material surfaces, adds the 文明門 sign, and writes the main high-detail GLB. Set `--subdiv 0/1/2` for smaller/larger versions: **more subdivisions demand sharply increasing memory and file size**.
- `asset_manifest.json`: machine-readable triangle counts, material map sizes, scope and limitations.
- `reference_concept.png`: the concept board supplied by the user (artistic reference, not a certified period photograph).

## UE5 import workflow

1. Enable your Unreal build's glTF/GLB Interchange import support if available, and import `Wenmingmen_HighDetail.glb` into the Content Browser. Import as **multiple static meshes** to preserve the material mesh pieces; turn on *Nanite* on opaque masonry, roof and wood meshes as appropriate. The transparent sign decal, water and small foliage may need conventional rendering rather than Nanite.
2. The source mesh is in **meters, Z-up**. Check the importer's output scale against the intended tower height (gate-ground at Z=0, upper eaves about Z=18.7 m). If it is imported 100× too small/large, adjust import scale in UE to get 1 UE unit = 1 cm. Do not assume that source architectural dimensions were measured from originals.
3. For the packed `*_ORM.png`, channel R is ambient occlusion, G is roughness, B is metallic. When manually wiring UE materials, disable **sRGB** on packed ORM and Normal maps. Unreal may require flipping the normal texture's green channel when converting an OpenGL-style glTF normal map to DirectX tangent convention; inspect the surface lighting and use the appropriate Normal texture setting/flip option.
4. Check opacity of the inscription decal and the water material in Unreal. The inscription is a transparent-texture plane in front of the stone plaque; do not use it as opaque stone. The canal water is a flat mesh with PBR water-texture placeholders, not a simulated water volume. The road, bridge and barge are part of the same visual package, not rigged gameplay props.
5. Create simple custom collision (UCX) for gate walls, walkable tunnel and bridge. The provided GLBs do **not** include UE-native collision, lightmap UV2, HLODs, material instances, blueprint, World Partition, Lumen-specific setup, or a pre-made `.uasset`. Bake authoring-quality lightmap UVs or use virtual shadow maps/dynamic lighting. Validate walkable arch opening and bridge in the actual level.
6. If GLB import is unavailable in your UE5 installation, import GLB into Blender and export **split static-mesh FBX** with the provided texture images; explicitly re-wire the maps in Unreal. Do not assume a packed FBX would preserve glTF material graph semantics.

## Why not 100 million triangles in one GLB?

A single glTF GLB has a 32-bit total container length field and is not a suitable carrier for a multi-gigabyte 100-million-triangle building mesh plus high-resolution textures. That much *unique* geometry is also unnecessarily expensive for most in-game viewpoints. To reach ~100 million **visible or instanced** triangles in a large UE city level, author reusable bricks, tiles and timber details as smaller Nanite-ready modules and place instances in the level, rather than demanding one monolithic FBX/GLB. Neither a 100-million-triangle unique mesh nor an import-tested Unreal `.uasset` is included here.
