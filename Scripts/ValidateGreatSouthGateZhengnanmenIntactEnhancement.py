"""Validate the added solid façade infill and original scene preservation."""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
REPORT_PATH = Path("C:/Git/AuraProj/Saved/RawModelImport/GreatSouthGate_Zhengnanmen_IntactEnhancement-validation.json")
PREFIX = "Zhengnanmen_IntactFill_"
EXPECTED_FILL_ACTORS = 192
EXPECTED_IMPORTED_STATIC_MESH_ACTORS = 12195

assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH), LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
fill = []
original_static_mesh_actors = []
for actor in actors:
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if actor.get_actor_label().startswith(PREFIX):
        if component and component.static_mesh and component.static_mesh.get_path_name() == "/Engine/BasicShapes/Cube.Cube":
            fill.append((actor, component))
    elif component and component.static_mesh and component.static_mesh.get_path_name().startswith(DEST):
        original_static_mesh_actors.append((actor, component))

errors = []
if len(fill) != EXPECTED_FILL_ACTORS:
    errors.append("expected {} solid fill actors, found {}".format(EXPECTED_FILL_ACTORS, len(fill)))
if len(original_static_mesh_actors) != EXPECTED_IMPORTED_STATIC_MESH_ACTORS:
    errors.append("expected {} original imported mesh actors, found {}".format(EXPECTED_IMPORTED_STATIC_MESH_ACTORS, len(original_static_mesh_actors)))
if any(not component.get_editor_property("visible") for _actor, component in fill):
    errors.append("one or more fill components are hidden")
if any(component.get_collision_profile_name() != "NoCollision" for _actor, component in fill):
    errors.append("one or more fill components have collision enabled")
if any(actor.get_editor_property("is_editor_only_actor") for actor, _component in fill):
    errors.append("one or more fill actors are editor-only")
if any(component.get_material(0) is None for _actor, component in fill):
    errors.append("one or more fill components have no material")
core_material_overrides = 0
for _actor, component in original_static_mesh_actors:
    if component.static_mesh.get_name() in {"N_FloorCore_0", "N_FloorCore_1", "N_FloorCore_2"}:
        core_material_overrides += int(component.get_material(0) is not None and component.get_material(0).get_name() == "M_Wood_RedLacquer")
if core_material_overrides != 3:
    errors.append("expected red material overrides on three floor cores, found {}".format(core_material_overrides))
world_bounds = []
for actor, _component in fill:
    origin, extent = actor.get_actor_bounds(False)
    world_bounds.append((origin, extent))
mins = [min(origin.x - extent.x for origin, extent in world_bounds), min(origin.y - extent.y for origin, extent in world_bounds), min(origin.z - extent.z for origin, extent in world_bounds)]
maxs = [max(origin.x + extent.x for origin, extent in world_bounds), max(origin.y + extent.y for origin, extent in world_bounds), max(origin.z + extent.z for origin, extent in world_bounds)]
if mins[2] < 780.0 or maxs[2] < 1880.0:
    errors.append("fill does not cover all three timber levels: {}..{}".format(mins[2], maxs[2]))

result = {
    "passed": not errors,
    "level": LEVEL_PATH,
    "fill_actor_count": len(fill),
    "original_imported_static_mesh_actor_count": len(original_static_mesh_actors),
    "core_material_overrides": core_material_overrides,
    "total_level_actor_count": len(actors),
    "fill_bounds_cm": {"min": mins, "max": maxs, "size": [maxs[i] - mins[i] for i in range(3)]},
    "errors": errors,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_SOUTH_GATE_ZHENGNANMEN_INTACT_VALIDATION", json.dumps(result))
assert not errors, "; ".join(errors)
