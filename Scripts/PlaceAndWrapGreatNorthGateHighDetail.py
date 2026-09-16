"""Place and wrap the imported GreatNorthGate HighDetail building in the level.

Creates one root actor (GuangzhouLandmark_GreatNorthGate_HighDetail) holding the
six imported mesh parts: the five detail meshes plus the roof asset, which
already contains all nine roof-tile sections because Interchange merges the
source's 9 roof nodes into that single mesh.

All six parts sit at identity in one shared building space, so no per-part
transform is needed.  The building is grounded on the terrain and placed in the
landmark row between the existing GuangzhouLandmark_GreatNorthGate and
GuangzhouLandmark_GreatSouthGate.  The four pre-existing landmarks are left
untouched.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatNorthGate_HighDetail.json"
MANIFEST = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-highdetail-placement.json"

ROOT_LABEL = "GuangzhouLandmark_GreatNorthGate_HighDetail"
ROOT_TAG = "GreatNorthGateHighDetailRoot"
PART_TAG = "GreatNorthGateHighDetailPart"
ACTOR_TAG = "ImportedGuangzhouLandmark"
PARK_TAG = "GuangzhouLandmarkPark"

# Free footprint in the landmark row: the corridor at x=-140400 holds only the
# four original landmarks, and this centre sits 3767 cm clear of both neighbours.
CENTER_X = -140400.0
CENTER_Y = 110442.5
CLEARANCE = 500.0
EXPECTED_PARTS = 6


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z,
            origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]


def load_level():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
             if loaded else None)
    assert world, LEVEL_PATH
    return world


def trace_ground_z(world, x, y, ignore):
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    if object_type is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -5000.0),
        types, True, ignore, draw, False)
    data = hit.to_tuple()
    return float(data[4].z) if data and data[0] is True else 0.0


import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
assert import_report["mesh_count"] == EXPECTED_PARTS, import_report["mesh_count"]

world = load_level()
actors = all_actors()

existing_roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(existing_roots) == 0, "GreatNorthGate HighDetail root already exists: " + \
    ", ".join(actor.get_actor_label() for actor in existing_roots)

# Overlap guard against every other static mesh actor in the level.
candidate = [CENTER_X - 1300.0, CENTER_Y - 700.0, -1e9,
             CENTER_X + 1300.0, CENTER_Y + 700.0, 1e9]
conflicts = []
for actor in actors:
    if not actor.get_class().get_name().startswith("StaticMeshActor"):
        continue
    other = bounds(actor)
    if (candidate[0] - CLEARANCE < other[3] and candidate[3] + CLEARANCE > other[0]
            and candidate[1] - CLEARANCE < other[4] and candidate[4] + CLEARANCE > other[1]):
        conflicts.append(actor.get_actor_label())
assert not conflicts, "Footprint overlaps: " + ", ".join(sorted(conflicts))

ignore = unreal.Array(unreal.Actor)
for actor in actors:
    if not actor.get_class().get_name().startswith("Landscape"):
        ignore.append(actor)
ground_z = trace_ground_z(world, CENTER_X, CENTER_Y, ignore)

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
root = actor_subsystem.spawn_actor_from_class(
    unreal.Actor.static_class(), unreal.Vector(CENTER_X, CENTER_Y, ground_z),
    unreal.Rotator(0.0, 0.0, 0.0), transient=False)
assert root, ROOT_LABEL
root.set_actor_label(ROOT_LABEL)
root.tags = [ACTOR_TAG, PARK_TAG, ROOT_TAG]
# A static root is required before attaching StaticMeshActor roots.
root.root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

root_location = root.get_actor_location()
part_tags = [ACTOR_TAG, PARK_TAG, PART_TAG]
part_labels = []
for entry in import_report["meshes"]:
    mesh = unreal.EditorAssetLibrary.load_asset(entry["path"])
    assert isinstance(mesh, unreal.StaticMesh), entry["path"]
    label = ROOT_LABEL + "__" + entry["name"]
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(), root_location, unreal.Rotator(0.0, 0.0, 0.0),
        transient=False)
    assert actor, label
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component, label
    component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.tags = part_tags
    attached = actor.root_component.attach_to_component(
        root.root_component, unreal.Name(""),
        unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
        unreal.AttachmentRule.KEEP_WORLD, False)
    assert attached, label
    actor.modify()
    part_labels.append(label)

# Ground the assembled group: shift the root so the lowest point sits on terrain.
parts = [actor for actor in all_actors() if PART_TAG in [str(tag) for tag in actor.tags]]
assert len(parts) == EXPECTED_PARTS, len(parts)
group_min_z = min(bounds(actor)[2] for actor in parts)
root.set_actor_location(
    unreal.Vector(root_location.x, root_location.y, root_location.z + (ground_z - group_min_z)),
    False, True)

parts = [actor for actor in all_actors() if PART_TAG in [str(tag) for tag in actor.tags]]
part_bounds = [bounds(actor) for actor in parts]
actual_bounds = [min(item[0] for item in part_bounds), min(item[1] for item in part_bounds),
                 min(item[2] for item in part_bounds), max(item[3] for item in part_bounds),
                 max(item[4] for item in part_bounds), max(item[5] for item in part_bounds)]
assert abs(actual_bounds[2] - ground_z) < 1.0, "Building is not grounded: {} vs {}".format(
    actual_bounds[2], ground_z)
assert all(actor.get_attach_parent_actor() is root for actor in parts), "Unparented parts"
assert actual_bounds[5] - actual_bounds[2] > 1000.0, "Building has no height"

assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Failed to save showcase level"

manifest = {
    "level": LEVEL_PATH,
    "source": import_report["source"],
    "asset_folder": import_report["destination"],
    "root": root.get_actor_label(),
    "root_actor": root.get_name(),
    "root_tags": [str(tag) for tag in root.tags],
    "location": [root.get_actor_location().x, root.get_actor_location().y,
                  root.get_actor_location().z],
    "rotation": [root.get_actor_rotation().roll, root.get_actor_rotation().pitch,
                  root.get_actor_rotation().yaw],
    "center_xy": [(actual_bounds[0] + actual_bounds[3]) / 2.0,
                   (actual_bounds[1] + actual_bounds[4]) / 2.0],
    "ground_z": ground_z,
    "bounds": actual_bounds,
    "size_cm": [actual_bounds[3] - actual_bounds[0], actual_bounds[4] - actual_bounds[1],
                 actual_bounds[5] - actual_bounds[2]],
    "parts": sorted(part_labels),
    "part_count": len(parts),
    "roof_sections_merged": 9,
    "passed": True,
}
MANIFEST.parent.mkdir(parents=True, exist_ok=True)
MANIFEST.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print("GREAT_NORTH_GATE_HIGHDETAIL_PLACEMENT", json.dumps({
    "root": manifest["root"],
    "location": [round(v, 1) for v in manifest["location"]],
    "ground_z": round(ground_z, 2),
    "bounds": [round(v, 1) for v in actual_bounds],
    "size_cm": [round(v, 1) for v in manifest["size_cm"]],
    "part_count": manifest["part_count"]}))
