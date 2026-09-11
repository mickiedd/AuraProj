"""Place the imported Guidemen mesh as a building in Scifi Desert.

The script chooses the first clear slot on the established western-edge
landmark grid, grounds the actor against the level landscape, refuses to
duplicate an existing Guidemen placement, and writes a reversible manifest.
"""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
MESH_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guidemen-placement.json")
ACTOR_TAG = "ImportedGuidemenBuilding"
ACTOR_LABEL = "GuangzhouLandmark_Guidemen"
CLEARANCE = 2500.0
GRID_STEP = 5000.0
EDGE_MARGIN = 12000.0


def actor_bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (
        origin.x - extent.x,
        origin.y - extent.y,
        origin.x + extent.x,
        origin.y + extent.y,
        origin.z - extent.z,
        origin.z + extent.z,
    )


def overlaps(left, right, padding=CLEARANCE):
    return (
        left[0] - padding < right[2]
        and left[2] + padding > right[0]
        and left[1] - padding < right[3]
        and left[3] + padding > right[1]
    )


def load_level():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.is_in_play_in_editor():
        unreal.EditorLevelLibrary.editor_end_play()
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH))
    except Exception:
        loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
        world = (
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if loaded
            else None
        )
    assert world, LEVEL_PATH
    return world


def trace_ground_z(world, x, y, ignore):
    start = unreal.Vector(x, y, 60000.0)
    end = unreal.Vector(x, y, -5000.0)
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    if object_type is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, start, end, types, True, ignore, draw, False
    )
    data = hit.to_tuple()
    if data and data[0] is True:
        return float(data[4].z)
    return 0.0


world = load_level()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
existing_guidemen = [actor for actor in actors if ACTOR_TAG in [str(tag) for tag in actor.tags]]
assert not existing_guidemen, "Guidemen placement already exists"

mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
assert isinstance(mesh, unreal.StaticMesh), MESH_PATH
mesh_bounds = mesh.get_bounds()
mesh_origin = mesh_bounds.origin
mesh_extent = mesh_bounds.box_extent
mesh_size = (mesh_extent.x * 2.0, mesh_extent.y * 2.0, mesh_extent.z * 2.0)

landscapes = [
    actor for actor in actors if actor.get_class().get_name().startswith("Landscape")
]
assert landscapes, "No Landscape actors found"
landscape_bounds = [actor_bounds(actor) for actor in landscapes]
terrain_rect = (
    min(item[0] for item in landscape_bounds) + EDGE_MARGIN,
    min(item[1] for item in landscape_bounds) + EDGE_MARGIN,
    max(item[2] for item in landscape_bounds) - EDGE_MARGIN,
    max(item[3] for item in landscape_bounds) - EDGE_MARGIN,
)

existing_static = [
    actor_bounds(actor)
    for actor in actors
    if actor.get_class().get_name().startswith("StaticMeshActor")
]
min_x, min_y, max_x, max_y = terrain_rect
slot = None
x = min_x
while x <= max_x and slot is None:
    y = max_y
    while y >= min_y:
        candidate = (
            x - mesh_extent.x,
            y - mesh_extent.y,
            x + mesh_extent.x,
            y + mesh_extent.y,
            0.0,
            0.0,
        )
        if not any(overlaps(candidate, old) for old in existing_static):
            slot = (x, y, candidate)
            break
        y -= GRID_STEP
    x += GRID_STEP
assert slot, "No clear Guidemen footprint found"

ignore = unreal.Array(unreal.Actor)
for actor in actors:
    if not actor.get_class().get_name().startswith("Landscape"):
        ignore.append(actor)

center_x, center_y, candidate = slot
ground_z = trace_ground_z(world, center_x, center_y, ignore)
bottom_z = mesh_origin.z - mesh_extent.z
location = unreal.Vector(
    center_x - mesh_origin.x,
    center_y - mesh_origin.y,
    ground_z - bottom_z,
)

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.StaticMeshActor.static_class(),
    location,
    unreal.Rotator(0.0, 0.0, 0.0),
    transient=False,
)
assert actor, ACTOR_LABEL
component = actor.get_component_by_class(unreal.StaticMeshComponent)
assert component
component.set_static_mesh(mesh)
actor.set_actor_label(ACTOR_LABEL)
actor.tags = [ACTOR_TAG, "GuangzhouLandmarkPark"]

actual_bounds = actor_bounds(actor)
assert not any(overlaps(actual_bounds, old, padding=1.0) for old in existing_static)
assert abs(actual_bounds[4] - ground_z) <= 2.0, actual_bounds

manifest = {
    "level": LEVEL_PATH,
    "tag": ACTOR_TAG,
    "label": ACTOR_LABEL,
    "asset": MESH_PATH,
    "terrain_rect": list(terrain_rect),
    "center_xy": [center_x, center_y],
    "location": [location.x, location.y, location.z],
    "ground_z": ground_z,
    "size_cm": list(mesh_size),
    "bounds": list(actual_bounds),
    "actor": actor.get_name(),
}
saved = unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
assert saved, "Level save failed; a running process may hold the map package open"
MANIFEST.parent.mkdir(parents=True, exist_ok=True)
MANIFEST.write_text(json.dumps(manifest, indent=2))
print("GUIDEMEN_PLACED", json.dumps(manifest))
