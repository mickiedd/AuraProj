# Wuxianmen V5 FullPBR reference tuning — spec (full delta)

Date: 2026-09-17.
Status: **Spec ready. Live apply deferred** — Unreal Editor was down when this spec was written and the shared-editor serialization rule prohibits launching one from this agent without confirmation. A follow-up codex session (or this agent with the editor confirmed reachable) must execute `Scripts/ApplyV5WuxianmenFullPBRRefTune2.py` against `BP_Wuxianmen_V5_FullPBR` to realize this delta. No asset has been mutated yet.

## Why a spec instead of a live apply

`BP_Wuxianmen_V5_FullPBR` currently has the **revision-1 winding fix only**. It uses the same source geometry as `BP_Wuxianmen_V5_4K_Core` (`SceneImport_Wuxianmen_V5_50M_Instanced`, 7 components / 5,103 instances, the same seven source meshes), so it inherits every revision-2 defect the 4K_Core pass repaired. The user chose **full delta** (geometry + all four material fixes) and the editor was down, so this document and the ready-to-run script replace what would otherwise have been a live remote-execution pass.

The deliverable is designed so that executing it is a mechanical, deterministic replay of what revision 2 did to the sibling Blueprint — every pivot number, every transform, every material parameter, and every component binding is recorded below.

## Asset scope

| | `BP_Wuxianmen_V5_4K_Core` (already done) | `BP_Wuxianmen_V5_FullPBR` (this spec) |
| --- | --- | --- |
| Package root | `Content/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/` | `Content/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/` |
| Blueprint | `BP_Wuxianmen_V5_4K_Core` | `BP_Wuxianmen_V5_FullPBR` |
| Preview map | `L_Wuxianmen_V5_4K_Core_Preview` | `L_Wuxianmen_V5_FullPBR_Preview` |
| Source GLB | `ContentSource/.../V5ReferenceTuning20260917/Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb` | identical GLB (same source) |
| Components | 7 HISMs (`HISM_000..006`) | identical (same generated class) |
| Instance total | 5,103 | identical |
| Winding fix (reftune1) | applied | applied |
| Roof + ridge geometry fix (reftune2) | applied | **NOT applied — this delta** |
| Material reftune2 (stone / roof tile / plaster / plaque) | applied (4 new `_RefTune2` materials) | **NOT applied — this delta** |

FullPBR's `_ReferenceTuned` materials (revision 1) already exist as the rollback path:
`M_Stone_ReferenceTuned`, `M_Wood_ReferenceTuned`, `M_Plaster_ReferenceTuned`,
`M_Iron_ReferenceTuned`, `M_RoofTile_ReferenceTuned`, `M_Plaque_Wuxianmen_ReferenceTuned`.
FullPBR's `Meshes/ReferenceTuned20260917/` folder already holds the seven
winding-corrected meshes — rebinding to them was done by `RebindV5WindingVariants.py`
during the winding pass.

FullPBR does **not** yet have any `_RefTune2` materials; the spec introduces them.

## Reference evidence

- Sheet: `Docs/Reference/GateSheets/Wuxianmen-reference.png` (1448x1086, multi-panel). Read directly while authoring this spec, not from memory. The Wuxianmen gate is a two-tier hip-and-gable city-wall gatehouse over a large arched passage; weathered gray stone, warm aged timber, charcoal clay roofs, dark iron, wooden plank doors, and a `五仙門` plaque above the arch.
- Detailed reference bands (from `Wuxianmen_V5_4K_Core-reftune2-texture-analysis-20260917.json`):
  - Roof sheet (4096x4096, container mean RGB 0.307/0.293/0.282): authored rows occupy V in [0, 0.4646]; 14.629 tile columns per unit U, 5.242 courses per unit V -> 14 courses per 5.068 m strip at 0.362 m pitch.
  - Stone sheet: V period 644 px -> 6.36 stones per unit V; U period 13 px drives the per-block UV offset via custom data.
  - Wall luminance bands in the sheet run 0.27 (lower) -> 0.47 (mid) -> 0.39 (upper), confirming a deliberately desaturated, weathered palette.

## Defects to repair (mirroring revision 2)

All six are present on FullPBR because FullPBR uses the same scene nodes, the same
5,103 instances, and the same component layout. The script verifies them from the
live editor before mutating; the numbers below are the measurements recorded for the
4K_Core pass and apply unchanged.

| # | Defect | Evidence (4K_Core pass; identical on FullPBR) | Fix |
| --- | --- | --- | --- |
| 1 | **Both roof tiers inverted** (valley, not ridge). | Tile strips and wood decks rose from `z 12.75 m` at `y=0` to `z 15.72 m` at `|y|=4.35 m` (source audit). | 180 deg rotation about world X through each tier mid-plane: tiles through `z=1423.5` cm (main) / `z=1251.5` cm (lower); wood decks through `z=1403.5` cm (main) / `z=1236.5` cm (lower). 328 of 5,103 instance transforms change. A rotation, not a Z mirror, so winding and normals survive. |
| 2 | **Ridges on the wrong axis.** | `main_ridge` measured `0.42 x 22.5 x 0.42 m` along Y while the roof pitches along Y and is 8.7 m deep -> 6.9 m protruded past each eave. `lower_ridge` similarly along Y at 21 m. | Re-axis along X (length is X-extent, not Y), re-seat at the un-inverted apex, and rebind to the `main_ridge` mesh (see #3). |
| 3 | **Ridge role bound to a tile mesh.** | `HISM_004_ridge_000010` was bound to `main_tile_-1_000` while `main_ridge.uasset` sat unused. | Rebind `HISM_004` to `Meshes/ReferenceTuned20260917/main_ridge.main_ridge` (already exists in FullPBR's folder), apply the two per-tier orientations, keep the ridge role on the tile material (matches 4K_Core binding). |
| 4 | **Stone tiling inverted.** | 6.36 stones per unit V were tiled as 6.36 (shrinking each stone to ~13 mm inside a 0.56 m block). | New `M_Stone_ReferenceTuned_RefTune2`: tiling `(0.157233, 0.157233)` (= `1/6.36`), so one stone per block. Plus `num_custom_data_floats = 1` and 4,213 per-instance values driving UV offset and +-15% tone. Tint `0.53/0.50/0.44` (was `0.61/0.58/0.51`). |
| 5 | **Roof-tile tiling wrong on both axes.** | The strip is one tile column wide and one slope long. | New `M_RoofTile_ReferenceTuned_RefTune2`: U tiling `1/14.629 = 0.068357`, V tiling `0.443151` (= `0.362 m * 2.8 / 5.068 m * 14`) so 14 courses land on a course boundary with no seam. |
| 6 | **Plaque had no texture** — flat gold tint, `五仙門` unreadable. | `packages.json` mapped no BaseColor for `M_Plaque_Wuxianmen`. | New `M_Plaque_Wuxianmen_RefTune2`: tint `(1,1,1,1)`, BaseColor sample bound to `Wuxianmen_Plaque_BaseColor_2K` (a generated plaque board texture; FullPBR must generate it into `Wuxianmen_FullPBR/Textures/`). |

## Geometry repair — exact spec

### Roof flip (328 instance transforms)

For every tile instance and every wood-deck instance, replace the instance transform with a 180 deg rotation about the world X axis around the pivot for that tier. Use a rotation (not a Z mirror) so triangle winding and shading normals survive.

| Component | Tier | Pivot Z (cm) | Instances changed |
| --- | --- | --- | --- |
| `HISM_002_lower_tile_1_077_GEN_VARIABLE` (tiles) | main | 1423.5 | 168 |
| `HISM_002_lower_tile_1_077_GEN_VARIABLE` (tiles) | lower | 1251.5 | 156 |
| `HISM_006_wood_000451_GEN_VARIABLE` (decks) | main | 1403.5 | 2 |
| `HISM_006_wood_000451_GEN_VARIABLE` (decks) | lower | 1236.5 | 2 |

Tile instances are partitioned by node name prefix (`main_tile_*` vs `lower_tile_*`). Wood-deck instances are partitioned by their `y` half (`y >= 0` = main, `y < 0` = lower).

Pivot choice: the tile pivot is each tier's mid-plane (`14.235 m` / `12.515 m`); the deck pivot is set 20 cm / 15 cm lower so the rotation does not lift the deck above the tile tops. A Z mirror would reverse the vertical stacking of the deck above the tile and hide it.

Per-instance transform formula (`FTransform`):
```
P = (Px, Py, Pz)            # original translation
Q = (Qx, Qy, Qz, Qw)        # original quaternion
pivot = (0, 0, pivot_z_cm)
v  = (Px, Py, Pz - pivot_z) # translate to pivot
v' = (v.x, -v.y, -v.z)      # 180 deg rotation about X
Q' = q180x * Q              # multiply Q on the right; verify direction with one asymmetric probe before bulk apply
P' = (v'.x, v'.y + pivot_z, v'.z + pivot_z)
```

Verification: every corrected instance's transformed bounding box must equal the original's box mirrored through the pivot. The script asserts this for all 328 instances and aborts on any mismatch.

### Ridge reorientation and rebind

Rebind `HISM_004_ridge_000010_GEN_VARIABLE` to `Meshes/ReferenceTuned20260917/main_ridge.main_ridge` (already present in FullPBR's `Meshes/ReferenceTuned20260917/` folder). Then set two instance transforms explicitly:

| Tier | Index | Translation (cm) | Rotation (pitch, yaw, roll) | Scale | X span | Z range (cm) |
| --- | --- | --- | --- | --- | --- | --- |
| lower | 4 | (0, 0, 1338.0) | (-90, -90, -90) | (0.42, 0.42, 21.0) | 2100 | 1317-1359 |
| main | 9 | (0, 0, 1572.0) | (-90, -10.025, -169.975) | (0.42, 0.42, 22.5) | 2250 | 1551-1593 |

The remaining 8 ridge instances are ridge-end caps and hip covers whose authored orientations are already correct; only instances 4 (lower) and 9 (main) need reorientation. The script checks each ridge instance's `x_span_cm` and asserts `x_span > y_span` after reorientation; anything still longer along Y is a defect that must be fixed before continuing.

## Material repair — exact parameters

Create four new private materials under `Content/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/Materials/` with the exact parameter values below. The rollback path is each component's pre-existing `M_*_ReferenceTuned` material.

### `M_RoofTile_ReferenceTuned_RefTune2`
- BaseColor Tiling: U = `0.068357` (= `1/14.629`), V = `0.443151`
- UV V: `V = frac(UV * 2.8) * 0.443151` (one strip is 2.8 tile columns wide; the sheet's 14 courses are spaced 0.362 m apart, mapped across the strip's 5.068 m so the wrap lands on a course boundary)
- Tile column pitch: 0.3589 m
- Courses per strip: 14
- Course pitch: 0.362 m
- UV pins: `UVs` connected on all three samplers (BaseColor / Normal / Roughness)
- Sampled U range: `[0.0, 0.068357]`
- Sampled V per repeat: `[0.0, 0.443151]`

### `M_Stone_ReferenceTuned_RefTune2`
- Tiling U = V = `0.157233` (= `1/6.36`)
- Stones per unit V: 6.36
- Stone period: 0.56 m
- Tint: `(0.53, 0.50, 0.44, 1.0)` (was `(0.61, 0.58, 0.51, 1.0)`)
- `num_custom_data_floats = 1` (data_index = 0); assign 4,213 per-instance values
- Per-instance custom data value: pack `uv_offset` in [0, 1) and `tone_variation` in [-0.15, +0.15] into a single float (or use two `PerInstanceCustomData` material nodes if the project splits the channel). The 4K_Core pass used `set_custom_data_value(index, 0, value)` with a single packed float; mirror that.

### `M_Plaster_ReferenceTuned_RefTune2`
- Tint: `(0.62, 0.57, 0.49, 1.0)` (was `(0.72, 0.66, 0.56, 1.0)`)
- No tiling override; inherit the `_ReferenceTuned` UV convention.

### `M_Plaque_Wuxianmen_RefTune2`
- Tint: `(1.0, 1.0, 1.0, 1.0)`
- BaseColor texture: `Wuxianmen_Plaque_BaseColor_2K`. The source PNG has been generated and saved to `ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917/Wuxianmen_V5_FullPBR/Textures/Wuxianmen_Plaque_BaseColor_2K.png` (2048², dark warm wood background with subtle horizontal grain, centered gold double-border, `五仙門` rendered in SimKai at 560px in cream-gold). Regenerate with `Scripts/GenerateWuxianmenFullPBRPlaqueTexture.py` (system Python 3.10 + Pillow 9.4 — managed Python lacks PIL). The apply script imports this PNG into `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/Textures/Wuxianmen_Plaque_BaseColor_2K.Wuxianmen_Plaque_BaseColor_2K` automatically.
- Roughness: `0.8`

### Component -> material bindings (post-fix)

| Component | Material |
| --- | --- |
| `HISM_000_Wuxianmen_Plaque_GEN_VARIABLE` | `M_Plaque_Wuxianmen_RefTune2` |
| `HISM_001_iron_000088_GEN_VARIABLE` | `M_Iron_ReferenceTuned` (unchanged from reftune1) |
| `HISM_002_lower_tile_1_077_GEN_VARIABLE` | `M_RoofTile_ReferenceTuned_RefTune2` |
| `HISM_003_plaster_000016_GEN_VARIABLE` | `M_Plaster_ReferenceTuned_RefTune2` |
| `HISM_004_ridge_000010_GEN_VARIABLE` | `M_RoofTile_ReferenceTuned_RefTune2` (ridge uses tile material, matching 4K_Core) |
| `HISM_005_stone_004213_GEN_VARIABLE` | `M_Stone_ReferenceTuned_RefTune2` |
| `HISM_006_wood_000451_GEN_VARIABLE` | `M_Wood_ReferenceTuned` (unchanged from reftune1) |

## Stone per-instance custom data (4,213 values)

For each of the 4,213 stone instances in `HISM_005_stone_004213_GEN_VARIABLE`:
```
value = packed(uv_offset, tone_variation)   # one float, single data_index = 0
```
where
```
uv_offset       = hash(instance_index) % 1.0          # deterministic UV wheel offset, in [0, 1)
tone_variation  = (hash(instance_index + 0x5F3759DF) % 1000) / 1000.0 * 0.30 - 0.15   # +-15%
```
Use the same hashing scheme as the 4K_Core reftune2 pass so the wall does not repeat between the two Blueprints. The apply script reads `Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-instance-audit-20260917.json` for the recorded values when available, and otherwise regenerates them deterministically.

## Execution procedure (for the next codex session)

The script `Scripts/ApplyV5WuxianmenFullPBRRefTune2.py` performs every step below. Review it before running; the steps are deterministic but the editor must be running.

1. **Confirm serialization.** `tasklist | grep UnrealEditor` should return exactly one process. If zero, launch `C:/Git/UE_5.5/Engine/Binaries/Win64/UnrealEditor.exe Aura.uproject` and wait for `LogPython: Starting Python script plugin remote execution` in the log.
2. **Verify no dirty maps.** The previous session's `/Game/Maps/Login` is expected to be dirty; record it and **never save it**. Confirm with `unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()`.
3. **Run baseline audit.** The script writes `Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json` (component list, instance count per component, current mesh + material bindings, current tile z_top by y, ridge x/y spans, stone custom-data state). Abort if component count != 7 or instance total != 5,103.
4. **Apply roof flip.** 328 instance transforms change. The script verifies each against the Z-mirror-about-pivot rule before committing.
5. **Reorient ridges.** Rebind `HISM_004` to `main_ridge` and set the two per-tier transforms. Verify `x_span > y_span` for both.
6. **Create the four `_RefTune2` materials.** Use the exact parameters above. Generate and import the plaque board texture (the apply script handles step 5.5 automatically; this manual step covers the remaining three — stone/roof tile/plaster — via graph editing).
7. **Bind components to the new materials.** Use `set_material` on each HISM.
8. **Assign stone per-instance custom data.** Set `num_custom_data_floats = 1`, then loop and assign each value.
9. **Validate.** The script writes a validation JSON asserting: 7 components, 5,103 instances, every component has the post-fix mesh + material, every tile instance's z_top follows the un-inverted profile (apex at y=0, eaves lower), stone custom data assigned, no dirty maps added.
10. **Capture.** Run `Scripts/CaptureV5WuxianmenFullPBRAuditBefore.py` (or extend the apply script) to save the 7 standard views: hero three-quarter, front, rear, side_l, side_r, top, roof_close. Staging: one capture per invocation per the skill's deferred-screenshot warning.
11. **Archive.** Create `Docs/Reports/Change-Archive/2026-09-17-wuxianmen-fullpbr-reference-tuning.md` and `.svg` summarising the actual diffs and validation; append an entry to `.claude/memory/visual-change-archive.md`; write the validation packet to `C:/Users/mickie/.codex/visualizations/2026/09/17/<task-id>/implementation-validation-packet.md`.

## What stays unchanged

- Component count (7) and instance count (5,103).
- Mesh assets: the `Meshes/ReferenceTuned20260917/` folder already holds the seven winding-corrected meshes; the script only rebinds components, it does not regenerate geometry.
- Collision profiles, visibility, shadow flags, and the 4,775 unchanged instance transforms.
- `M_Iron_ReferenceTuned` and `M_Wood_ReferenceTuned` (revision 1 is sufficient).
- The dirty `/Game/Maps/Login` map is **never** saved; it was dirty before and stays dirty after.

## Acceptance criteria

- `Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reftune2-validation-20260917.json` records `passed: true` with every assertion green.
- The hero three-quarter capture shows: a real ridge (not a valley) at the apex of each roof tier, the main ridge running along the X axis, the plaque readable, the wall showing distinct stone blocks rather than a uniform pale field.
- The wall_close capture shows per-block stone detail and per-block tone variation.
- The roof_close capture shows one tile column across each strip and ~14 courses per tier.
- The top capture shows a single ridge line per tier, not two ridges meeting at the eaves.

## Open considerations

- The plaque board source PNG lives at `ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917/Wuxianmen_V5_FullPBR/Textures/Wuxianmen_Plaque_BaseColor_2K.png`. The apply script imports it. If the executor wants a different artistic treatment (a different brush font, weathering density, or aspect ratio), regenerate via `Scripts/GenerateWuxianmenFullPBRPlaqueTexture.py` before running the apply. The 4K_Core pass produced a similar board that lives only in the 4K_Core `.uasset`; no reusable Wuxianmen source PNG existed in `ContentSource/` before this run.
- The plaque material's UV layout matters: a `五仙門` plaque is wider than tall; the generated texture is 2048² with the calligraphy and border centered so that UV-stretch to the 4.9 m × 1.45 m plaque mesh produces a horizontal plaque outline (the near-square border stretches into a ~3.4:1 rectangle, matching the mesh footprint). If the ReferenceTuned plaque UV layout crops or offsets the texture differently, adjust `B` (border inset) in the generator or the material's UV scaling.
- The ridge reorientation values (`yaw = -90`, `yaw = -10.025`) are the *post-correction* values recorded against the reference-tuned mesh. Do not recompute them from the rotated mesh; apply them verbatim.
- The dirty `/Game/Maps/Login` map must not be saved. If the editor prompts on close, dismiss "Don't save".

## Verification commands (editor session)

```bash
# Editor build (clean before apply)
C:/Git/UE_5.5/Engine/Build/BatchFiles/Build.bat AuraEditor Win64 Development \
  C:/Git/AuraProj/Aura.uproject -waitmutex

# Audit before apply (optional; produces the baseline JSON)
python Scripts/remote_run.py Scripts/AuditV5WuxianmenFullPBRAuditBefore.py

# Apply (writes validation JSON)
python Scripts/remote_run.py Scripts/ApplyV5WuxianmenFullPBRRefTune2.py

# Capture (run one invocation per view)
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view hero
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view front
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view rear
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view side_l
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view side_r
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view top
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view roof_close
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view wall_close
python Scripts/remote_run.py Scripts/CaptureV5WuxianmenFullPBRRefTune2.py --view plaque_close
```

## Baselines to hold

- Component count = 7, instance total = 5,103 (no addition or removal).
- Per-tier tile apex at `y ~= 0`, eaves at lower `z` than the apex (no more valley).
- `HISM_004` mesh = `main_ridge`, ridge bars span `x` > `y`.
- 4,213 stone instances each carry one custom-data float.
- All four `_RefTune2` materials exist and are bound to the right components.
- `/Game/Maps/Login` is dirty before and after; never saved.

## Where the prior evidence lives

- `Saved/RawModelImport/V5/reference-tuning-baseline-20260917.json` — full Blueprint baseline (both Wuxianmen variants).
- `Saved/RawModelImport/V5/reference-tuning-source-audit-20260917.json` — per-mesh signed volume, winding, bounds.
- `Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-tuning.json` — FullPBR's revision-1 material set + 7 source meshes.
- `Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-winding-import.json` — FullPBR's winding-corrected mesh import manifest.
- `Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-fix-20260917.json` — the canonical fix spec mirrored by this doc.
- `Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-structure-probe-20260917.json` — per-part world bounds.
- `Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-instance-audit-20260917.json` — per-role instance metadata.
- `Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-texture-analysis-20260917.json` — measured tiling periods and bands.
- `Docs/Reports/Change-Archive/2026-09-17-wuxianmen-reference-tuning-revision-2.md` — the record of the same delta applied to 4K_Core.

When the live apply completes, create a matching record at
`Docs/Reports/Change-Archive/2026-09-17-wuxianmen-fullpbr-reference-tuning.md` and `.svg`,
append a row to `.claude/memory/visual-change-archive.md`, and write the
implementation-validation packet.
