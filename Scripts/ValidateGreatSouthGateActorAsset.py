"""Validate the serialized Great South Gate V2 independent Blueprint asset."""
import json
import os

import unreal


ASSET_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"
REPORT_PATH = "C:/Git/AuraProj/Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-actor-asset.json"
VALIDATION_PATH = "C:/Git/AuraProj/Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-actor-asset-validation.json"


def path_of(obj):
    return obj.get_path_name() if obj else ""


def write_report(data):
    os.makedirs(os.path.dirname(VALIDATION_PATH), exist_ok=True)
    with open(VALIDATION_PATH, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)


source_report = {}
if os.path.exists(REPORT_PATH):
    with open(REPORT_PATH, "r", encoding="utf-8") as handle:
        source_report = json.load(handle)

bp = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
assert bp, ASSET_PATH

generated_class = bp.generated_class()
assert generated_class, "Blueprint has no generated class"
generated_default_object = generated_class.get_default_object()
assert generated_default_object, "Blueprint generated class has no default object"
# UE 5.5's Python wrapper does not expose the Blueprint parent-class field on
# BlueprintGeneratedClass. The creator records the exact parent used, while
# this validator independently reopens the serialized asset and verifies its
# generated class, components, meshes, and instance payload.
parent_class_path = source_report.get("parent_class", "")
is_packed_level_actor = parent_class_path == "/Script/Engine.PackedLevelActor"

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
component_records = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    if not library.is_component(data):
        continue
    component = library.get_object(data)
    if not component:
        continue
    class_name = component.get_class().get_name()
    if class_name != "HierarchicalInstancedStaticMeshComponent":
        continue
    component_records.append({
        "name": component.get_name(),
        "mesh": path_of(component.static_mesh),
        "instance_count": component.get_instance_count(),
    })

component_count = len(component_records)
instance_count = sum(record["instance_count"] for record in component_records)
expected_component_count = source_report.get("hism_component_count")
expected_instance_count = source_report.get("instance_count")

report = {
    "passed": (
        is_packed_level_actor
        and component_count == expected_component_count
        and instance_count == expected_instance_count
        and all(record["mesh"] for record in component_records)
    ),
    "blueprint": ASSET_PATH,
    "generated_class": path_of(generated_class),
    "generated_default_object_class": path_of(generated_default_object.get_class()),
    "parent_class": parent_class_path,
    "hism_component_count": component_count,
    "expected_hism_component_count": expected_component_count,
    "instance_count": instance_count,
    "expected_instance_count": expected_instance_count,
    "empty_mesh_components": sum(1 for record in component_records if not record["mesh"]),
    "component_records": component_records,
}
write_report(report)
assert report["passed"], json.dumps(report)
print("GREAT_SOUTH_GATE_ACTOR_ASSET_VALIDATED", json.dumps({
    key: value for key, value in report.items() if key != "component_records"
}))
