"""Validate the persisted Great West Gate parent actor and attached parts."""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
ACTOR_TAG = "ImportedGreatWestGate"
PARK_TAG = "GuangzhouLandmarkPark"
ROOT_TAG = "GreatWestGateRoot"
PART_TAG = "GreatWestGatePart"
PARENT_LABEL = "GuangzhouLandmark_GreatWestGate"
PART_PREFIX = "GuangzhouLandmark_GreatWestGate__"
REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/great-west-gate-wrapper.json")
VALIDATION_REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/great-west-gate-wrapper-validation.json")


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z]


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
loaded = level_editor.load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(roots) == 1, len(roots)
root = roots[0]
assert root.get_actor_label() == PARENT_LABEL
assert ACTOR_TAG in [str(tag) for tag in root.tags]
assert PARK_TAG in [str(tag) for tag in root.tags]
assert abs(root.get_actor_rotation().yaw) < 0.001

parts = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
assert len(parts) == 20, len(parts)
assert all(PARK_TAG in [str(tag) for tag in part.tags] for part in parts)
assert all(part.get_actor_label().startswith(PART_PREFIX) for part in parts)
assert all(part.get_attach_parent_actor() is root for part in parts)

manifest = json.loads(REPORT.read_text(encoding="utf-8"))
assert manifest["level"] == LEVEL_PATH
assert manifest["root"] == PARENT_LABEL
assert manifest["part_count"] == 20
assert sorted(manifest["parts"]) == sorted(part.get_actor_label() for part in parts)

part_bounds = [bounds(part) for part in parts]
actual_bounds = [min(item[0] for item in part_bounds), min(item[1] for item in part_bounds),
                 max(item[2] for item in part_bounds), max(item[3] for item in part_bounds),
                 min(item[4] for item in part_bounds), max(item[5] for item in part_bounds)]
result = {
    "passed": True,
    "level": LEVEL_PATH,
    "root": root.get_name(),
    "label": root.get_actor_label(),
    "rotation": [root.get_actor_rotation().roll, root.get_actor_rotation().pitch, root.get_actor_rotation().yaw],
    "location": [root.get_actor_location().x, root.get_actor_location().y, root.get_actor_location().z],
    "part_count": len(parts),
    "parent_links": [part.get_attach_parent_actor().get_actor_label() for part in sorted(parts, key=lambda actor: actor.get_actor_label())],
    "bounds": actual_bounds,
}
VALIDATION_REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_WEST_GATE_WRAPPER_VALIDATION", json.dumps(result))
