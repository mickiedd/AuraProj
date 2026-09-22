# AuraProj — long-term project notes

## Landmark library

`Content/Assets/Environment/GuangzhouLandmarks/` holds the Guangzhou landmark library.
**Nine placeables**, each an independent Blueprint Actor Asset:

| Gate | Blueprint |
| --- | --- |
| Zhengnanmen (Great South Gate) | `GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset` (114 comp / 12,383 inst) and `V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3` (25) |
| Xiaobeimen (Small North Gate) | `V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3` (51) and `V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3` (16) |
| Guidemen (Guide Gate) | `V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` (47 / 18,822) — the reference-repaired model, see below |
| Wuxianmen (Five Immortals Gate) | `V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core` and `V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR` (7 / 5,103 each) |
| GreatNorthGate (Dabeimen) | `GreatNorthGate/BP_GreatNorthGate` (1 / 1) — wrapped 2026-09-21 |
| Zhenhai Tower | `ZhenhaiTower/BP_ZhenhaiTower` (1 / 1) — wrapped 2026-09-21 |

Not placeable landmarks (deliberately left unwrapped): `Xiaobeimen/SM_Xiaobeimen` and
`SM_Xiaobeimen_GeometryFixed` — superseded raw meshes; wrapping them would be a third
Xiaobeimen.

**Guidemen naming — read this before trusting a name.** `BP_Guidemen_V5_4K` was the
original import and was **deleted on 2026-09-22**; it never received the reference
repairs. `BP_Guidemen_V5_4K_PreRebuild_20260918` sounds like a stale backup but is the
**good, repaired model**: the 2026-09-21 window/roof pass and the 2026-09-22
arch/door/plaque follow-up were both applied to it *by that name*. It is what the showcase
places, and it is the one to tune. (An earlier version of this file said "recovery copy,
never place it" — that was wrong.) The repaired model measures the same 3008.32 cm
bounding circle and 1967.5 cm height as the deleted one, so swapping between them does not
move the ring. Fifteen older scripts still name the deleted path and will fail if run.

## The "independent Blueprint" convention

An independent landmark Blueprint is a **`PackedLevelActor` Blueprint** holding one
`HierarchicalInstancedStaticMeshComponent` per source mesh, with source transforms
recorded as instance transforms. Root component is a `LevelInstanceComponent`. Reference
implementation: `Scripts/CreateGreatSouthGateActorAsset.py`; reusable wrapper:
`Scripts/WrapLandmarkMeshBlueprints.py`.

`BlueprintEditorLibrary.compile_blueprint` returns `None` in this build — verify by
spawning the generated class, not by asserting its return value.

## Levels

- `Content/Scifi_desert_city/Level/L_showcase_level` — the gameplay/showcase level the
  earlier landmark placement work targeted.
- `Content/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase` — the
  landmark showcase level created 2026-09-21: all nine placeables in a ring of radius
  10612.11 cm around a central plaza, each facing the centre, on an 8000 m ground plane
  (raised from 420 m on 2026-09-22: at 420 m the plane's edge was visible from eye level).
  Built and re-buildable by `Scripts/CreateGuangzhouLandmarkShowcase.py`; validated by
  `Scripts/ValidateGuangzhouLandmarkShowcase.py`. Since 2026-09-22 each landmark also
  owns one shadow-casting spot light (`Light_<key>`, tag `GuangzhouLandmarkLight`,
  attached to the landmark, all parameters ratios of that building's own geometry) as
  an accent on top of the unchanged global key/fill/sky. 34 actors total. The Guidemen
  slot key is `Guidemen_ReferenceRepaired`, so its actor is
  `Landmark_Guidemen_ReferenceRepaired` and its light `Light_Guidemen_ReferenceRepaired` —
  the old `Guidemen_V5_4K` key went out with the deleted asset.

## UE 5.5 Python traps (all verified in this project)

1. `unreal.Rotator` positional order is **`(roll, pitch, yaw)`**, not `(pitch, yaw, roll)`.
   `Rotator(0.0, yaw, 0.0)` sets **pitch**. Always use keyword arguments.
2. Hidden `Collision_UCX_*` components sit exactly **128 cm below the actor origin** on
   every landmark, so `get_actor_bounds` reports correctly grounded actors as sunk ~1 m.
   Measure rendered geometry (skip `visible == False` components) instead.
3. `HierarchicalInstancedStaticMeshComponent` derives from `StaticMeshComponent`, so
   querying both classes double-counts every HISM.
4. `relative_transform` is unreliable on the V3 Blueprints (returns a rotation that swaps
   the local Z and Y ranges). Use `relative_location` — those components have no relative
   rotation, only translation.
5. `HitResult` fields are protected; read `hit.to_tuple()` and index instead. Indices:
   4 = location, 8 = **PhysicalMaterial**, 9 = hit_actor, 10 = hit_component. Reading 8 as
   the actor fails with `'PhysicalMaterial' object has no attribute 'get_actor_label'`.
   `SystemLibrary.line_trace_single` returns **None** (not an empty HitResult) on a miss —
   which is a usable answer: nothing is there.
6. `export_render_target` writes nothing. The working capture path is
   `SceneCapture2D → persistent TextureRenderTarget2D → RenderingLibrary.read_render_target`
   (returns normalised 0..1) → PPM P6 → PNG converted outside the editor (no PIL in-editor).
7. **Exposure on the capture path is the real trap, not the level.** A single
   `capture_scene()` does not let auto-exposure converge. Pinning exposure with an
   unbound `PostProcessVolume` (override `auto_exposure_min_brightness` and
   `auto_exposure_max_brightness` to the same EV100) DOES work — the 2026-09-21 bracket
   proved it, means 215/200/85/25 — but **only while the capture keeps its default
   flags**; see trap 9. `get_editor_property("settings")` returns a struct copy — write
   it back. Always self-check that two exposures separate before trusting a frame.
8. **`set_actor_hidden_in_game` does not disable a light.** A "hide the lights to test
   whether the scene is lit" experiment returns an identical image either way and proves
   nothing. This cost a whole diagnosis on 2026-09-21. To A/B a light, set its intensity
   to 0 and restore it, without saving.
9. **`capture_every_frame = False` stops a SceneCapture2D honouring post-process**, so the
   exposure pin silently does nothing: EV100 3.0/4.0/5.0/6.0 give statistically identical
   frames. Two of the three flags the capture scripts set —
   `b_always_persist_rendering_state`, `b_capture_on_construction` — **do not exist** on
   this build's component (real name `always_persist_rendering_state`, no `b_`), and
   `try/except` swallowed that. Measured: default flags separate EV100 3.0/7.0 by 60×;
   `always_persist_rendering_state=False` separates them by nothing. Still unfixed in
   `CaptureDadongmen*.py`, `CaptureZhengnanmenManual4K.py`.
10. **Judge showcase frames for RELATIVE differences only.** The bright blue lower half of
   an eye-level frame **is** the ground plane — traces hit `Showcase_Ground` at 15-90 m —
   dark paving lit cool by the sky light; the cyan at low EV is the red channel clipping
   first, not a different surface. With the harness fixed, EV100 4.0 gives 30% mid-tones
   and a legible scene, so an earlier "no exposure yields mid-tones" reading was a harness
   artefact, not the level. One real finding, since fixed: the ground plane ended 160 m
   from the plaza, so a band of the frame was SkyAtmosphere below the horizon —
   `GROUND_SCALE` is now 8000 m (half extent 4000 m), which pushes the edge out of frame.

## Shared editor

One live Unreal Editor is shared by all agents through UE Python Remote Execution
(`Scripts/remote_run.py`, `UE_ENGINE_ROOT` defaults to `C:/Git/UnrealEngine-5.5`).
Serialize heavy passes. A resident `codex.exe` is normal — judge activity from recent
session/artifact mtimes, not process presence. Before switching levels, confirm
`get_dirty_content_packages()` and `get_dirty_map_packages()` are both empty.
