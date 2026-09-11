"""Validate the Xiaobeimen v4 mesh and its saved level placement."""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
PLACEMENT = PROJECT_ROOT / "Saved/RawModelImport/guangzhou-landmark-placement.json"
REPORT = PROJECT_ROOT / "Saved/RawModelImport/xiaobeimen-v4-validation.json"
STATS = Path(
    "C:/Works/Raw3DModels/Extracted/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed/model_stats_v4.json"
)
TAG = "ImportedGuangzhouLandmark"
LABEL = "GuangzhouLandmark_Xiaobeimen"
NEW_ASSET = "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/SM_Xiaobeimen_GeometryFixed"
OLD_ASSET = "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/SM_Xiaobeimen"


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z]


def overlaps_xy(left, right, padding=1.0):
    return (left[0] - padding < right[2] and left[2] + padding > right[0] and
            left[1] - padding < right[3] and left[3] + padding > right[1])


world_loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if world_loaded else None)
assert world, LEVEL_PATH

placement = json.loads(PLACEMENT.read_text(encoding="utf-8"))
entry = next(item for item in placement["actors"] if item["label"] == LABEL)
stats = json.loads(STATS.read_text(encoding="utf-8"))
mesh = unreal.EditorAssetLibrary.load_asset(NEW_ASSET)
assert isinstance(mesh, unreal.StaticMesh), NEW_ASSET
assert unreal.EditorAssetLibrary.does_asset_exist(OLD_ASSET), "v3 rollback mesh was deleted"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
tagged = [actor for actor in actors if TAG in [str(tag) for tag in actor.tags]]
assert len(tagged) == 4, "Expected four tagged landmarks, got {}".format(len(tagged))
matches = [actor for actor in tagged if actor.get_actor_label() == LABEL]
assert len(matches) == 1, "Expected one Xiaobeimen actor"
actor = matches[0]
component = actor.get_component_by_class(unreal.StaticMeshComponent)
assert component and component.static_mesh == mesh, component.static_mesh.get_path_name() if component else "missing"

local = mesh.get_bounds()
local_min = [local.origin.x - local.box_extent.x, local.origin.y - local.box_extent.y, local.origin.z - local.box_extent.z]
local_max = [local.origin.x + local.box_extent.x, local.origin.y + local.box_extent.y, local.origin.z + local.box_extent.z]
expected_min, expected_max = stats["active_bounds_cm"]
# Unreal's OBJ importer mirrors the source Y axis into its left-handed world.
expected_unreal_min = [expected_min[0], -expected_max[1], expected_min[2]]
expected_unreal_max = [expected_max[0], -expected_min[1], expected_max[2]]
for actual, expected in zip(local_min + local_max, expected_unreal_min + expected_unreal_max):
    assert abs(actual - expected) < 1.0, "v4 bounds mismatch: {} vs {}".format(local_min + local_max, expected_unreal_min + expected_unreal_max)

assert mesh.get_editor_property("nanite_settings").enabled, "Nanite is disabled"
slot_names = [str(slot.material_slot_name) for slot in mesh.static_materials]
assert slot_names == [
    "Stone", "StoneLight", "WoodDark", "Metal", "StoneVar", "Plaster",
    "StoneDark", "WoodRed", "RoofGreen", "RoofTile", "RoofRidge", "StoneRoad",
], slot_names

actual = bounds(actor)
center_x = (actual[0] + actual[2]) / 2.0
center_y = (actual[1] + actual[3]) / 2.0
assert abs(center_x - entry["center_xy"][0]) < 1.0, "Xiaobeimen center X drifted"
assert abs(center_y - entry["center_xy"][1]) < 1.0, "Xiaobeimen center Y drifted"
assert abs(actual[4] - entry["ground_z"]) < 1.0, "Xiaobeimen ground contact drifted"

errors = []
for index, left in enumerate(tagged):
    for right in tagged[index + 1:]:
        if overlaps_xy(bounds(left), bounds(right), padding=100.0):
            errors.append("landmark overlap: {} / {}".format(left.get_actor_label(), right.get_actor_label()))
for landmark in tagged:
    landmark_bounds = bounds(landmark)
    for other in actors:
        class_name = other.get_class().get_name()
        if other in tagged or class_name.startswith("Landscape"):
            continue
        if class_name.startswith("StaticMeshActor") and overlaps_xy(landmark_bounds, bounds(other), padding=1.0):
            errors.append("overlap with existing actor: {} / {}".format(landmark.get_actor_label(), other.get_name()))
assert not errors, errors

result = {
    "passed": True,
    "level": LEVEL_PATH,
    "actor": actor.get_name(),
    "label": LABEL,
    "mesh": mesh.get_path_name(),
    "rollback_mesh_exists": True,
    "mesh_bounds_cm": {"min": local_min, "max": local_max},
    "actor_bounds": actual,
    "placement_center_xy": entry["center_xy"],
    "ground_z": entry["ground_z"],
    "source_final_triangles": stats["final_triangles"],
    "source_degenerate_triangles": stats["degenerate_triangles"],
    "source_roof_tile_triangles_inside_gate_opening": stats["roof_tile_triangles_inside_gate_opening"],
    "material_slots": slot_names,
    "nanite": True,
    "tagged_actor_count": len(tagged),
}
REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("XIAOBEIMEN_V4_VALIDATION", json.dumps(result))
