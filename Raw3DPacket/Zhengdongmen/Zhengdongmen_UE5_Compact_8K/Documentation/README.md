# Zhengdongmen / Great East Gate — COMPACT 8K-color material package

This is the re-downloadable lightweight replacement for the earlier oversized ~4.3 GB archive. It includes the **same original procedural gate geometry** (five independent GLB versions, collision reference) with the six newly generated material illustrations actually rebound as embedded preview base colors in each GLB, along with existing sign artwork. For final textures, use the 8K JPG Base Color files under `Textures`.

## File sizes and quality tradeoffs

* `BaseColor` maps: **8192 x 8192 JPEG**, quality 82, 7 material maps. They were upscaled from original illustrations that were approximately 1254 x 1254 pixels (the sign was 4096 square): these are 8K image dimensions, not genuine 8K captured detail.
* `Normal`, `Roughness`, `Metallic`, `AO`, `Height`: **1024 x 1024 PNG**, 5 maps per material, approximate procedural maps newly matched to the refreshed base colors. This is an intentional download-size compromise; the previous 8K maps were upscales, not native-detail maps.
* Every GLB has 384 x 384 preview Base Color maps **actually applied** to the seven named material slots. This makes the GLBs smaller. Import external 8K color maps in UE5 to obtain 8K visible color detail.
* This package does **not** contain full-resolution 8K normal/roughness/metallic/AO/height maps, compiled Unreal `.uasset` files, true AAA geometry, nor verified second UV lightmap channels. It is the original modest-detail procedural gate blockout with upgraded color textures.

## UE5 import

1. Import `Meshes/Zhengdongmen_LOD0.glb` via glTF/Interchange or convert using `Scripts/export_fbx_in_blender.py` in Blender. Importing every LOD into the same location makes five gates overlap.
2. Import seven 8K JPEG Base Colors plus corresponding 1K mask/normal PNGs and set the material slots using `Documentation/MATERIAL_ASSIGNMENTS.json`. Base Color uses sRGB. For the Normal map choose Normalmap compression and disable sRGB. Scalar maps need sRGB disabled.
3. If normal bump directions appear inverted, flip green: generated normals use glTF/OpenGL +Y convention. In real UE assets a verified bake from high-poly geometry is preferable.
4. Verify glTF meter-to-UE-centimeter conversion and gate width of approximately 2800 cm; verify material UV tiling and collision in Editor. No source FBX, native UE materials or material instances are provided.

## Known fidelity limitations

The generated reference illustrations may not be perfectly tileable. Illustrated roof tile seams may visually double the modeled roof tile geometry. The DoorWood image depicts studs/knockers as image detail, not separate geometry, and its metallic mask does not precisely isolate the metal hardware. Building shape, scale, UVs, geometry, materials and historicity are not certified AAA production-ready. This asset should be manually reviewed in UE5.
