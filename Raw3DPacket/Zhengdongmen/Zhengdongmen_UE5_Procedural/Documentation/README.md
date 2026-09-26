# Guangzhou Zhengdongmen / Dadongmen — procedural reconstruction study

**Status: usable procedural reference/blockout package, NOT a complete commercially publish-ready AAA asset.** The 2D reference shows a reconstructed *Great East Gate* (大东门 / 正东门). “Zhengximen” refers to the *West* Gate, and is not this object. This mesh is an **interpretation of a single presentation board**, not a photogrammetric copy or a verified, surveyed reconstruction. The board provides no dependable metric elevations; all dimensions below are **design assumptions**.

## Delivered in this archive

- `Meshes/Zhengdongmen_LOD0.glb` — assembled, polygonal gate with a real walk-through arch and UV-mapped PBR base colors. Separate scene nodes per material, not separate, fully production-ready modular architecture.
- `Meshes/Zhengdongmen_LOD1.glb` through `LOD4.glb` — regenerated lower-detail *alternatives*, **not verified screen-size LODs** nor one FBX with UE LOD groups. Each GLB is a full gate; import **one at a time**, otherwise all five overlap.
- `Textures/T_ZDM_*_{BaseColor,Normal,Roughness,Metallic,AO,Height}_4K.png` — seven procedural material sets, six 4096×4096 maps each. Source patterns are generated at 768px and enlarged to 4K; these are NOT unique 4K scanned materials or a substitute for manual AAA texture work. The GLBs embed 512px preview base-color versions so that the GLB files remain manageable; **use the external 4K images in Unreal for final materials**. Metallic and roughness maps must be hooked up separately: GLB embeds base color and static metal/roughness factors, **not** the six full map channels.
- `Collision/Zhengdongmen_simple_collision.glb` — approximate box collision, **reference only**. It contains boxes that may not align perfectly with the detailed architectural mesh. Do not enable collision on the whole gatehouse proxy if you need interior walkability.
- `Preview/Zhengdongmen_actual_mesh_preview.png` — software render of generated LOD1 mesh (not a photograph or the provided concept image).
- `Scripts/build_gate.py`, `Scripts/render_preview.py` — reproducible editable procedural generator and preview code. `Scripts/export_fbx_in_blender.py` — optional Blender conversion for FBX/secondary UVs; **FBX is not shipped** because Blender/Unreal Editor is unavailable here.
- `Documentation/mesh_report.json` — actual per-LOD triangle counts, broken down by material. Other audit notes in this README.

## Design dimensions and frame

- Overall fortification: nominal 28 m wide, 14.8 m deep, 9.45 m high (excluding parapets); high roof ridge approximately 19.0 m above nominal ground.
- Main arched opening: nominal 5.32 m span, curved crown at ~6.06 m; pier to pier construction is invented to approximate the reference silhouette.
- Mesh units: **metres**, +Z up, -Y front/approach, pivot of all geometry at (0,0,0), ground around Z=0. Unreal works in **centimetres**, so use your importer's metre-to-centimetre conversion (100×) and verify overall width ≈ 2800 cm after import.

## Material setup in Unreal Engine 5

1. Import **only** `Zhengdongmen_LOD0.glb` initially with the Interchange/glTF pipeline if your version supports GLB, or use Blender (script included) to export FBX locally. In import options combine the material scene nodes into a single static mesh only if desired; names may differ by importer. Ensure scene orientation places the roof above the walls.
2. Rebuild each named material using the corresponding external 4K images: BaseColor→Base Color (sRGB on), Normal→Normal (set texture compression Normalmap, sRGB off), Roughness→Roughness (sRGB off), Metallic→Metallic (sRGB off), AO→Ambient Occlusion (sRGB off). Height is provided as a standalone mask and requires your chosen parallax or displacement implementation. These maps are not automatically wired in the GLB.
3. Texture UV0 intentionally repeats on tiled surfaces and therefore has overlapping or out-of-[0,1] coordinates. **The included GLBs do not have secondary non-overlapping lightmap UVs**. The optional Blender script can generate a second channel (heavy on large mesh); review and repack it by hand before relying on baked lighting. Lumen dynamic GI does not itself create a valid lightmap channel.
4. Apply collision carefully: assign individual simplified shapes or make explicit `UCX_` collision in Blender and export alongside the **same named FBX static mesh**. A standalone GLB containing UCX-named objects does not by itself guarantee Unreal import recognition. Add NavMesh for traversable gate arch after checking clearances.
5. Nanite can be switched on after import for LOD0, but it does not repair visual or topological defects. LOD1–LOD4 are separate fallback reference files rather than configured UE screen-size thresholds.

## Remaining work for requested AAA commercial specification

The package is **not** an exact match to orthographic/reference photos; building proportions, actual site scale, arch geometry, roof structural load path and history need verification. Roof tiles are simplified box-like strokes rather than accurate interlocking curved ceramic profiles. The plaster/window assembly, bridge across the aperture, and roof geometry are stylized. Rework architecture to construction-valid watertight modules, hand-author tile ridge beasts, structural dougong, windows, wood joinery, internal stairs, drainage and signage. Validate UV seams/lightmap packing, per-mesh normals and tangents, geometry intersections, meshes' collision fidelity, LOD silhouette errors, UE material parameter instances and UE platform performance. **Neither .uasset files nor a UE-opened .uproject are supplied**; those must be created in an actual installed Unreal Editor. An FBX can be generated on a local workstation with Blender and the script; a true shipped, UE-tested FBX/.uasset artifact is not claimed.

The original reference board supplied in chat may contain third-party images and text. No third-party images are copied into output textures; the textures and geometry are generated by the included procedural code. You are responsible for reviewing any branding, signage, historical-accuracy or commercial-use requirements before release.

## Validated generated geometry (this delivery)

| Scene | Triangles |
|---|---:|
| LOD0 | 50,442 |
| LOD1 | 16,538 |
| LOD2 | 5,550 |
| LOD3 | 1,130 |
| LOD4 | 962 |

All five mesh files have been round-trip loaded from the exported GLB and checked for triangle-only geometry, UV0 presence, and plausible bounds. This is a procedural visual blockout, not a high-poly commercial hero asset. The 4K texture resolution describes output dimensions only, not unique authored 4K detail. The optional Blender script is unexecuted and **FBX, .uasset, instance files, socket definitions and nonoverlapping lightmap UVs are not delivered**.
