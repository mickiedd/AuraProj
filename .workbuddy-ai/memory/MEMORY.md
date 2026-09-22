# AuraProj — long-term project notes

## Landmark library

`Content/Assets/Environment/GuangzhouLandmarks/`. **Eight placeables** (nine until
2026-09-22), each an independent Blueprint Actor Asset. Counts are components / instances;
`r_xy` and height are what the showcase ring measured, in cm.

| Gate | Blueprint | comp / inst | r_xy / height |
| --- | --- | --- | --- |
| Zhengnanmen (Great South Gate) | `GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset` | 114 / 12,383 | 1595.31 / 2302.50 |
| Zhengnanmen — AAA V3 | `V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3` | 19 / 19 | 1504.16 / 2250.00 |
| Xiaobeimen (Small North Gate) — AAA V3 | `V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3` | 46 / 46 | 2704.24 / 2253.08 |
| Xiaobeimen — Production V3 | `V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3` | 10 / 10 | 2718.00 / 2215.00 |
| Guidemen (Guide Gate) | `V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` | 47 / 18,822 | 3008.32 / 1967.50 |
| Wuxianmen (Five Immortals Gate) | `V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR` | 11 / 4,308 | 1864.67 / 1721.00 |
| GreatNorthGate (Dabeimen) | `GreatNorthGate/BP_GreatNorthGate` | 1 / 1 | 2120.00 / 2476.02 |
| Zhenhai Tower | `ZhenhaiTower/BP_ZhenhaiTower` | 1 / 1 | 2459.54 / 2085.00 |

Not placeable, deliberately left unwrapped: `Xiaobeimen/SM_Xiaobeimen` and
`SM_Xiaobeimen_GeometryFixed` — superseded raw meshes; wrapping them would be a third
Xiaobeimen.

**Two variants were retired; ~30 older scripts still name their paths and will fail if run.**

- `V5/Guidemen_4K/BP_Guidemen_V5_4K` — deleted 2026-09-22, never received the reference
  repairs. The good Guidemen is `BP_Guidemen_V5_4K_PreRebuild_20260918`: the 2026-09-21
  window/roof pass and the 2026-09-22 arch/door/plaque follow-up were both applied to it *by
  that name*. "PreRebuild" is a leftover from when it was only a backup, so it does **not**
  mean stale. Same 3008.32 cm circle and 1967.5 cm height as the deleted one, so swapping
  between them does not move the ring.
- `V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core` — deleted 2026-09-22 with its whole folder
  (56 assets: the Blueprint, `L_Wuxianmen_V5_4K_Core_Preview`, and its own
  Materials/Meshes/Textures). Wuxianmen is now FullPBR only; FullPBR carries the 2026-09-22
  reference repair in `V5/Wuxianmen_FullPBR/ReferenceRepair20260922/`.

## The "independent Blueprint" convention

A landmark Blueprint is a `PackedLevelActor` Blueprint holding one
`HierarchicalInstancedStaticMeshComponent` per source mesh, source transforms recorded as
instance transforms; root component is a `LevelInstanceComponent`. Reference:
`Scripts/CreateGreatSouthGateActorAsset.py`. Reusable wrapper:
`Scripts/WrapLandmarkMeshBlueprints.py`.

`BlueprintEditorLibrary.compile_blueprint` returns `None` in this build — verify by spawning
the generated class, not by asserting its return value.

## Levels

- `Content/Scifi_desert_city/Level/L_showcase_level` — gameplay/showcase level the earlier
  landmark placement work targeted.
- `Content/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase` — the landmark
  showcase. Built and re-buildable by `Scripts/CreateGuangzhouLandmarkShowcase.py`, validated
  by `Scripts/ValidateGuangzhouLandmarkShowcase.py`. Since 2026-09-22: **8 landmarks** in a
  ring of radius **9541.1 cm** around a central plaza, each facing the centre, on an 8000 m
  ground plane (raised from 420 m so the plane's edge leaves the frame), **31 actors**. Each
  landmark owns one shadow-casting spot light (`Light_<key>`, tag `GuangzhouLandmarkLight`,
  attached to the landmark, every parameter a ratio of that building's own geometry) as an
  accent on top of the unchanged global key/fill/sky. **The radius is derived, not fixed**:
  `sum(2 x radius_xy + 3000) / 2pi` over the placed set, so adding or removing a landmark
  re-spreads every slot. Slot keys: Zhengnanmen_HighFidelity, Zhengnanmen_AAA_V3,
  Xiaobeimen_AAA_V3, Xiaobeimen_Production_V3, Guidemen_ReferenceRepaired,
  Wuxianmen_V5_FullPBR, GreatNorthGate, ZhenhaiTower.

## Retiring a landmark asset — the order that works

1. Drop the entry from `LANDMARKS` in the builder and from `EXPECTED_LABELS` in the validator
   (expected light labels derive from that list, so the light check follows for free).
2. Re-run the builder — it clears and rebuilds, so the level stops referencing the asset.
3. Re-run the validator; expect 0 errors / 0 warnings.
4. Then delete. Templates: `Scripts/RetireGuidemenV5_4K.py`,
   `Scripts/RetireWuxianmenV5_4K_Core.py`. Both refuse to delete while a referencer remains
   and verify the asset gone rather than trusting `delete_asset`'s return value. The
   Wuxianmen one tests for referencers *outside the folder being deleted*, not "no
   referencers at all", because a folder's preview map and its Blueprint reference each other.
5. Write the Change-Archive record — AGENTS.md requires it before the job counts as done.

**Open debt:** the landmark-registry scripts still hardcode the retired paths and will skip or
fail — `AuditGuangzhouLandmarks.py`, `AuditGuangzhouLandmarksV2.py`,
`ExportLandmarkMeshes.py`, `FixGuangzhouLandmarksNanite.py`,
`InspectLandmarkBlueprintDetail.py`, `SnapshotV5ReferenceTuning.py`,
`ValidateGuangzhouLandmarksCrossBuilding.py`, `ValidateV5ReferenceTuning.py` (also pins
per-asset expected dimensions), `ProbeShowcaseLightingReference.py`.

## UE 5.5 Python traps (all verified in this project)

1. `unreal.Rotator` positional order is **(roll, pitch, yaw)** — `Rotator(0.0, yaw, 0.0)` sets
   pitch and tips the landmark over. Always use keyword arguments.
2. Hidden `Collision_UCX_*` components sit exactly **128 cm below the actor origin** on every
   landmark, so `get_actor_bounds` reports correctly grounded actors as sunk ~1 m. Measure
   rendered geometry (skip `visible == False` components) instead.
3. `HierarchicalInstancedStaticMeshComponent` derives from `StaticMeshComponent`, so querying
   both classes double-counts every HISM.
4. `relative_transform` is unreliable on the V3 Blueprints (returns a rotation that swaps the
   local Z and Y ranges). Use `relative_location` — those components have no relative
   rotation, only translation.
5. `HitResult` fields are protected; read `hit.to_tuple()` and index: 4 = location,
   8 = **PhysicalMaterial**, 9 = hit_actor, 10 = hit_component. Reading 8 as the actor fails
   with `'PhysicalMaterial' object has no attribute 'get_actor_label'`.
   `SystemLibrary.line_trace_single` returns **None** (not an empty HitResult) on a miss —
   which is a usable answer: nothing is there.
6. `export_render_target` writes nothing. The working capture path is `SceneCapture2D` →
   persistent `TextureRenderTarget2D` → `RenderingLibrary.read_render_target` (normalised
   0..1) → PPM P6 → PNG converted outside the editor (no PIL in-editor).
7. **Exposure is the trap, not the level.** One `capture_scene()` does not let auto-exposure
   converge. Pinning it with an unbound `PostProcessVolume` (override
   `auto_exposure_min_brightness` and `auto_exposure_max_brightness` to the same EV100) does
   work — but only while the capture keeps its default flags (trap 9).
   `get_editor_property("settings")` returns a struct copy — write it back. Self-check that
   two exposures separate before trusting a frame.
8. **`set_actor_hidden_in_game` does not disable a light.** A "hide the lights to test whether
   the scene is lit" experiment returns an identical image either way and proves nothing. To
   A/B a light, set its intensity to 0 and restore it, without saving.
9. **`capture_every_frame = False` stops a SceneCapture2D honouring post-process**, so the
   exposure pin silently does nothing (EV100 3.0/4.0/5.0/6.0 give identical frames). Two flags
   the capture scripts set — `b_always_persist_rendering_state` and
   `b_capture_on_construction` — **do not exist** on this build's component (real name
   `always_persist_rendering_state`, no `b_`), and `try/except` swallowed that. Measured:
   default flags separate EV100 3.0/7.0 by 60x; `always_persist_rendering_state=False`
   separates them by nothing. Still unfixed in `CaptureDadongmen*.py`,
   `CaptureZhengnanmenManual4K.py`.
10. **Judge showcase frames for RELATIVE differences only.** The bright blue lower half of an
    eye-level frame **is** the ground plane (traces hit `Showcase_Ground` at 15-90 m) — dark
    paving lit cool by the sky light; the cyan at low EV is the red channel clipping first, not
    a different surface. EV100 4.0 gives 30% mid-tones and a legible scene.
11. `find_package_referencers_for_asset(path, load_assets_to_confirm=False)` is the gate before
    deleting an asset, but it does not reliably report a preview map's reference to its own
    Blueprint — scope the test to referencers *outside* the folder being removed.

## Shared editor

One live Unreal Editor is shared by all agents through UE Python Remote Execution
(`Scripts/remote_run.py`, `UE_ENGINE_ROOT` defaults to `C:/Git/UnrealEngine-5.5`). Serialize
heavy passes. A resident `codex.exe` is normal — judge activity from recent session/artifact
mtimes, not process presence. Before switching levels, confirm `get_dirty_content_packages()`
and `get_dirty_map_packages()` are both empty.

## Git conventions and traps

`.claude/memory/visual-change-archive.md` is an **append-only index** — every completed job
adds one line at the tail and nobody edits or deletes an existing line. So a conflict on it is
always append-vs-append at the same tail offset, and the correct resolution is the **union of
both sides**, never picking one. Prove it first: `git show :1:` (base), `:2:` (ours), `:3:`
(theirs) into temp files and diff each against the base — if both show **0 removed lines**,
union is provably safe. Then verify the result is a strict superset of base, ours *and*
theirs. Resolve `Docs/Reports/Change-Archive/` the same way; its files are one-per-job and
never collide.

Traps hit while resolving the 2026-09-22 merge:

- Redirecting `git show <stage>:<path>` into a file **writes CRLF** (`core.autocrlf=true` plus
  `.gitattributes` `* text=auto`), silently converting the whole file. Normalize back with
  `sed -i 's/\r$//'` before staging.
- `grep -c $'\r'` is unreliable in this Git Bash for counting CRs — it reported 400 CRLF lines
  on a file with 0 CR bytes. Count bytes: `tr -cd '\r' < file | wc -c`.
- `git diff --no-index` needs real file paths; process substitution (`<(git show ...)`) fails
  with "Could not access '/dev/fd/63'". `grep -c` exits 1 on zero matches, breaking `&&`
  chains — use `;` or a helper function.

`.gitattributes` routes `*.uasset` and `*.umap` through **Git LFS** (3721 tracked files).
Check `git diff --cached --name-only` for those extensions before committing a merge, and note
the `post-commit`/`post-merge` git-lfs hooks can be cut off by a shell timeout *after* the
commit has already succeeded — verify by inspecting the commit, not the exit status.
