# add_navmesh_bounds_volume.py — run inside the editor via remote_run.py.
#
#   python Scripts/remote_run.py Scripts/add_navmesh_bounds_volume.py
#
# Spawns a NavMeshBoundsVolume covering the level's actor bounds, saves the
# level, and triggers a navigation build (BuildPaths). Fixes AI pathfinding in
# levels that have no NavMesh (e.g. L_showcase_level had none, so the combat
# BT's EQS->MoveTo chain and the BehaviorU wander both silently failed).
#
# Edit TARGET_LEVEL for a different map. The editor may open its startup map
# instead of the map passed on the command line, so this script loads the
# target level itself.

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
