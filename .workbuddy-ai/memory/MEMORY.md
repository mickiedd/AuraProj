# AuraProj — long-term project notes

## Landmark library

`Content/Assets/Environment/GuangzhouLandmarks/` holds the Guangzhou landmark library.
**Nine placeables**, each an independent Blueprint Actor Asset:

| Gate | Blueprint |
| --- | --- |
| Zhengnanmen (Great South Gate) | `GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset` (114 comp / 12,383 inst) and `V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3` (25) |
| Xiaobeimen (Small North Gate) | `V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3` (51) and `V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3` (16) |
| Guidemen (Guide Gate) | `V5/Guidemen_4K/BP_Guidemen_V5_4K` (47 / 18,822) |
| Wuxianmen (Five Immortals Gate) | `V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core` and `V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR` (7 / 5,103 each) |
| GreatNorthGate (Dabeimen) | `GreatNorthGate/BP_GreatNorthGate` (1 / 1) — wrapped 2026-09-21 |
| Zhenhai Tower | `ZhenhaiTower/BP_ZhenhaiTower` (1 / 1) — wrapped 2026-09-21 |

Not placeable landmarks (deliberately left unwrapped): `Xiaobeimen/SM_Xiaobeimen` and
`SM_Xiaobeimen_GeometryFixed` — superseded raw meshes; wrapping them would be a third
Xiaobeimen. `V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` is a recovery copy,
never place it.

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
  10612.12 cm around a central plaza, each facing the centre, on a 420 m ground plane.
  Built and re-buildable by `Scripts/CreateGuangzhouLandmarkShowcase.py`; validated by
  `Scripts/ValidateGuangzhouLandmarkShowcase.py`.

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
5. `HitResult` fields are protected; read `hit.to_tuple()` and index instead.
6. `export_render_target` writes nothing. The working capture path is
   `SceneCapture2D → persistent TextureRenderTarget2D → RenderingLibrary.read_render_target`
   (returns normalised 0..1) → PPM P6 → PNG converted outside the editor (no PIL in-editor).
7. **A single `capture_scene()` does not let auto-exposure converge**, so captures are
   taken far too bright and look "unlit" — bright surfaces clip to white, dark to black,
   bimodal histogram. Pin exposure with an unbound `PostProcessVolume` (override
   `auto_exposure_min_brightness` and `auto_exposure_max_brightness` to the same EV100;
   3.0 is right for the showcase level, not the preview levels' 1.0) and bracket to
   choose. `get_editor_property("settings")` returns a struct copy — write it back.
8. **`set_actor_hidden_in_game` does not disable a light.** A "hide the lights to test
   whether the scene is lit" experiment returns an identical image either way and proves
   nothing. This cost a whole diagnosis on 2026-09-21.

## Shared editor

One live Unreal Editor is shared by all agents through UE Python Remote Execution
(`Scripts/remote_run.py`, `UE_ENGINE_ROOT` defaults to `C:/Git/UnrealEngine-5.5`).
Serialize heavy passes. A resident `codex.exe` is normal — judge activity from recent
session/artifact mtimes, not process presence. Before switching levels, confirm
`get_dirty_content_packages()` and `get_dirty_map_packages()` are both empty.
