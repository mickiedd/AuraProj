# Zhengximen / Great West Gate / 正西门 — Unreal Engine 5 Asset Package

A complete procedural source-art package derived from the supplied Zhengximen reconstruction board. The package includes a dense source mesh, authored LOD0–LOD4, FBX + GLB, 4K PBR maps, lightmap UVs, collision proxies, sockets, material-instance specifications, UE5 import scripts, and 2560×1440 preview renders.

## Geometry
- HighPoly source: **421,432 triangles**.
- LOD0: **206,252 tris**; LOD1: **92,532**; LOD2: **46,152**; LOD3: **8,748**; LOD4: **484**.
- Modular material-group GLBs are in `Meshes/Modular/`.
- LOD0 FBX includes UCX box collision proxies and socket nodes.
- FBX files contain UV0 plus a non-overlapping UV1 lightmap atlas. GLBs are convenient geometry alternatives; use the FBX source when you need the authored UV/lightmap/collision import path.

## Textures / materials
Eight architectural materials are supplied: GrayBrick, StoneFoundation, AgedWood, DarkTimber, ClayRoofTile, LimePlaster, BlackIron, and GatePlaque. Each has six **4096×4096** PNG maps: Base Color, DirectX Normal, Roughness, Metallic, AO, and Height. Material instance bindings are documented in `Materials/MI_*.json`.

## Unreal Engine import
1. Run `Unreal/Import_Zhengximen_UE5.py` or manually import `Meshes/SM_Zhengximen_LOD0.fbx` with **Combine Meshes ON**, **Import Materials OFF**, **Import Textures OFF**, **Generate Lightmap UVs OFF**, and **Nanite ON**.
2. Import `LOD1` through `LOD4` using Static Mesh Editor > LOD Import. The helper script attempts this automatically where the UE Python API supports it.
3. Run `Unreal/Build_Zhengximen_Materials_UE5.py`, then verify material slots.
4. Texture settings: BaseColor sRGB ON; Normal uses Normalmap compression; Roughness/Metallic/AO/Height sRGB OFF.
5. For Lumen, keep materials opaque and avoid excessive WPO. Height maps are optional for POM/BumpOffset or your project's Nanite displacement workflow.

## Scale / reconstructed dimensions
- Central wall width: ~25 m
- Overall span including wings: ~53 m
- Main wall depth: ~10 m
- Roof-top height: ~16.4 m
- Gate opening: ~4.5 m wide, arch top ~5.65 m

The supplied reference board does not provide explicit surveyed dimensions, so these are proportional art-reconstruction estimates, not a measured conservation/BIM record.

## File-format note
`.uasset` files are engine-version-specific binaries and cannot be reliably authored without opening the target Unreal Engine version/project. The `Unreal/Content/Zhengximen/` structure is therefore intentionally **.uasset-ready**, with source assets and scripts that create/import the project assets in your UE5 installation.

## Rights / provenance
The mesh geometry, procedural textures, scripts, and package previews were generated for this package; no third-party stock meshes or texture libraries are included. You should separately ensure you have the necessary rights to any external reference imagery you use in publication/marketing.
