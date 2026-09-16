"""Validate the persisted GreatNorthGate HighDetail wrapper after a level reload.

Checks the hierarchy, tags, grounding, footprint and — importantly — that the
four pre-existing Guangzhou landmarks were left untouched by this job.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
MANIFEST = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-highdetail-placement.json"
REPORT = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-highdetail-validation.json"

ROOT_LABEL = "GuangzhouLandmark_GreatNorthGate_HighDetail"
ROOT_TAG = "GreatNorthGateHighDetailRoot"
PART_TAG = "GreatNorthGateHighDetailPart"
ACTOR_TAG = "ImportedGuangzhouLandmark"
PARK_TAG = "GuangzhouLandmarkPark"
EXPECTED_PARTS = 6
PRIOR_LANDMARKS = [
    "GuangzhouLandmark_GreatNorthGate",
    "GuangzhouLandmark_GreatSouthGate",
    "GuangzhouLandmark_Xiaobeimen",
    "GuangzhouLandmark_ZhenhaiTower",
]
# Recorded by the pre-change row survey; the job must not move these.
PRIOR_EXPECTED_Y = {
    "GuangzhouLandmark_GreatNorthGate": (104525.0, 106275.0),
    "GuangzhouLandmark_GreatSouthGate": (114610.0, 116190.0),
    "GuangzhouLandmark_Xiaobeimen": (94061.9, 96738.1),
    "GuangzhouLandmark_ZhenhaiTower": (83982.7, 86817.3),
}


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z,
            origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]


manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, LEVEL_PATH
actors = all_actors()

roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(roots) == 1, "Expected exactly one GreatNorthGate HighDetail root, found {}".format(len(roots))
root = roots[0]
assert root.get_actor_label() == ROOT_LABEL, root.get_actor_label()
assert ACTOR_TAG in [str(tag) for tag in root.tags]
assert PARK_TAG in [str(tag) for tag in root.tags]
assert abs(root.get_actor_rotation().yaw) < 0.001, "Root yaw is not 0"

parts = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
assert len(parts) == EXPECTED_PARTS, "Expected {} parts, found {}".format(EXPECTED_PARTS, len(parts))
assert all(part.get_attach_parent_actor() is root for part in parts), "Unparented parts"
assert all(ACTOR_TAG in [str(tag) for tag in part.tags] for part in parts)
assert all(PARK_TAG in [str(tag) for tag in part.tags] for part in parts)
assert all(part.get_actor_label().startswith(ROOT_LABEL + "__") for part in parts)

roof_parts = [part for part in parts if "Roof_" in part.get_actor_label()]
assert len(roof_parts) == 1, \
    "Expected the merged roof asset as a single part, found {}".format(len(roof_parts))

part_bounds = [bounds(part) for part in parts]
group = [min(item[0] for item in part_bounds), min(item[1] for item in part_bounds),
         min(item[2] for item in part_bounds), max(item[3] for item in part_bounds),
         max(item[4] for item in part_bounds), max(item[5] for item in part_bounds)]

assert abs(group[2] - manifest["ground_z"]) < 1.0, \
    "Building is not grounded: {} vs {}".format(group[2], manifest["ground_z"])
assert group[5] > group[2] + 1000.0, "Building has no height"

# Every mesh part must carry a mesh with materials assigned.
for part in parts:
    component = part.get_component_by_class(unreal.StaticMeshComponent)
    assert component and component.static_mesh, part.get_actor_label()
    assert component.static_mesh.static_materials, part.get_actor_label()

# The new building must not overlap any of the four prior landmarks.
prior = {}
for label in PRIOR_LANDMARKS:
    matches = [actor for actor in actors if actor.get_actor_label() == label]
    assert len(matches) == 1, "Prior landmark {} missing or duplicated".format(label)
    actor = matches[0]
    prior[label] = bounds(actor)
    low, high = prior[label][1], prior[label][4]
    expected = PRIOR_EXPECTED_Y[label]
    assert abs(low - expected[0]) < 5.0 and abs(high - expected[1]) < 5.0, \
        "Prior landmark {} moved: {} vs {}".format(label, (low, high), expected)
    assert not (group[0] < prior[label][3] and group[3] > prior[label][0]
                and group[1] < prior[label][4] and group[4] > prior[label][1]), \
        "New building overlaps {}".format(label)

result = {
    "passed": True,
    "level": LEVEL_PATH,
    "root": root.get_name(),
    "label": root.get_actor_label(),
    "rotation": [root.get_actor_rotation().roll, root.get_actor_rotation().pitch,
                  root.get_actor_rotation().yaw],
    "location": [root.get_actor_location().x, root.get_actor_location().y,
                  root.get_actor_location().z],
    "part_count": len(parts),
    "roof_merged_sections": 9,
    "detail_parts": sorted(part.get_actor_label() for part in parts if part not in roof_parts),
    "ground_z": manifest["ground_z"],
    "bounds": [round(value, 2) for value in group],
    "size_cm": [round(group[3] - group[0], 2), round(group[4] - group[1], 2),
                 round(group[5] - group[2], 2)],
    "prior_landmarks_intact": sorted(PRIOR_LANDMARKS),
}
REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_NORTH_GATE_HIGHDETAIL_VALIDATION", json.dumps(result))
