# 文明門 / Wenmingmen — interpretive 3D reconstruction for Unreal Engine 5

## Current package status

This is a procedural interpretation of the supplied Wenming Gate concept board, not a measured historical survey or a photogrammetry scan. Dimensions, lettering, and architectural details are design approximations. The current source contains **380,586 base triangles** and **2,140,582 triangles** in `Wenmingmen_HighDetail.glb` (see `asset_manifest.json`), below the earlier 100-million-triangle request.

**Native Unreal reimport completed in UE 5.5.** The existing `/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen/BP_Wenmingmen` now references the refreshed native meshes and textures at their original asset paths. `Scripts/UpdateWenmingmen.py` first reimported eight meshes and 25 textures and preserved all eight Blueprint component names; later arch and pavilion-window passes reimported their changed mesh sets in isolated updates. A separate fresh `Scripts/ValidateWenmingmen.py` run loaded and spawned the saved Blueprint with matching **6500.0 × 2806.9 × 2199.0 cm** bounds. The latest window update log is `Saved/RawModelImport/Wenmingmen-windows-update.log`; the matching fresh reload log is `Saved/RawModelImport/Wenmingmen-windows-validation.log`.

## Files and modeled changes

- `Wenmingmen_HighDetail.glb`: the current self-contained GLB source. It has eight material-based mesh nodes, including a transparent inscription overlay, with embedded PBR textures. The stone and limestone meshes receive subdivision; the physically modeled tile roof receives a lighter subdivision pass.
- `Wenmingmen_Interpretive.glb`: the 380,586-triangle base model used to build the high-detail source.
- `source/build_wenmingmen.py`: editable source for the gate, curtain walls, pavilion, roofs, bridge, canal, and details. The gate has a **closed, recessed double timber door shaped to the arch**, with jamb and curved frame. The central facade uses coursed pale ashlar and radial arch stones, while the flanking curtains use smaller, darker masonry courses. The upper pavilion has recessed paired timber shutters behind its lattice windows and closed end bays; the lower balcony remains open behind its railing. The two roofs have overlapping clay pans and rounded caps over their joints. The raised bridge has two arched openings and a curved deck.
- `source/create_high_detail.py`: adds selective subdivision and textured inscription overlays to the base GLB. The main plaque reads 文明門 right-to-left (visible order 門明文). The two vertical side couplets have front and rear overlays. Their wording is interpretive and should not be treated as verified archival text.
- `materials_v2/`: four reference-guided generated material sources for gray wall stone, pale limestone, dark clay roof tile, and aged timber, plus the map-generation script and material preview. See `materials_v2/README.md` for prompts, map derivation, and regeneration commands. These are generated surfaces informed by the concept image, not scans of the original gate.
- `textures/`: the four material sets above as 2048 × 2048 BaseColor, Normal, and packed ORM maps; 2K procedural plaster; 1K iron, water, and foliage sets; and the inscription atlas. The four generated sets are embedded in the current GLBs. The BaseColor, Normal, and ORM maps remain separate for native UE texture import.
- `reference_concept.png`: the supplied artistic concept board, not an archival photograph.
- `asset_manifest.json`: machine-readable mesh counts, texture dimensions, and scope notes.

## Source regeneration

Run the commands from the repository root in a Python environment with NumPy, SciPy, Pillow, and trimesh. The generated four-material set can be rebuilt and installed using the command in `materials_v2/README.md`. When rebuilding geometry, pass `--reuse-textures` so the older procedural texture generator does not overwrite those maps:

```sh
python ContentSource/GuangzhouLandmarks/Wenmingmen/source/build_wenmingmen.py --reuse-textures
python ContentSource/GuangzhouLandmarks/Wenmingmen/source/create_high_detail.py
```

The second script supports `--subdiv` for stone and limestone and `--roof-subdiv` for the modeled roof. More subdivision increases memory use and file size without adding new architectural shapes.

## Unreal import and validation

1. After changing the GLB or source textures, reimport them into the existing native assets with `Scripts/UpdateWenmingmen.py` from a UE 5.5 editor or Python commandlet. `Scripts/ImportWenmingmen.py` is the create-only first-import path and skips already-validated assets.
2. Keep the eight existing Blueprint component paths and material slots aligned with the GLB nodes. Check the import orientation and scale: source coordinates are meters and Z-up; the existing Unreal import applies a −90° roll and represents one meter as 100 cm.
3. For packed `*_ORM.png`, R is ambient occlusion, G roughness, and B metallic. Disable sRGB for Normal and ORM. The native import flips the Normal green channel for Unreal's convention; inspect lighting after reimport.
4. On Metal SM5, Nanite is unavailable and the editor uses each mesh's raster fallback. The five Nanite-enabled structural meshes are deliberately configured to `PERCENT_TRIANGLES` at `1.0` with relative error `0.0`; the import and update scripts enforce this so the fallback does not collapse into coarse triangular wall holes. `Scripts/FixWenmingmenFallback.py` can repair an already imported editor asset and writes a verification report.
5. Run `Scripts/ValidateWenmingmen.py` in a fresh UE process, then visually inspect front, three-quarter, arch, wall, roof, and bridge views of the Blueprint. Confirm the door fits the opening, the stone courses are coherent, the roof tiles face outward, and the bridge openings remain clear.

The GLB is interchange source, not a substitute for native mesh, material, texture, and Blueprint assets. It does not supply authored UCX collision, lightmap UV2, HLODs, material instances, a simulated water volume, or a historically verified text or survey. The existing Unreal import uses complex collision on structural meshes. The commandlet checked preserved collision and Nanite settings and material references; appearance and any level placement require viewport inspection.

## Why this is below 100 million triangles

A single GLB has a 32-bit container length and is a poor carrier for a multi-gigabyte, 100-million-triangle building plus high-resolution textures. The current 2,140,582-triangle source uses modeled architectural detail where it affects the silhouette and subdivision where it adds small surface relief. A larger scene should use reusable Nanite-ready brick, tile, and timber modules as instances rather than one monolithic interchange mesh.
