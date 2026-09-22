# AuraProj — long-term project notes

## Landmark library

`Content/Assets/Environment/GuangzhouLandmarks/`. **Live list: `LANDMARKS` in
`Scripts/CreateGuangzhouLandmarkShowcase.py`; per-gate geometry in
`Saved/RawModelImport/guangzhou-landmark-showcase.json`.** Seven today: Zhengnanmen (HighFidelity +
AAA V3), Xiaobeimen AAA V3, Guidemen, Wuxianmen FullPBR, GreatNorthGate, ZhenhaiTower.

Retired 2026-09-22, whole folders deleted: `V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3`
(176 assets, incl. its own preview map), `V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core` (56), and
`BP_Guidemen_V5_4K`. Surviving Guidemen is `V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` —
it carries the 2026-09-21 window/roof and 2026-09-22 arch/door/plaque repairs, so "PreRebuild" does
**not** mean stale. Deliberately unwrapped (a third Xiaobeimen otherwise):
`Xiaobeimen/SM_Xiaobeimen`, `SM_Xiaobeimen_GeometryFixed`. ~30 older scripts still name retired
paths and will fail if run.

## Conventions

Landmark Blueprint = `PackedLevelActor` Blueprint, one `HierarchicalInstancedStaticMeshComponent`
per source mesh, source transforms as instance transforms, root a `LevelInstanceComponent`
(`Scripts/CreateGreatSouthGateActorAsset.py`, `WrapLandmarkMeshBlueprints.py`).
`BlueprintEditorLibrary.compile_blueprint` returns `None` — verify by spawning the class.

## Levels

- `Scifi_desert_city/Level/L_showcase_level` — 1786 actors; the older, parallel landmark park.
- `Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase` — the ring showcase
  (`Scripts/CreateGuangzhouLandmarkShowcase.py` builds, `ValidateGuangzhouLandmarkShowcase.py`
  checks). 7 landmarks, 8000 m ground. Each landmark owns one attached shadow-casting spot light
  (`Light_<key>`, tag `GuangzhouLandmarkLight`, all parameters ratios of its own geometry) over an
  unchanged global key/fill/sky. **Radius is derived**: `sum(2 x radius_xy + 3000) / 2pi`.

## Retiring a landmark — the order that works

1. Drop it from `LANDMARKS` and `EXPECTED_LABELS` (light labels derive from the latter).
2. Remove its actor from **every** referencing level, not just the ring — check
   `find_package_referencers_for_asset` first; `L_showcase_level` was a second, easy-to-miss owner.
3. Re-run builder, then validator → expect 0 errors / 0 warnings.
4. Delete via a `Scripts/Retire*.py` template: refuses while a referencer remains, verifies the asset
   gone rather than trusting `delete_asset`, and tests referencers *outside* the folder being deleted
   (a folder's preview map and its Blueprint reference each other). **Sweep with per-asset
   `delete_asset`, leaf-first and batched, not `delete_directory`** (trap 12), and make the script
   resumable — report what is left rather than asserting the asset still exists, so a cut-short run
   can simply be re-run.
5. Change-Archive record (AGENTS.md requires it).

**Open debt:** registry scripts still hardcode retired paths — `AuditGuangzhouLandmarks{,V2}.py`,
`ExportLandmarkMeshes.py`, `FixGuangzhouLandmarksNanite.py`, `InspectLandmarkBlueprintDetail.py`,
`SnapshotV5ReferenceTuning.py`, `ValidateGuangzhouLandmarksCrossBuilding.py`,
`ValidateV5ReferenceTuning.py`, `ProbeShowcaseLightingReference.py`.

## UE 5.5 Python traps (detail in the Change-Archive records)

1. `unreal.Rotator` positional order is **(roll, pitch, yaw)** — use keyword arguments.
2. Hidden `Collision_UCX_*` components sit **128 cm below the actor origin**: `get_actor_bounds`
   reports grounded actors as sunk ~1 m. Measure rendered geometry, skip `visible == False`.
3. `HierarchicalInstancedStaticMeshComponent` derives from `StaticMeshComponent` — querying both
   double-counts every HISM.
4. `relative_transform` is unreliable on the V3 Blueprints (swaps local Z and Y ranges); use
   `relative_location` — those components carry translation only.
5. `HitResult` fields are protected: read `hit.to_tuple()` (4 = location, 8 = PhysicalMaterial,
   9 = hit_actor, 10 = hit_component). `line_trace_single` returns **None** on a miss.
6. `export_render_target` writes nothing: use `SceneCapture2D` → persistent `TextureRenderTarget2D`
   → `RenderingLibrary.read_render_target` → PPM P6 → PNG outside the editor.
7. One `capture_scene()` never converges auto-exposure: pin it with an unbound `PostProcessVolume`
   (both `auto_exposure_*_brightness` = one EV100), but only while the capture keeps default flags
   (trap 9). `get_editor_property("settings")` returns a struct copy — write it back.
8. `set_actor_hidden_in_game` does **not** disable a light. To A/B a light, zero its intensity and
   restore it, without saving.
9. `capture_every_frame = False` stops a SceneCapture2D honouring post-process, voiding the exposure
   pin. `b_always_persist_rendering_state` / `b_capture_on_construction` **do not exist** (real name
   `always_persist_rendering_state`); `try/except` hid it. Unfixed in `CaptureDadongmen*.py`,
   `CaptureZhengnanmenManual4K.py`.
10. Judge showcase frames for **relative** differences only: the blue lower half of an eye-level
    frame **is** the ground plane (traces hit `Showcase_Ground` at 15-90 m); low-EV cyan is the red
    channel clipping first. EV100 4.0 gives 30% mid-tones.
11. `unreal.get_editor_subsystem` rejects non-subsystem classes: `EditorLevelLibrary` is gone, use
    `LevelEditorSubsystem` / `EditorActorSubsystem` / `UnrealEditorSubsystem`.
12. **`EditorAssetLibrary.delete_directory` can WEDGE the editor.** On a 176-asset folder it froze
    the game thread inside `Force Deleting N Package(s)`: **0 CPU over 15 s**, `Responding` False,
    and **no modal dialog** (main window visible *and* enabled) — a real block, not slow work.
    Delete one asset at a time with `delete_asset`, leaf-first (textures → materials → meshes),
    batched, logging each path *before* deleting it. It returned True on a 56-asset folder once, so
    this is size-dependent rather than always broken. See `Scripts/FinishRetireXiaobeimenProductionV3.py`.
13. `find_package_referencers_for_asset(..., load_assets_to_confirm=False)` gates deletion but its
    edge list is partly **inverted** — treat it as candidate discovery (it is what revealed the
    second showcase level) and confirm by loading the level and counting actors.

## Shared editor

One live Unreal Editor, shared by all agents via UE Python Remote Execution
(`Scripts/remote_run.py`, `UE_ENGINE_ROOT` = `C:/Git/UnrealEngine-5.5`). Serialize heavy passes. A
resident `codex.exe` is normal — judge activity from artifact mtimes. Before switching levels
confirm `get_dirty_content_packages()` and `get_dirty_map_packages()` are both empty.
Remote-exec output arrives as a Python list repr with escaped `\r\n` — capture to a file and split on
`{'type': 'Info', 'output': '`; a console tail truncates it mid-record.

## Git

`.claude/memory/visual-change-archive.md` is **append-only** — one line per job, never edited or
deleted. Its conflicts are always append-vs-append at the same tail offset: resolve as the **union of
both sides**, never picking one. Prove it by `git show :1:`/`:2:`/`:3:` into temp files and diffing
each against the base — both showing **0 removed lines** makes the union safe, then verify the result
is a strict superset of base, ours and theirs. Same for `Docs/Reports/Change-Archive/`.

- `git show <stage>:<path>` redirected to a file **writes CRLF** (`core.autocrlf=true` +
  `.gitattributes` `* text=auto`) — fix with `sed -i 's/\r$//'`.
- `grep -c $'\r'` is unreliable; count bytes: `tr -cd '\r' < file | wc -c`.
- `git diff --no-index` needs real paths; `grep -c` exits 1 on zero matches, breaking `&&` chains.

`*.uasset` / `*.umap` use **Git LFS**; `post-commit` / `post-merge` hooks can be cut off by a shell
timeout *after* the commit succeeded — verify by inspecting the commit, not the exit status.
