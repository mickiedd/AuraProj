"""Read-only save/reload validation for the Guidemen realism upgrade group."""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/RealismUpgrade"
ACTOR_TAG = "ImportedGuidemenBuilding"
UPGRADE_TAG = "GuidemenRealismUpgrade"
ROOT_TAG = "GuidemenRealismUpgradeRoot"
PART_TAG = "GuidemenRealismUpgradePart"
PARENT_LABEL = "GuangzhouLandmark_Guidemen"
ACTOR_PREFIX = "GuangzhouLandmark_Guidemen__"
MESH_COUNT = 10
REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/guidemen-realism-placement.json")
VALIDATION_REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/guidemen-realism-validation.json")


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z)


def overlaps(left, right, padding=1.0):
    return (left[0] - padding < right[2] and left[2] + padding > right[0] and
            left[1] - padding < right[3] and left[3] + padding > right[1])


def load_level():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.is_in_play_in_editor():
        unreal.EditorLevelLibrary.editor_end_play()
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH))
    except Exception:
        loaded = level_editor.load_level(LEVEL_PATH)
        world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
                 if loaded else None)
    return world


world = load_level()
assert world, LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(roots) == 1, len(roots)
root = roots[0]
assert root.get_actor_label() == PARENT_LABEL
assert ACTOR_TAG in [str(tag) for tag in root.tags]
group = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
assert len(group) == MESH_COUNT, len(group)
assert all(UPGRADE_TAG in [str(tag) for tag in actor.tags] for actor in group)
assert all(actor.get_actor_label().startswith(ACTOR_PREFIX) for actor in group)
assert all(actor.get_attach_parent_actor() is root for actor in group)

mesh_paths = []
for actor in group:
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component and component.static_mesh, actor.get_name()
    path = component.static_mesh.get_path_name()
    assert path.startswith(DESTINATION + "/"), path
    mesh_paths.append(path)

manifest = json.loads(REPORT.read_text(encoding="utf-8"))
assert manifest["level"] == LEVEL_PATH
assert manifest["asset_folder"] == DESTINATION
assert manifest["mesh_count"] == MESH_COUNT
assert sorted(manifest["actors"]) == sorted(actor.get_actor_label() for actor in group)

part_bounds = [bounds(actor) for actor in group]
actual = [min(item[0] for item in part_bounds), min(item[1] for item in part_bounds),
          max(item[2] for item in part_bounds), max(item[3] for item in part_bounds),
          min(item[4] for item in part_bounds), max(item[5] for item in part_bounds)]
assert all(abs(actual[index] - manifest["bounds"][index]) <= 2.0 for index in range(6))
assert abs(actual[4] - manifest["ground_z"]) <= 2.0

external_overlap = []
for other in actors:
    if other in group or other.get_class().get_name().startswith("Landscape"):
        continue
    if other.get_class().get_name().startswith("StaticMeshActor") and overlaps(actual, bounds(other)):
        external_overlap.append(other.get_name())
assert not external_overlap, external_overlap

result = {
    "passed": True,
    "level": LEVEL_PATH,
    "asset_folder": DESTINATION,
    "piece_count": len(group),
    "actors": sorted(actor.get_actor_label() for actor in group),
    "mesh_paths": sorted(mesh_paths),
    "bounds": actual,
    "ground_z": manifest["ground_z"],
    "external_overlap": external_overlap,
}
VALIDATION_REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GUIDEMEN_REALISM_VALIDATION", json.dumps(result))
