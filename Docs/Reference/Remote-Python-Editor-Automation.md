# Remote Python Editor Automation (UE Remote Execution)

> Runbook for driving the Unreal editor from a script via the PythonScriptPlugin
> Remote Execution protocol. Written 2026-08-07 after adding a NavMesh to
> L_showcase_level entirely from the command line (no editor clicks).
>
> **Why this exists:** a monster spawned via the `AddMonster` cheat in
> L_showcase_level never moved. Root cause: the level had **no NavMesh**, so both
> the combat BT's EQS→MoveTo chain and the BehaviorU wander (`MoveToLocation` with
> `bUsePathfinding=true`) silently failed. The fix had two parts:
> 1. **Runtime fallback** (already in code): `Method_MoveToWanderTarget()` in
>    [AuraBehaviorUAgentComponent.cpp](../../Source/Aura/Private/AI/AuraBehaviorUAgentComponent.cpp)
>    now detects a missing navmesh and falls back to a direct (non-pathfinding) move.
> 2. **Level fix** (this runbook): add a NavMeshBoundsVolume + build the navmesh in
>    the level, then recook for the dedicated server.

## When to use this

- A level has no NavMesh and AI (combat BT or BehaviorU wander) silently fails to move.
- Any editor asset/level operation that would otherwise require clicking menus:
  spawn actors, set properties, save levels, trigger builds.

## Prerequisites

- **Editor running with Remote Execution enabled.** The PythonScriptPlugin's
  remote execution is on by default in editor builds. The editor must be *running*
  — the client discovers it over UDP multicast, so there is no way to start it.
- **The project's client wrapper:** `Scripts/remote_run.py` (wraps the engine's
  official `remote_execution.py` client). Use it — do not hand-roll a client.
- **The reusable navmesh script:** `Scripts/add_navmesh_bounds_volume.py`.

## The workflow

### 1. Launch the editor

```bash
"C:/Git/UnrealEngine-5.5/Engine/Binaries/Win64/UnrealEditor.exe" \
  "C:/Git/AuraProj/Aura.uproject" \
  "/Game/Scifi_desert_city/Level/L_showcase_level"
```

Run it in the background (it stays open). **Note:** the map argument may be
ignored — the editor often opens its startup map (Login) instead. The navmesh
script loads the target level itself, so this is fine.

### 2. Run the navmesh script

```bash
python Scripts/remote_run.py Scripts/add_navmesh_bounds_volume.py
```

The script (see [Reference section](#the-add-navmesh-script) below):
1. Loads the target level (edit `TARGET_LEVEL` at the top for another map).
2. Removes any existing `NavMeshBoundsVolume` actors.
3. Computes the level's actor bounds (skipping sky domes / giant ground planes).
4. Spawns a `NavMeshBoundsVolume` scaled to cover the bounds.
5. Saves the level.
6. Triggers `BuildPaths` (the nav build; ~40s on L_showcase_level).

### 3. Verify the navmesh

Run a small probe script the same way:

```python
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
nav = None
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_class().get_name() == "RecastNavMesh":
        nav = a
        break
print("RecastNavMesh:", nav.get_name() if nav else None)
nav_sys = unreal.NavigationSystemV1.get_navigation_system(world)
p = nav_sys.find_path_to_location_synchronously(
    world, unreal.Vector(0, 0, 200), unreal.Vector(1000, 0, 200), None)
print("Path found 0,0 -> 1000,0:", p is not None)
```

Then **re-save the level** — the `RecastNavMesh` actor is created *during* the
build, after the script's save, so it must be saved again or it won't persist:

```python
import unreal
unreal.EditorLevelLibrary.save_current_level()
print("Level re-saved")
```

### 4. Recook for the dedicated server

The packaged server loads from `Saved/Cooked/WindowsServer/` — the level's
navmesh data only reaches it via a cook. **In Git Bash, prefix
`MSYS_NO_PATHCONV=1`** or the `-map=/Game/...` argument gets mangled into
`C:/Program Files/Git/Game/...` and the cook silently cooks nothing.

```bash
MSYS_NO_PATHCONV=1 "C:/Git/UnrealEngine-5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Git/AuraProj/Aura.uproject" \
  -run=cook -targetplatform=WindowsServer \
  -map=/Game/Scifi_desert_city/Level/L_showcase_level \
  -iterate -unattended -nop4
```

Expect `Success - 0 error(s), 0 warning(s)` (~45s). The cooked `.uexp` grows
noticeably (L_showcase_level: 133MB with navmesh data).

### 5. Restart the server and test

Restart the dedicated server (it loads the freshly cooked level) and spawn a
monster via the `AddMonster` cheat. It should now move. Check the server log
(`Saved/Cooked/WindowsServer/Aura/Saved/Logs/GameServerManager`) for EQS/MoveTo
activity instead of silent failure.

## Gotchas (hard-won details)

| Symptom | Cause | Fix |
|---|---|---|
| Client gets no pong / "no editor found" | Editor not running | Launch the editor first; remote execution can't start it |
| Client misses pongs on Windows | Multicast joined on a non-loopback interface; the editor binds `127.0.0.1:6766` and joins the group on loopback | Bind the client socket to `127.0.0.1` and join the group with `struct.pack("4s4s", inet_aton(GROUP), inet_aton("127.0.0.1"))` — `remote_run.py` already does this |
| `SyntaxError: multiple statements found while compiling a single statement` | `exec_mode: "ExecuteStatement"` allows one statement only | Use `exec_mode: "ExecuteFile"` for multi-line scripts — `remote_run.py` already does this |
| `World` has no `get_current_level` | Not exposed in the Python API | Use `EditorLevelLibrary.save_current_level()` / `get_all_level_actors()` |
| `EditorLevelLibrary.get_level_actors_bounds` doesn't exist | Not exposed | Compute bounds per actor via `a.get_actor_bounds(False, True)` |
| `NavigationSystemV1.build` not exposed | Not exposed | `unreal.SystemLibrary.execute_console_command(world, "BuildPaths")` |
| Spawned volume is 100×100×100 | `spawn_actor_from_class` creates the default brush geometry | Scale by `desired_extent / 100` |
| Cook "Success" but nothing cooked | Git Bash MSYS path conversion mangled `-map=/Game/...` | Prefix `MSYS_NO_PATHCONV=1` |
| `maxTiles` overflow warning (768800 tiles vs 16-bit serialized max) | RecastNavMesh tile-count exceeds the serialized bit width | **Benign on the dedicated server**: `IsGameStaticNavMesh` (RecastNavMeshGenerator.cpp:4918) = game world + Static mode → saved data loads as-is; the recreate only happens in the editor |
| RecastNavMesh actor missing after reload | It's created during the build, *after* the script's save | Re-save the level after `BuildPaths` completes |

## The add-navmesh script

`Scripts/add_navmesh_bounds_volume.py` — full source:

```python
import unreal

TARGET_LEVEL = "/Game/Scifi_desert_city/Level/L_showcase_level"

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world is None:
    raise RuntimeError("No editor world is loaded")

# Load the target level if the editor opened something else (e.g. its startup map).
if world.get_name() != TARGET_LEVEL.rsplit("/", 1)[-1]:
    loaded = unreal.EditorLevelLibrary.load_level(TARGET_LEVEL)
    if not loaded:
        raise RuntimeError("Failed to load %s" % TARGET_LEVEL)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
level_name = world.get_name()
print("Level:", level_name)

# 1. Remove any existing NavMeshBoundsVolume actors (stale / probe leftovers).
removed = 0
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_class().get_name() == "NavMeshBoundsVolume":
        unreal.EditorLevelLibrary.destroy_actor(a)
        removed += 1
print("Removed existing NavMeshBoundsVolume actors:", removed)

# 2. Compute the level's actor bounds. Skip actors with absurd extents (sky
#    domes, giant ground planes) and zero-size actors so the volume hugs the
#    playable geometry instead of the whole world.
actors = unreal.EditorLevelLibrary.get_all_level_actors()
min_x = min_y = min_z = float("inf")
max_x = max_y = max_z = float("-inf")
count = 0
for a in actors:
    origin, extent = a.get_actor_bounds(False, True)
    if extent.x > 100000 or extent.y > 100000 or extent.z > 100000:
        continue
    if extent.x < 0.01 and extent.y < 0.01 and extent.z < 0.01:
        continue
    min_x = min(min_x, origin.x - extent.x)
    max_x = max(max_x, origin.x + extent.x)
    min_y = min(min_y, origin.y - extent.y)
    max_y = max(max_y, origin.y + extent.y)
    min_z = min(min_z, origin.z - extent.z)
    max_z = max(max_z, origin.z + extent.z)
    count += 1

if count == 0:
    center = unreal.Vector(0, 0, 0)
    extent = unreal.Vector(5000, 5000, 1000)
else:
    center = unreal.Vector((min_x + max_x) / 2, (min_y + max_y) / 2, (min_z + max_z) / 2)
    extent = unreal.Vector((max_x - min_x) / 2 + 500, (max_y - min_y) / 2 + 500, (max_z - min_z) / 2 + 200)
print("Bounds from %d actors -> center=%s extent=%s" % (count, center, extent))

# 3. Spawn the NavMeshBoundsVolume. spawn_actor_from_class creates the default
#    100x100x100 brush geometry, so scaling by extent/100 covers the bounds.
vol = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, center, unreal.Rotator(0, 0, 0))
if vol is None:
    raise RuntimeError("Failed to spawn NavMeshBoundsVolume")
print("Spawned:", vol.get_name())

scale = unreal.Vector(extent.x / 100.0, extent.y / 100.0, extent.z / 100.0)
vol.set_actor_scale3d(scale)
print("Scale:", scale)

# 4. Save the level (the volume persists; the RecastNavMesh actor created by
#    the build below must be saved AGAIN after the build finishes).
saved = unreal.EditorLevelLibrary.save_current_level()
print("Level saved:", saved)

# 5. Trigger a navigation build (editor console command bound to Ctrl+Shift+B).
unreal.SystemLibrary.execute_console_command(world, "BuildPaths")
print("BuildPaths command issued")

print("DONE: level=%s center=%s extent=%s saved=%s" % (level_name, center, extent, saved))
```

## Related

- [AuraBehaviorUAgentComponent.cpp](../../Source/Aura/Private/AI/AuraBehaviorUAgentComponent.cpp) — the runtime navmesh fallback.
- [AuraEditorModule.cpp](../../Source/AuraEditor/Private/AuraEditorModule.cpp) — the "Level Tools" menu entry that does the same thing from the editor UI.
- **Log locations:** client in `Saved/Logs`, server in `Saved/Cooked/WindowsServer/Aura/Saved/Logs/GameServerManager` (newest file first in each).
