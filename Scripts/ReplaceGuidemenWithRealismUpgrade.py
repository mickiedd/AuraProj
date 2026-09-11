"""Replace the single Guidemen actor with the aligned realism-upgrade pieces."""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/RealismUpgrade"
OLD_LABEL = "GuangzhouLandmark_Guidemen"
ACTOR_PREFIX = "GuangzhouLandmark_Guidemen__"
ACTOR_TAG = "ImportedGuidemenBuilding"
UPGRADE_TAG = "GuidemenRealismUpgrade"
PARK_TAG = "GuangzhouLandmarkPark"
PLACEMENT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/guidemen-realism-placement.json"

assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT_ROOT.resolve(), "Wrong Unreal project"


def actor_bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z)


def overlaps(left, right, padding=1.0):
    return (left[0] - padding < right[2] and left[2] + padding > right[0] and
            left[1] - padding < right[3] and left[3] + padding > right[1])


def editor_world():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.is_in_play_in_editor():
        unreal.EditorLevelLibrary.editor_end_play()
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    except Exception:
        loaded = level_editor.load_level(LEVEL_PATH)
        world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
                 if loaded else None)
    assert world, LEVEL_PATH
    return world


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def discover_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DESTINATION], True)
    meshes = []
    for path in unreal.EditorAssetLibrary.list_assets(DESTINATION, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            meshes.append(asset)
    meshes.sort(key=lambda mesh: mesh.get_name())
    assert len(meshes) == 10, [mesh.get_path_name() for mesh in meshes]
    return meshes


def union_mesh_bounds(meshes):
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3
    for mesh in meshes:
        bounds = mesh.get_bounds()
        current_min = [bounds.origin.x - bounds.box_extent.x,
                        bounds.origin.y - bounds.box_extent.y,
                        bounds.origin.z - bounds.box_extent.z]
        current_max = [bounds.origin.x + bounds.box_extent.x,
                        bounds.origin.y + bounds.box_extent.y,
                        bounds.origin.z + bounds.box_extent.z]
        mins = [min(a, b) for a, b in zip(mins, current_min)]
        maxs = [max(a, b) for a, b in zip(maxs, current_max)]
    return mins, maxs


world = editor_world()
actors = all_actors()
old = [actor for actor in actors if actor.get_actor_label() == OLD_LABEL]
assert len(old) == 1, "Expected one existing Guidemen actor: {}".format(len(old))
old_actor = old[0]
old_bounds = actor_bounds(old_actor)
old_location = old_actor.get_actor_location()
old_rotation = old_actor.get_actor_rotation()
old_scale = old_actor.get_actor_scale3d()

existing_upgrade = [actor for actor in actors if UPGRADE_TAG in [str(tag) for tag in actor.tags]]
assert not existing_upgrade, "Realism upgrade actor group already exists"

meshes = discover_meshes()
local_min, local_max = union_mesh_bounds(meshes)
local_size = [b - a for a, b in zip(local_min, local_max)]

spawned = []
for mesh in meshes:
    label = ACTOR_PREFIX + mesh.get_name()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(), old_location, old_rotation, transient=False)
    assert actor, label
    actor.set_actor_scale3d(old_scale)
    actor.set_actor_label(label)
    actor.tags = [ACTOR_TAG, PARK_TAG, UPGRADE_TAG]
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component, label
    component.set_static_mesh(mesh)
    actor.modify()
    spawned.append(actor)

group_bounds = [actor_bounds(actor) for actor in spawned]
actual_bounds = [min(bounds[0] for bounds in group_bounds),
                 min(bounds[1] for bounds in group_bounds),
                 max(bounds[2] for bounds in group_bounds),
                 max(bounds[3] for bounds in group_bounds),
                 min(bounds[4] for bounds in group_bounds),
                 max(bounds[5] for bounds in group_bounds)]

# Preserve the prior footprint and terrain contact. The upgrade pack is
# authored in the same local coordinate system, so this should be zero.
z_shift = old_bounds[4] - actual_bounds[4]
if abs(z_shift) > 0.5:
    for actor in spawned:
        location = actor.get_actor_location()
        actor.set_actor_location(unreal.Vector(location.x, location.y, location.z + z_shift), False, True)
    group_bounds = [actor_bounds(actor) for actor in spawned]
    actual_bounds = [min(bounds[0] for bounds in group_bounds),
                     min(bounds[1] for bounds in group_bounds),
                     max(bounds[2] for bounds in group_bounds),
                     max(bounds[3] for bounds in group_bounds),
                     min(bounds[4] for bounds in group_bounds),
                     max(bounds[5] for bounds in group_bounds)]

assert abs(actual_bounds[4] - old_bounds[4]) <= 2.0, (actual_bounds, old_bounds)
assert abs(actual_bounds[0] - old_bounds[0]) <= 2.0, (actual_bounds, old_bounds)
assert abs(actual_bounds[1] - old_bounds[1]) <= 2.0, (actual_bounds, old_bounds)
assert abs(actual_bounds[2] - old_bounds[2]) <= 2.0, (actual_bounds, old_bounds)
assert abs(actual_bounds[3] - old_bounds[3]) <= 2.0, (actual_bounds, old_bounds)

external_static = []
for actor in actors:
    if actor is old_actor or actor.get_class().get_name().startswith("Landscape"):
        continue
    if actor.get_class().get_name().startswith("StaticMeshActor"):
        external_static.append(actor_bounds(actor))
assert not any(overlaps(actual_bounds, other) for other in external_static), "Upgrade overlaps existing static mesh"

assert unreal.EditorLevelLibrary.destroy_actor(old_actor), "Failed to remove old Guidemen actor"
assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Level save failed"

manifest = {
    "level": LEVEL_PATH,
    "asset_folder": DESTINATION,
    "tag": ACTOR_TAG,
    "upgrade_tag": UPGRADE_TAG,
    "old_actor": OLD_LABEL,
    "actors": [actor.get_actor_label() for actor in spawned],
    "location": [spawned[0].get_actor_location().x,
                  spawned[0].get_actor_location().y,
                  spawned[0].get_actor_location().z],
    "rotation": [old_rotation.roll, old_rotation.pitch, old_rotation.yaw],
    "scale": [old_scale.x, old_scale.y, old_scale.z],
    "ground_z": actual_bounds[4],
    "old_bounds": list(old_bounds),
    "bounds": actual_bounds,
    "size_cm": local_size,
    "mesh_count": len(meshes),
    "mesh_paths": [mesh.get_path_name() for mesh in meshes],
    "nanite": {mesh.get_name(): mesh.get_editor_property("nanite_settings").enabled for mesh in meshes},
    "collision": "disabled for separate gameplay collision authoring",
}
PLACEMENT_REPORT.parent.mkdir(parents=True, exist_ok=True)
PLACEMENT_REPORT.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print("GUIDEMEN_REALISM_PLACEMENT", json.dumps(manifest))
