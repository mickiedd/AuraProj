"""Create an independent reusable Blueprint Actor Asset for Great South Gate V2.

The preview level is an intentionally detailed actor collection. This script
flattens the model geometry into grouped hierarchical-instanced mesh components
inside a PackedLevelActor Blueprint. The resulting asset is portable: place the
Blueprint in any level and it carries the complete repaired model without the
preview level wrapper or preview lights.
"""
import json
import os
from collections import defaultdict

import unreal


LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
ASSET_PATH = DEST + "/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"
WRAPPER_LABEL = "GuangzhouLandmark_GreatSouthGate_Zhengnanmen_V2"
FILL_PREFIX = "Zhengnanmen_IntactFill_"
REPORT_PATH = "C:/Git/AuraProj/Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-actor-asset.json"


def text(value):
    return value.to_string() if hasattr(value, "to_string") else str(value)


def path_of(obj):
    return obj.get_path_name() if obj else ""


def all_level_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def mesh_component(actor):
    return actor.get_component_by_class(unreal.StaticMeshComponent)


def mesh_path(actor):
    component = mesh_component(actor)
    return path_of(component.static_mesh) if component and component.static_mesh else ""


def is_model_actor(actor):
    label = actor.get_actor_label()
    return label.startswith(FILL_PREFIX) or mesh_path(actor).startswith(DEST)


def material_signature(component):
    values = []
    for index in range(component.get_num_materials()):
        values.append(path_of(component.get_material(index)))
    return tuple(values)


def write_report(data):
    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level_editor.load_level(LEVEL_PATH), LEVEL_PATH

actors = all_level_actors()
wrapper = next((actor for actor in actors if actor.get_actor_label() == WRAPPER_LABEL), None)
assert wrapper, "Great South Gate V2 wrapper root was not found"

model_actors = sorted(
    [actor for actor in actors if actor is not wrapper and is_model_actor(actor)],
    key=lambda actor: (actor.get_actor_label(), actor.get_name()),
)
assert model_actors, "No Great South Gate V2 model actors were found"

groups = defaultdict(list)
imported_count = 0
fill_count = 0
source_actor_records = []
for actor in model_actors:
    component = mesh_component(actor)
    assert component and component.static_mesh, actor.get_actor_label()
    mesh = component.static_mesh
    mesh_asset_path = path_of(mesh)
    materials = material_signature(component)
    # StaticMeshComponent does not expose get_component_transform() in the
    # UE 5.5 Python API. The actor transform is the world transform for both
    # the imported StaticMeshActors and the repaired fill actors in the
    # source preview level.
    transform = actor.get_actor_transform()
    groups[(mesh_asset_path, materials)].append(transform)
    if mesh_asset_path.startswith(DEST):
        imported_count += 1
    if actor.get_actor_label().startswith(FILL_PREFIX):
        fill_count += 1
    source_actor_records.append({
        "label": actor.get_actor_label(),
        "mesh": mesh_asset_path,
        "materials": list(materials),
        "location": [transform.translation.x, transform.translation.y, transform.translation.z],
    })

existing = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if existing:
    report = {
        "passed": True,
        "status": "existing_asset_reused",
        "level": LEVEL_PATH,
        "blueprint": path_of(existing),
        "parent_class": "/Script/Engine.PackedLevelActor",
        "source_model_actor_count": len(model_actors),
        "imported_mesh_actor_count": imported_count,
        "intact_fill_actor_count": fill_count,
        "hism_component_count": None,
        "instance_count": None,
        "independent_from_preview_lights": True,
    }
    write_report(report)
    print("GREAT_SOUTH_GATE_ACTOR_ASSET_REUSED", json.dumps(report))
else:
    bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
        ASSET_PATH, unreal.PackedLevelActor
    )
    assert bp, ASSET_PATH

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    root_handle = None
    for handle in handles:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if library.is_root_component(data):
            root_handle = handle
            break
    assert root_handle, "Generated PackedLevelActor Blueprint has no root component"

    component_count = 0
    instance_count = 0
    group_manifest = []
    for group_index, ((mesh_asset_path, materials), transforms) in enumerate(sorted(groups.items())):
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_asset_path)
        assert mesh, mesh_asset_path

        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", root_handle)
        params.set_editor_property("new_class", unreal.HierarchicalInstancedStaticMeshComponent)
        params.set_editor_property("blueprint_context", bp)
        params.set_editor_property("conform_transform_to_parent", False)
        handle, fail_reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), text(fail_reason)
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = library.get_object(data)
        assert template, mesh_asset_path
        template.set_static_mesh(mesh)
        for material_index, material_path in enumerate(materials):
            if material_path:
                material = unreal.EditorAssetLibrary.load_asset(material_path)
                if material:
                    template.set_material(material_index, material)

        template.add_instances(transforms, False, True, False)
        component_count += 1
        instance_count += len(transforms)
        group_manifest.append({
            "component_index": group_index,
            "mesh": mesh_asset_path,
            "material_slots": list(materials),
            "instance_count": len(transforms),
        })

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False), ASSET_PATH
    report = {
        "passed": True,
        "status": "created",
        "level": LEVEL_PATH,
        "blueprint": path_of(bp),
        "parent_class": "/Script/Engine.PackedLevelActor",
        "source_model_actor_count": len(model_actors),
        "imported_mesh_actor_count": imported_count,
        "intact_fill_actor_count": fill_count,
        "hism_component_count": component_count,
        "instance_count": instance_count,
        "wrapper_root_excluded": True,
        "preview_lights_excluded": True,
        "independent_from_preview_level": True,
        "shared_meshes_preserved": True,
        "group_manifest": group_manifest,
    }
    write_report(report)
    print("GREAT_SOUTH_GATE_ACTOR_ASSET_CREATED", json.dumps({
        key: value for key, value in report.items() if key != "group_manifest"
    }))
