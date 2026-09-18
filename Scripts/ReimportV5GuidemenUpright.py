"""Reimport Guidemen with its source-specific zero-roll Interchange contract.

The original ``Meshes/Source`` assets remain as a rollback set.  The canonical
Blueprint is rebuilt against ``Meshes/UprightSource`` after exact mesh/instance
coverage checks.
"""
from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
EAL = unreal.EditorAssetLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _transform(record):
    return unreal.Transform(
        location=unreal.Vector(*record["location"]),
        rotation=unreal.Rotator(pitch=record["rotation"][0], yaw=record["rotation"][1], roll=record["rotation"][2]),
        scale=unreal.Vector(*record["scale"]),
    )


def _component_name(index, mesh_path):
    stem = mesh_path.rsplit("/", 1)[-1].split(".", 1)[0]
    return "HISM_{:03d}_{}".format(index, re.sub(r"[^A-Za-z0-9_]", "_", stem)[:48])


def _material_assets(cfg, tuning):
    result = {name: EAL.load_asset(path) for name, path in tuning["tuned_materials"].items()}
    assert set(result) == set(cfg["glb_material_names"])
    assert all(result.values())
    return result


def _import(cfg, materials, checkpoint):
    destination = cfg["destination"] + "/Meshes/UprightSource"
    existing = [
        actor for actor in _actors()
        if (actor.get_component_by_class(unreal.StaticMeshComponent)
            and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(destination + "/"))
    ]
    if EAL.does_directory_exist(destination):
        assert existing, "UprightSource exists without a recoverable scene"
        imported_actors = existing
        print("V5_GUIDEMEN_UPRIGHT_RECOVERING", len(imported_actors))
    else:
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.import_offset_rotation = unreal.Rotator(roll=0.0)
        pipeline.common_meshes_properties.bake_meshes = False
        pipeline.common_meshes_properties.bake_pivot_meshes = False
        pipeline.mesh_pipeline.combine_static_meshes = False
        pipeline.mesh_pipeline.build_nanite = True
        pipeline.mesh_pipeline.set_editor_property("collision", False)
        pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
        pipeline.mesh_pipeline.generate_lightmap_u_vs = False
        pipeline.material_pipeline.import_materials = False
        pipeline.material_pipeline.texture_pipeline.import_textures = False
        scene_pipeline = unreal.InterchangeGenericLevelPipeline()
        scene_pipeline.scene_hierarchy_type = unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
        parameters = unreal.ImportAssetParameters()
        parameters.is_automated = True
        parameters.replace_existing = False
        parameters.import_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
        parameters.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name()), unreal.SoftObjectPath(scene_pipeline.get_path_name())]
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        before = {_path(actor) for actor in _actors()}
        assert manager.import_scene(destination, manager.create_source_data(cfg["source_path"]), parameters)
        imported_actors = [actor for actor in _actors() if _path(actor) not in before]
    groups = defaultdict(list)
    slots_by_mesh = {}
    for actor in imported_actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component or not component.static_mesh:
            continue
        mesh = component.static_mesh
        mesh_path = _path(mesh)
        if mesh_path not in slots_by_mesh:
            slots = []
            for index, slot in enumerate(mesh.get_editor_property("static_materials")):
                slot_name = str(slot.material_slot_name)
                assert slot_name in materials, (mesh_path, slot_name)
                mesh.set_material(index, materials[slot_name])
                slots.append(_path(materials[slot_name]))
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            body.set_editor_property("double_sided_geometry", True)
            assert EAL.save_loaded_asset(mesh)
            slots_by_mesh[mesh_path] = slots
        # UE 5.5 does not expose get_component_transform() to Python.  These
        # Interchange scene nodes are StaticMeshActors, so the actor transform
        # is the authoritative world transform (same contract used elsewhere
        # in this project for packed landmark assembly).
        transform = actor.get_actor_transform()
        rotation = transform.rotation.rotator()
        groups[mesh_path].append({
            "location": [transform.translation.x, transform.translation.y, transform.translation.z],
            "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "scale": [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z],
        })
    assert len(groups) == cfg["glb_referenced_mesh_count"], (len(groups), cfg["glb_referenced_mesh_count"])
    assert sum(map(len, groups.values())) == cfg["glb_mesh_instance_count"]
    assert EAL.save_directory(destination, only_if_is_dirty=False, recursive=True)
    scene = {
        "source": cfg["source_path"],
        "import_offset_roll": 0.0,
        "mesh_count": len(groups),
        "instance_count": sum(map(len, groups.values())),
        "groups": [
            {"mesh": mesh_path, "materials": slots_by_mesh[mesh_path], "instances": groups[mesh_path]}
            for mesh_path in sorted(groups)
        ],
    }
    checkpoint.write_text(json.dumps(scene, indent=2), encoding="utf-8")
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in imported_actors:
        subsystem.destroy_actor(actor)
    return scene


def _rebuild_blueprint(cfg, report, scene):
    blueprint = EAL.load_asset(report["blueprint"])
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root = next(handle for handle in handles if library.is_root_component(subsystem.k2_find_subobject_data_from_handle(handle)))
    remove = []
    for handle in handles:
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            remove.append(handle)
        elif isinstance(component, unreal.SceneComponent) and component.get_name().startswith("BuildingOrientation"):
            remove.append(handle)
    for handle in remove:
        assert subsystem.delete_subobject(root, handle, blueprint) == 1
    manifest = []
    for index, group in enumerate(scene["groups"]):
        params = unreal.AddNewSubobjectParams(
            parent_handle=root,
            new_class=unreal.HierarchicalInstancedStaticMeshComponent,
            blueprint_context=blueprint,
            conform_transform_to_parent=False,
        )
        handle, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, unreal.Text(_component_name(index, group["mesh"])))
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        mesh = EAL.load_asset(group["mesh"])
        component.set_static_mesh(mesh)
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("BlockAll")
        component.set_editor_property("cast_shadow", True)
        for material_index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(material_index, slot.material_interface)
        transforms = [_transform(item) for item in group["instances"]]
        component.add_instances(transforms, False, True, False)
        assert component.get_instance_count() == len(transforms)
        manifest.append({"component": component.get_name(), "mesh": group["mesh"], "instances": len(transforms)})
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return manifest


def main():
    cfg = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))[0]
    assert cfg["import_offset_roll"] == 0.0
    report_path = ROOT / (cfg["name"] + "-import.json")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    tuning_path = ROOT / (cfg["name"] + "-tuning.json")
    tuning = json.loads(tuning_path.read_text(encoding="utf-8"))
    checkpoint = ROOT / "Guidemen-upright-scene.json"
    scene = json.loads(checkpoint.read_text(encoding="utf-8")) if checkpoint.exists() else None
    if scene is None:
        destination = cfg["destination"] + "/Meshes/UprightSource"
        recoverable = EAL.does_directory_exist(destination) and any(
            actor.get_component_by_class(unreal.StaticMeshComponent)
            and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(destination + "/")
            for actor in _actors()
        )
        if not recoverable:
            assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
            unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        scene = _import(cfg, _material_assets(cfg, tuning), checkpoint)
    manifest = _rebuild_blueprint(cfg, report, scene)
    report["rollback_scene"] = report["scene"]
    report["scene"] = scene
    report["hism_component_count"] = len(manifest)
    report["instance_count"] = sum(item["instances"] for item in manifest)
    report["component_manifest"] = manifest
    report["canonical_import_offset_roll"] = 0.0
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    tuning["changed_meshes"] = [group["mesh"] for group in scene["groups"]]
    tuning["upright_reimport"] = True
    tuning_path.write_text(json.dumps(tuning, indent=2), encoding="utf-8")
    blueprint = EAL.load_asset(report["blueprint"])
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(blueprint.generated_class(), unreal.Vector())
    actor.set_actor_label("Preview_" + cfg["name"])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, report["preview_level"])
    result = {
        "passed": True,
        "blueprint": report["blueprint"],
        "mesh_count": len(manifest),
        "instance_count": report["instance_count"],
        "canonical_mesh_root": cfg["destination"] + "/Meshes/UprightSource",
        "rollback_mesh_root": cfg["destination"] + "/Meshes/Source",
        "import_offset_roll": 0.0,
    }
    (ROOT / "Guidemen-upright-reimport.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("V5_GUIDEMEN_UPRIGHT_REIMPORT_COMPLETE", json.dumps(result))


if __name__ == "__main__":
    main()
