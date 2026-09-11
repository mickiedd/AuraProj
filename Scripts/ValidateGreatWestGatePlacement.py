"""Validate the imported Great West Gate mesh parts and saved placement."""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatWestGate.json"
PLACEMENT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/great-west-gate-placement.json"
REPORT = PROJECT_ROOT / "Saved/RawModelImport/great-west-gate-validation.json"
TAG = "ImportedGreatWestGate"
PARK_TAG = "GuangzhouLandmarkPark"
ROOT_TAG = "GreatWestGateRoot"
PART_TAG = "GreatWestGatePart"
PARENT_LABEL = "GuangzhouLandmark_GreatWestGate"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatWestGate/"


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z]


def overlaps_xy(left, right, padding=1.0):
    return (left[0] - padding < right[2] and left[2] + padding > right[0] and
            left[1] - padding < right[3] and left[3] + padding > right[1])


loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, LEVEL_PATH

import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
placement_report = json.loads(PLACEMENT_REPORT.read_text(encoding="utf-8"))
assert import_report["mesh_count"] == 20, import_report["mesh_count"]
assert len(import_report["mesh_paths"]) == 20

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(roots) == 1, "Expected one Great West Gate root"
root = roots[0]
assert root.get_actor_label() == PARENT_LABEL
assert TAG in [str(tag) for tag in root.tags]
assert abs(root.get_actor_rotation().yaw) < 0.001
group = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
assert len(group) == 20, "Expected 20 Great West parts, got {}".format(len(group))
assert all(PARK_TAG in [str(tag) for tag in actor.tags] for actor in group)

errors = []
mesh_paths = []
material_slots = 0
for actor in group:
    if actor.get_attach_parent_actor() is not root:
        errors.append(actor.get_actor_label() + " is not attached to the Great West Gate root")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or not component.static_mesh:
        errors.append(actor.get_actor_label() + " has no static mesh")
        continue
    mesh = component.static_mesh
    mesh_paths.append(mesh.get_path_name())
    if not mesh.get_path_name().startswith(DEST):
        errors.append(actor.get_actor_label() + " points outside GreatWestGate folder")
    material_slots += len(mesh.static_materials)
    if any(slot.material_interface is None for slot in mesh.static_materials):
        errors.append(actor.get_actor_label() + " has an unassigned material slot")
    if abs(actor.get_actor_location().x - root.get_actor_location().x) > 1.0 or abs(actor.get_actor_location().y - root.get_actor_location().y) > 1.0:
        errors.append(actor.get_actor_label() + " does not share the building transform")

group_bounds = [bounds(actor) for actor in group]
actual_bounds = [min(item[0] for item in group_bounds), min(item[1] for item in group_bounds),
                 max(item[2] for item in group_bounds), max(item[3] for item in group_bounds),
                 min(item[4] for item in group_bounds), max(item[5] for item in group_bounds)]
expected_bounds = placement_report["bounds"]
if any(abs(actual - expected) > 1.0 for actual, expected in zip(actual_bounds, expected_bounds)):
    errors.append("saved group bounds changed")
if abs(actual_bounds[4] - placement_report["ground_z"]) > 1.0:
    errors.append("Great West Gate is not grounded")
if abs((actual_bounds[0] + actual_bounds[2]) / 2.0 - placement_report["center_xy"][0]) > 1.0:
    errors.append("Great West Gate center X changed")
if abs((actual_bounds[1] + actual_bounds[3]) / 2.0 - placement_report["center_xy"][1]) > 1.0:
    errors.append("Great West Gate center Y changed")

# Overlap between the 20 constituent parts is expected: walls, roofs, trim,
# and interior pieces intentionally meet. Only the union footprint is checked
# against unrelated level actors below.
for building in group:
    building_bounds = bounds(building)
    for other in actors:
        if other in group or other.get_class().get_name().startswith("Landscape"):
            continue
        if other.get_class().get_name().startswith("StaticMeshActor") and overlaps_xy(building_bounds, bounds(other), padding=1.0):
            errors.append("overlap with existing actor: {} / {}".format(building.get_actor_label(), other.get_name()))

assert not errors, errors
unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
result = {
    "passed": True,
    "level": LEVEL_PATH,
    "tag": TAG,
    "part_count": len(group),
    "mesh_paths": sorted(mesh_paths),
    "material_slots": material_slots,
    "bounds": actual_bounds,
    "center_xy": [(actual_bounds[0] + actual_bounds[2]) / 2.0,
                   (actual_bounds[1] + actual_bounds[3]) / 2.0],
    "ground_z": actual_bounds[4],
    "nanite": import_report["nanite"],
    "old_landmark_tag_count": len([a for a in actors if "ImportedGuangzhouLandmark" in [str(tag) for tag in a.tags]]),
}
REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_WEST_GATE_VALIDATION", json.dumps(result))
