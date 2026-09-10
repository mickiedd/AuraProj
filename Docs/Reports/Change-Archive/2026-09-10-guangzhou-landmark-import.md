# Guangzhou landmark import — 2026-09-10

Imported the four main buildings supplied in `C:/Works/Raw3DModels/` into `/Game/Assets/Environment/GuangzhouLandmarks`. [Visual summary](2026-09-10-guangzhou-landmark-import.svg).

## Changed behavior

- Great South Gate, Great North Gate, Zhenhai Tower and Xiaobeimen are now saved Nanite Static Mesh assets, with 36 assigned material slots.
- GLB sources are Z-up: import rotation is roll -90 degrees. Zhenhai coordinates are centimeters despite the glTF meter convention, so its import offset scale is 0.01. The other two GLBs use scale 1. Xiaobeimen stays at scale 1 with no rotation.
- Xiaobeimen's 12,020 OBJ groups were consolidated into 12 material groups without changing the 817,152 faces or UV indices. Original ZIPs and extracted source files remain available. A derived `SM_Xiaobeimen_Combined.obj` sits beside its extracted source.
- Unreal's initial GLB material graphs contained invalid TextureObject nodes. Rebuilt those graphs from the supplied external PBR maps. Base color uses sRGB; data maps use linear sampling; OpenGL normals are green-flipped, while North Gate uses the supplied DirectX normals. Xiaobeimen uses the supplied normal-map convention.
- All changes are isolated to the new content folder, five new Python scripts, this archive pair, and one appended project archive-index entry. Pre-existing project changes were preserved.

## Reproduction

Extract each ZIP beneath `C:/Works/Raw3DModels/Extracted/<ZIP base name>/`. From `C:/Git/AuraProj` with the matching Unreal Editor open and Python remote execution enabled:

```powershell
python Scripts/PrepareXiaobeimenObj.py
python Scripts/remote_run.py Scripts/ImportGuangzhouLandmarks.py
python Scripts/remote_run.py Scripts/ImportXiaobeimen.py
python Scripts/remote_run.py Scripts/RepairGuangzhouMaterials.py
python Scripts/remote_run.py Scripts/ValidateGuangzhouLandmarks.py
```

Import scripts reject existing destination folders; do not rerun the import steps against the completed content. The repair step is intentionally rerunnable and overwrites only these imported material graphs. The validator is read-only. `remote_run.py` prints the Unreal success flag but its shell exit code alone does not prove success; inspect the result and `Saved/RawModelImport/validation.json`.

## Validation

- Unreal 5.5.4 live editor: all four meshes loaded; 197 assets loaded; all 36 material slots assigned; dimensions and ground origins correct; Nanite enabled. Passed after repair.
- Every texture has nonzero dimensions; every textured material has connected Base Color, Normal and Roughness inputs pointing to valid textures. Black_Sign intentionally uses a constant color.
- Visual inspection in each Static Mesh Editor confirmed textured appearance and source Nanite triangle counts: 92,794 / 13,377,152 / 5,053,728 / 817,152.
- Five scripts parsed successfully; `git diff --check` passed. No C++ or Blueprint edits in this task.
- Evidence: `Saved/RawModelImport/validation.json` and the local validation packet with four screenshots, scripts, logs, and file hashes.

## Limits

No gameplay collision, collision-proxy setup, lightmap UV generation, or level placement was performed. North Gate and some other source meshes emit degenerate tangent/near-zero binormal warnings; original UV stretching, intersecting parts and other source modeling artifacts are retained. Extra automatically imported textures remain alongside the explicit PBR textures. Zhenhai includes non-power-of-two source textures. No packaged-build or gameplay tests were run. Validation was performed by the implementing agent; the current handoff skill creates a local packet and does not perform external independent review.
