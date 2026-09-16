# V4 independent building import — 2026-09-15

## Intent and result
Import all four packages from `C:/Works/Raw3DModels/V4` as complete, independent Actor Blueprints. Each Blueprint can be dragged into a level and moves its entire building assembly together.

[Visual summary](2026-09-15-v4-independent-building-import.svg)

All assets are under `/Game/Assets/Environment/GuangzhouLandmarks/V4/`:

| Folder | Blueprint | Visible components | Imported mesh assets | Materials | Textures |
|---|---|---:|---:|---:|---:|
| Dadongmen | BP_Dadongmen_V4 | 1 | 5 | 7 | 42 |
| Guidemen | BP_Guidemen_V4 | 20 | 63 | 8 | 48 |
| Wuxianmen | BP_Wuxianmen_V4 | 8 | 40 | 8 | 48 |
| Zhengximen | BP_Zhengximen_V4 | 1 | 5 | 8 | 48 |

Each folder also contains a lit `L_<name>_V4_Preview` map. Primary meshes use Nanite. Alternate LODs and optional Guidemen props are imported as separate resources, rather than overlapping components in the primary Blueprint. Static collision follows visible mesh geometry; no bounding-box collider closes the arch. These are static building actors, without animated door gameplay.

## Import decisions and repairs
- glTF/GLB scenes preserve the full authored component transforms and UVs. Production textures replace embedded preview maps.
- Zhengximen GLBs omit UVs/material bindings. Its FBX files contain them but malformed inline ASCII records and an incorrect zero definition count prevent Unreal from finding meshes. A separate prepared copy normalizes record formatting and definition counts. All numerical arrays, including vertices, indices, normals and UVs, are verified unchanged. The legacy FBX importer is selected temporarily and the prior Interchange flag restored.
- Guidemen and Zhengximen source faces require two-sided materials. Matching double-sided collision geometry retains collision from either face direction.
- All 186 production PNGs decoded successfully. Guidemen's additional `.tmp.png` duplicate is excluded. Base color uses sRGB; normal and scalar maps use linear sampling. Guidemen OpenGL normal maps flip the green channel. Height/packed maps are retained as resources; height displacement is not enabled.
- Source mesh appearance is preserved, including Zhengximen's procedural horizontal wall courses/UV repetition. Dadongmen water remains an opaque static surface in its combined Nanite mesh.
- No existing gameplay level placement or prior V3 building asset is part of this change.

## Reproduction and evidence
1. Run `python Scripts/PrepareV4Buildings.py` from the project. It extracts safely under `Saved/RawModelImport/V4`, inventories source hashes and calls `PrepareV4ZhengximenFbx.py`.
2. With a saved map in the Aura editor, run `Scripts/ImportV4Buildings.py`. Imports checkpoint after each source and refuse uncheckpointed partial destinations. Completed packages are skipped on repeat runs.
3. Run `Scripts/FinalizeV4BuildingMaterials.py` for the facing compatibility settings (also applied by the final importer).
4. Run `Scripts/ValidateV4Buildings.py` in the editor. It checks complete source coverage, loaded mesh sections/UV channels, texture settings, complete spawned assemblies, transform offsets, collision settings and package-local dependency closure.
5. Run the same validator in a fresh process:

```powershell
& 'C:/Git/UnrealEngine-5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/Git/AuraProj/Aura.uproject' -run=pythonscript -script='C:/Git/AuraProj/Scripts/ValidateV4Buildings.py' -unattended -NullRHI -nosound -nosplash -nop4
```

The commandlet lacks StaticMeshEditorSubsystem, so UV-channel checks are performed in the live editor; all other assertions also run from disk in the fresh process. Both validations passed. All eight final front/rear screenshots were inspected by the implementing agent. This is local validation, not an independent reviewer verdict. A packaged game/cook and performance benchmark were not run.

Evidence: `C:/Git/AuraProj/Saved/RawModelImport/V4/` contains `archives.json`, `packages.json`, `fbx-repair.json`, four `*-import.json` files, `texture-integrity.json`, `facing-compatibility.json`, `validation-editor.json`, `validation-fresh.json`, logs, and eight `<name>_V4-front/rear.png` renders. Original source archives are untouched. Native game assets have no runtime dependency on these external source files or the preview maps.
