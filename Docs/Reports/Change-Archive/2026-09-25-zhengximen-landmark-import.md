# Zhengximen / Great West Gate imported as a landmark Blueprint

**Date:** 2026-09-25
**Illustration:** [2026-09-25-zhengximen-landmark-import.svg](2026-09-25-zhengximen-landmark-import.svg)

## Intent

The user supplied `Zhengximen_GreatWestGate_UE5_Package.zip` (244 MB, 93 files) — a
self-contained UE5 art package for 正西门 / Zhengximen / the Great West Gate — together
with a reference board, and asked for it to be imported into the project **as a
building blueprint**. Zhengximen was not previously in the landmark library: `find
Content -iname "*Zhengximen*"` returned nothing, and the older Windows-era
`Scripts/ImportAndPlaceGreatWestGate.py` points at a different source GLB
(`C:/Works/Raw3DModels/SM_Zhengximen_GreatWestGate_5M_PBR.glb`), a different level
(`L_showcase_level`), and could never have run on this machine. The user separately
confirmed it should also join the showcase ring.

## Changed behaviour

Before: no Zhengximen assets existed anywhere in `Content/`. After: the landmark
library gains a complete, placeable gate.

**Destination** — mirrors the Wenmingmen / Xiaobeimen layout:

```
/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen/
  BP_Zhengximen              PackedLevelActor, 7 HierarchicalInstancedStaticMeshComponents
  Meshes/                    7 x SM_Zhengximen_<Material>
  Materials/                 M_Zhengximen_Master + 8 x MI_<Material>
  Textures/                  48 x T_<Material>_<Map>, all 4096x4096
```

| | |
|---|---|
| Triangles | **206,252** — the declared LOD0 count exactly |
| Footprint | **5320 x 1080 x 1689.68 cm** (package declares 53.2 x 10.8 x 16.9 m) |
| Import transform | roll **−90°**, uniform scale **1.0** |
| Material groups | 7 with geometry (GrayBrick, StoneFoundation, AgedWood, DarkTimber, ClayRoofTile, BlackIron, GatePlaque); LimePlaster has textures but no geometry, so it gets an MI and no mesh |

### Two findings that decided the route

**1. The package's documented import path does not work in this engine build.**
`README.md` and `UE5_IMPORT.md` both say to import `Meshes/SM_Zhengximen_LOD0.fbx`
with Combine Meshes ON, and it is the only variant carrying the UCX collision proxies
and the UV1 lightmap atlas. It was probed twice — once through the legacy
`AssetImportTask` + `FbxImportUI` route, once through
`InterchangeManager.import_asset` with an explicit `InterchangeGenericAssetsPipeline`
— and both times Interchange logs `Interchange start importing source
[...LOD0.fbx]` and never logs the matching `completed` line. No asset, no error, no
traceback: the FBX silently produces nothing. Raw evidence in
`Saved/RawModelImport/zhengximen-probe-stdout.log`, alongside four successful GLB
imports in the same run, which is what rules out "the importer was simply broken".

The GLB route does work, but `SM_Zhengximen_LOD0.glb` carries **no materials at all**
(its glTF has `"materials": []`), so importing it combined would give one material
slot for the entire gate and lose every architectural material. The per-material
modular GLBs avoid that: one mesh per material group, one material slot each, so the
material binding is by construction rather than by matching whatever the source
happened to name its slots.

The cost is the UCX collision and the UV1 atlas. Collision is rebuilt as
`CTF_USE_COMPLEX_AS_SIMPLE` on the five structural groups, which is exactly what
`ImportWenmingmen.py` does; the project lights with Lumen, so the lightmap atlas is
not on the critical path.

**2. Roll 0 lays the gate on its side, and a "tallest axis is Z" test cannot catch it.**
The GLBs are trimesh output with identity node transforms and **Z-up** POSITION values,
which is not the glTF Y-up convention, so Interchange applies its Y-up-to-Z-up
conversion and tips the model over. At roll 0 every group returns with its source Y and
Z transposed — AgedWood's 18.86 x 8.64 x 14.70 m arrives as 18.86 x 14.70 x 8.64 —
and roll −90 restores the source exactly. Uniform scale 1.0 is correct: Interchange
already converts the glTF metres to centimetres.

The first version of the importer accepted a group when its X fell in a plausible band
and Z exceeded Y. **That let two groups through transposed.** The brick wall is
genuinely 1000 cm deep and 870 cm tall, and the timber band 818 deep and 675 tall, so
"height exceeds depth" is false for correctly-oriented parts and true for transposed
ones. The importer now reads each GLB's own POSITION accessor bounds and accepts only
a (roll, scale) whose result matches that footprint within 2%, which is ground truth
rather than a heuristic. Final run: all seven groups resolved to roll −90, scale 1.0,
first candidate.

### Deliberate choices

- **Nanite fallback forced to the full source mesh** (`fallback_target =
  PERCENT_TRIANGLES`, `fallback_percent_triangles = 1.0`, `fallback_relative_error =
  0.0`). On Metal a Nanite mesh renders from its *fallback*, and the default `AUTO`
  decimates it — the documented cause of black triangular holes on walls and roofs.
- **Normals are not flipped.** The package states DirectX (Y−), which is what Unreal
  expects; `flip_green_channel` exists to convert OpenGL maps and would have inverted
  every normal. Contrast `ImportWenmingmen.py`, whose source genuinely was OpenGL.
- **Every path is project-relative**, derived from `unreal.Paths.project_dir()`. The
  package's own `Unreal/*.py` helpers hardcode `/Game/Zhengximen` and derive their root
  from `__file__`, which lands outside the project.

## Tests

Import and validation both ran as **isolated UE 5.5 Python commandlets**
(`UnrealEditor-Cmd -run=pythonscript -unattended -nopause -nullrhi`), not through the
live editor, because UE Python remote execution could not discover the running editor.
Each step was followed by a **separate fresh process**, since a green run in the process
that did the work proves nothing about what was serialised to disk.

| Step | Script | Result |
|---|---|---|
| Import meshes, textures, materials | `Scripts/ImportZhengximenLandmark.py` | `passed: true`, `problems: []`, 7/7 groups, 48 textures, 8 instances |
| Blueprint wrapper | `Scripts/CreateZhengximenLandmarkBlueprint.py` | exit 0, 7 HISMs, root `LevelInstanceComponent` |
| Landmark validation | `Scripts/ValidateZhengximenLandmark.py` | **0 errors / 0 warnings** |
| Ring rebuild | `Scripts/CreateGuangzhouLandmarkShowcase.py` | `passed: true`, 8 landmarks |
| Ring validation | `Scripts/ValidateGuangzhouLandmarkShowcase.py` | **0 errors / 0 warnings** |

The landmark validator asserts: all seven meshes load from disk; each has exactly one
material slot and it resolves to its own `MI_*`; Nanite is enabled **with the full
fallback** (a reimport silently resets it to `AUTO`, which is why it is asserted in a
fresh process rather than trusted); the five structural groups carry
`COMPLEX_AS_SIMPLE`; 48 textures exist with the right sRGB and compression; 8 instances
are parented to the master with all five textures bound; and the Blueprint spawns to
the same footprint as the union of its meshes.

Note the union is taken from the parts' **absolute** bounds, not the largest per-axis
extent. Per-part sizes under-report the building: the tallest single part is 1470 cm
against the building's 1689.7 cm, because each part has its own origin.

### Ring placement

Registered in `LANDMARKS` as `("Zhengximen", "Zhengximen/BP_Zhengximen",
"Zhengximen (Great West Gate)", 0.0)` and `Landmark_Zhengximen` in `EXPECTED_LABELS`,
plus a `FACADE_LOCAL_AXIS` entry.

**The ring radius is derived, so adding a landmark re-spaces every one of them.** It
went **8912.75 → 10254.19 cm**; Zhengximen contributes its own arc plus the 3000 cm gap.
That is arithmetic rather than a regression, and it means all seven existing gates moved.
Zhengximen itself measures **1689.7 cm** in the level, matching its Blueprint bounds, so
what got placed is the imported model. The rebuilt map reports `passed: true`, 8
landmarks, 0 XY overlaps, 0 light-isolation violations, 31 actors, and every facade
0.000° off the plaza.

**Facing was measured, not assumed.** In the package's own modular GLBs, the gate plaque
(門額) occupies local Y −3.055 .. −2.915 m and the iron door fittings Y −0.614 ..
−0.387 m. Both sit on **−Y**, so Zhengximen follows the majority local −Y front
convention and needs `facing_offset = 0.0` — unlike Wenmingmen, which needed 180.0. The
validator's facing check confirms it at 0.000° off the plaza.

## Files

**Added**

- `Scripts/ImportZhengximenLandmark.py`
- `Scripts/CreateZhengximenLandmarkBlueprint.py`
- `Scripts/ValidateZhengximenLandmark.py`
- `Scripts/CaptureZhengximenNativeBlueprint.py`
- `Scripts/_ProbeZhengximenMeshImport.py`, `Scripts/_ProbeZhengximenEditor.py`,
  `Scripts/_ProbeZhengximenRemote.py` (transient diagnostics)

**Modified**

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — `LANDMARKS` entry
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — `EXPECTED_LABELS` and
  `FACADE_LOCAL_AXIS` entries

**Generated**

- `Content/Assets/Environment/GuangzhouLandmarks/Zhengximen/` — 64 assets
  (7 meshes, 8 material instances, 1 master material, 48 textures, 1 Blueprint)
- `Saved/RawModelImport/Zhengximen-import.json`, `zhengximen-blueprint.json`,
  `zhengximen-validation.json`, plus the import/validation logs
- `Raw3DPacket/Zhengximen_GreatWestGate_UE5_Package/` — the extracted source

**Unrelated fix made on the way**

`.claude/memory/visual-change-archive.md` carried unresolved git conflict markers
(`<<<<<<< Updated upstream` / `=======` / `>>>>>>> Stashed changes`) around the
2026-09-25 Wenmingmen facade-facing entry. The upstream side was empty and the stashed
side held four real entries, so the resolution was to drop the three marker lines and
keep the content. Left uncorrected, the next append would have landed inside a conflict
block.

## Open items

- **Native visual capture is PENDING and is recorded as such rather than folded into
  the pass.** Every result above is structural: it proves the assets exist, are
  correctly configured, and spawn at the right size — it does **not** prove the gate
  *looks* right. A render needs an open editor with a live RHI, which a Python
  commandlet does not have. UE Python remote execution cannot discover the running
  editor on this machine: multicast to `239.0.0.1:6766` gets no answer even though the
  editor binds it and `PythonRemoteExecution=True` is set, and `adb` (PID 4721)
  separately holds `127.0.0.1:6766`. Run
  `Scripts/CaptureZhengximenNativeBlueprint.py` with the editor open, via
  `remote_run.py` or the editor's Python console.
- **The reference board is interpretive, not a survey.** The package's own README
  states its dimensions are proportional art-reconstruction estimates, so the
  comparison against the supplied photo is a visual judgement, not a measurement.
- **No native LODs.** The package ships authored LOD1–LOD4 for the combined mesh, but
  those are not part of the modular per-material split, and with Nanite enabled and the
  fallback forced to full density they would not be used anyway.
- **The extracted 280 MB source is staged under `Raw3DPacket/`, not `ContentSource/`,
  and is not committed.** Wenmingmen's source was staged in `ContentSource` with Git
  LFS; decide whether Zhengximen's should be too before it is needed again.
