"""Read-only validation for the Guidemen placement in Scifi Desert."""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
MESH_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/SM_Guidemen"
MESH_OBJECT_PATH = MESH_PATH + ".SM_Guidemen"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guidemen-placement.json")
ACTOR_TAG = "ImportedGuidemenBuilding"


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (
        origin.x - extent.x,
        origin.y - extent.y,
        origin.x + extent.x,
        origin.y + extent.y,
        origin.z - extent.z,
        origin.z + extent.z,
    )


def overlaps_xy(left, right, padding=1.0):
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
        loaded = level_editor.load_level(LEVEL_PATH)
        world = (
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if loaded
            else None
        )
    return world


world = load_level()
assert world, LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
tagged = [actor for actor in actors if ACTOR_TAG in [str(tag) for tag in actor.tags]]
assert len(tagged) == 1, len(tagged)
actor = tagged[0]
component = actor.get_component_by_class(unreal.StaticMeshComponent)
assert component and component.static_mesh
assert component.static_mesh.get_path_name() == MESH_OBJECT_PATH
assert actor.get_actor_label() == "GuangzhouLandmark_Guidemen"

manifest = json.loads(MANIFEST.read_text())
assert manifest["level"] == LEVEL_PATH
assert manifest["asset"] == MESH_PATH
assert manifest["actor"] == actor.get_name()
actual = bounds(actor)
assert abs(actual[4] - manifest["ground_z"]) <= 2.0
assert all(
    abs(actual[index] - manifest["bounds"][index]) <= 2.0 for index in range(6)
)

overlap_errors = []
for other in actors:
    if other is actor or other.get_class().get_name().startswith("Landscape"):
        continue
    if other.get_class().get_name().startswith("StaticMeshActor") and overlaps_xy(
        actual, bounds(other)
    ):
        overlap_errors.append(other.get_name())
assert not overlap_errors, overlap_errors

result = {
    "passed": True,
    "level": LEVEL_PATH,
    "actor": actor.get_name(),
    "label": actor.get_actor_label(),
    "asset": component.static_mesh.get_path_name(),
    "location": [
        actor.get_actor_location().x,
        actor.get_actor_location().y,
        actor.get_actor_location().z,
    ],
    "bounds": list(actual),
    "ground_z": manifest["ground_z"],
}
(MANIFEST.parent / "guidemen-placement-validation.json").write_text(
    json.dumps(result, indent=2)
)
print("GUIDEMEN_PLACEMENT_VALIDATION", json.dumps(result))
