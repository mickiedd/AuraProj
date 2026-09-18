"""Import every Raw3DModels/V5 archive as an independent HISM Blueprint.

Run ``PrepareV5Buildings.py`` first.  Call ``import_package(0..2)`` from the
running editor.  The importer preserves every authored GLB mesh-node transform,
groups repeated geometry into HISM components, enables Nanite, uses complex
collision, and makes all surfaces/two-sided collision robust against visible
back-face hollows.
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
MEL = unreal.MaterialEditingLibrary

FALLBACK_COLORS = {
    "M_DarkInterior": (0.018, 0.014, 0.011, 1.0),
    "M_FadedRedWood": (0.30, 0.075, 0.035, 1.0),
    "M_DefaultStone": (0.42, 0.39, 0.33, 1.0),
    "M_Plaster": (0.64, 0.58, 0.48, 1.0),
    "M_Iron": (0.06, 0.045, 0.03, 1.0),
    "M_Plaque_Wuxianmen": (0.56, 0.37, 0.18, 1.0),
}


def _write(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2), encoding="utf-8")


def _path(value) -> str:
    return value.get_path_name() if value else ""


def _constant(material, color, x=-700, y=0):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*color))
    return node


def _scalar(material, value, x=-700, y=0):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def _sample(material, texture, x, y, sampler):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    return node


def _import_textures(cfg, report):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    result = {}
    sources = sorted({relative for maps in cfg["material_maps"].values() for relative in maps.values()})
    for relative in sources:
        source = ROOT / relative
        asset_name = source.stem
        asset_path = cfg["destination"] + "/Textures/" + asset_name
        texture = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if texture is None:
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = cfg["destination"] + "/Textures"
            task.destination_name = asset_name
            task.automated = True
            task.save = True
            task.replace_existing = False
            tools.import_asset_tasks([task])
            assert task.imported_object_paths, str(source)
            texture = EAL.load_asset(task.imported_object_paths[0])
        assert isinstance(texture, unreal.Texture2D), asset_path
        channel = next(channel for maps in cfg["material_maps"].values() for channel, item in maps.items() if item == relative)
        normal = channel == "Normal"
        scalar = channel in ("Roughness", "MetallicRoughness", "AO")
        texture.set_editor_property("srgb", not (normal or scalar))
        texture.set_editor_property(
            "compression_settings",
            unreal.TextureCompressionSettings.TC_NORMALMAP if normal else
            unreal.TextureCompressionSettings.TC_MASKS if scalar else
            unreal.TextureCompressionSettings.TC_DEFAULT,
        )
        assert EAL.save_loaded_asset(texture)
        result[relative] = texture
    report["textures"] = {relative: _path(texture) for relative, texture in result.items()}
    return result


def _create_materials(cfg, textures, report):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    result = {}
    for material_name in cfg["glb_material_names"]:
        asset_path = cfg["destination"] + "/Materials/" + material_name
        material = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if material is None:
            material = tools.create_asset(material_name, cfg["destination"] + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
            assert isinstance(material, unreal.Material), asset_path
            material.set_editor_property("two_sided", True)
            material.set_editor_property("used_with_nanite", True)
            maps = cfg["material_maps"].get(material_name, {})
            if "BaseColor" in maps:
                sample = _sample(material, textures[maps["BaseColor"]], -850, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
                assert MEL.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
            else:
                color = _constant(material, FALLBACK_COLORS.get(material_name, (0.35, 0.31, 0.25, 1.0)))
                assert MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
            if "Normal" in maps:
                sample = _sample(material, textures[maps["Normal"]], -850, 240, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
                assert MEL.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
            if "Roughness" in maps:
                sample = _sample(material, textures[maps["Roughness"]], -850, 480, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                assert MEL.connect_material_property(sample, "R", unreal.MaterialProperty.MP_ROUGHNESS)
            elif "MetallicRoughness" in maps:
                sample = _sample(material, textures[maps["MetallicRoughness"]], -850, 480, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                assert MEL.connect_material_property(sample, "G", unreal.MaterialProperty.MP_ROUGHNESS)
                assert MEL.connect_material_property(sample, "B", unreal.MaterialProperty.MP_METALLIC)
            else:
                roughness = _scalar(material, 0.78, -850, 480)
                assert MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
            if "AO" in maps:
                sample = _sample(material, textures[maps["AO"]], -850, 720, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                assert MEL.connect_material_property(sample, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
            if material_name == "M_Iron" and "MetallicRoughness" not in maps:
                metallic = _scalar(material, 0.88, -600, 720)
                assert MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
            MEL.layout_material_expressions(material)
            MEL.recompile_material(material)
            assert EAL.save_loaded_asset(material)
        assert material.get_editor_property("two_sided"), asset_path
        result[material_name] = material
    report["materials"] = {name: _path(material) for name, material in result.items()}
    return result


def _all_level_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _import_scene(cfg, materials):
    destination = cfg["destination"] + "/Meshes/Source"
    existing = [
        actor for actor in _all_level_actors()
        if (actor.get_component_by_class(unreal.StaticMeshComponent)
            and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(destination + "/"))
    ]
    if EAL.does_directory_exist(destination):
        assert existing, "Uncheckpointed partial destination has no recoverable live scene: " + destination
        actors = existing
        print("V5_RECOVERING_LIVE_SCENE", cfg["name"], len(actors))
    else:
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.import_offset_rotation = unreal.Rotator(roll=cfg.get("import_offset_roll", -90.0))
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
        parameters.override_pipelines = [
            unreal.SoftObjectPath(pipeline.get_path_name()),
            unreal.SoftObjectPath(scene_pipeline.get_path_name()),
        ]
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        before = {_path(actor) for actor in _all_level_actors()}
        source = manager.create_source_data(cfg["source_path"])
        assert manager.import_scene(destination, source, parameters), cfg["source_path"]
        actors = [actor for actor in _all_level_actors() if _path(actor) not in before]
    mesh_paths = set()
    groups = defaultdict(list)
    material_slots = {}
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component is None or component.static_mesh is None:
            continue
        mesh = component.static_mesh
        mesh_path = _path(mesh)
        if mesh_path not in mesh_paths:
            slots = []
            for index, slot in enumerate(mesh.get_editor_property("static_materials")):
                slot_name = str(slot.material_slot_name)
                assert slot_name in materials, (mesh_path, slot_name, sorted(materials))
                mesh.set_material(index, materials[slot_name])
                slots.append(_path(materials[slot_name]))
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            body.set_editor_property("double_sided_geometry", True)
            assert EAL.save_loaded_asset(mesh)
            mesh_paths.add(mesh_path)
            material_slots[mesh_path] = slots
        transform = actor.get_actor_transform()
        rotation = transform.rotation.rotator()
        groups[mesh_path].append({
            "location": [transform.translation.x, transform.translation.y, transform.translation.z],
            "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "scale": [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z],
        })
    assert len(mesh_paths) == cfg["glb_referenced_mesh_count"], (cfg["name"], len(mesh_paths), cfg["glb_referenced_mesh_count"])
    assert sum(map(len, groups.values())) == cfg["glb_mesh_instance_count"], (cfg["name"], sum(map(len, groups.values())), cfg["glb_mesh_instance_count"])
    assert EAL.save_directory(destination, only_if_is_dirty=False, recursive=True)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors:
        actor_subsystem.destroy_actor(actor)
    return {
        "source": cfg["source_path"],
        "mesh_count": len(mesh_paths),
        "instance_count": sum(map(len, groups.values())),
        "groups": [
            {"mesh": mesh_path, "materials": material_slots[mesh_path], "instances": groups[mesh_path]}
            for mesh_path in sorted(groups)
        ],
    }


def _transform(record):
    return unreal.Transform(
        location=unreal.Vector(*record["location"]),
        rotation=unreal.Rotator(pitch=record["rotation"][0], yaw=record["rotation"][1], roll=record["rotation"][2]),
        scale=unreal.Vector(*record["scale"]),
    )


def _safe_component_name(index, mesh_path):
    stem = mesh_path.rsplit("/", 1)[-1].split(".", 1)[0]
    return "HISM_{:03d}_{}".format(index, re.sub(r"[^A-Za-z0-9_]", "_", stem)[:48])


def _create_blueprint(cfg, scene, report):
    blueprint_path = cfg["destination"] + "/BP_" + cfg["name"]
    assert not EAL.does_asset_exist(blueprint_path), blueprint_path
    blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(blueprint_path, unreal.Actor)
    assert blueprint, blueprint_path
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root = next((handle for handle in handles if library.is_root_component(subsystem.k2_find_subobject_data_from_handle(handle))), None)
    if root is None:
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.SceneComponent, blueprint_context=blueprint)
        root, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(root), str(reason)
    root_component = library.get_object(subsystem.k2_find_subobject_data_from_handle(root))
    root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    root_rotation = cfg.get("blueprint_root_rotation", [0.0, 0.0, 0.0])
    geometry_parent = root
    if any(abs(value) > 0.001 for value in root_rotation):
        params = unreal.AddNewSubobjectParams(
            parent_handle=root,
            new_class=unreal.SceneComponent,
            blueprint_context=blueprint,
            conform_transform_to_parent=False,
        )
        geometry_parent, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(geometry_parent), str(reason)
        subsystem.rename_subobject(geometry_parent, unreal.Text("BuildingOrientation"))
        orientation = library.get_object(subsystem.k2_find_subobject_data_from_handle(geometry_parent))
        orientation.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        orientation.set_editor_property(
            "relative_rotation",
            unreal.Rotator(pitch=root_rotation[0], yaw=root_rotation[1], roll=root_rotation[2]),
        )
    component_manifest = []
    for index, group in enumerate(scene["groups"]):
        params = unreal.AddNewSubobjectParams(
            parent_handle=geometry_parent,
            new_class=unreal.HierarchicalInstancedStaticMeshComponent,
            blueprint_context=blueprint,
            conform_transform_to_parent=False,
        )
        handle, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, unreal.Text(_safe_component_name(index, group["mesh"])))
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        mesh = EAL.load_asset(group["mesh"])
        component.set_static_mesh(mesh)
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("BlockAll")
        component.set_editor_property("cast_shadow", True)
        for material_index, material_path in enumerate(group["materials"]):
            component.set_material(material_index, EAL.load_asset(material_path))
        transforms = [_transform(record) for record in group["instances"]]
        component.add_instances(transforms, False, True, False)
        assert component.get_instance_count() == len(transforms)
        component_manifest.append({
            "component": component.get_name(),
            "mesh": group["mesh"],
            "materials": group["materials"],
            "instance_count": len(transforms),
        })
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    report["blueprint"] = blueprint_path
    report["parent"] = "/Script/Engine.Actor"
    report["hism_component_count"] = len(component_manifest)
    report["instance_count"] = sum(item["instance_count"] for item in component_manifest)
    report["component_manifest"] = component_manifest
    return blueprint


def import_package(index: int) -> None:
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
    cfg = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))[index]
    assert cfg["destination"].startswith("/Game/Assets/Environment/GuangzhouLandmarks/V5/")
    report_path = ROOT / (cfg["name"] + "-import.json")
    if report_path.exists():
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if report.get("complete"):
            print("V5_ALREADY_COMPLETE", cfg["name"])
            return
    else:
        report = {"name": cfg["name"], "archive": cfg["archive"], "archive_sha256": cfg["archive_sha256"]}
    scene_destination = cfg["destination"] + "/Meshes/Source"
    recoverable_scene = EAL.does_directory_exist(scene_destination) and any(
        actor.get_component_by_class(unreal.StaticMeshComponent)
        and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh
        and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(scene_destination + "/")
        for actor in _all_level_actors()
    )
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if not recoverable_scene:
        assert not dirty_maps, "Save the current map before V5 import: " + str([_path(value) for value in dirty_maps])
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    else:
        print("V5_RECOVERY_MAP_ACCEPTED", cfg["name"], [_path(value) for value in dirty_maps])
    textures = _import_textures(cfg, report)
    materials = _create_materials(cfg, textures, report)
    _write(report_path, report)
    scene = report.get("scene")
    if scene is None:
        scene = _import_scene(cfg, materials)
        report["scene"] = scene
        _write(report_path, report)
    blueprint = _create_blueprint(cfg, scene, report)
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(blueprint.generated_class(), unreal.Vector())
    assert actor
    actor.set_actor_label("Preview_" + cfg["name"])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    report["preview_level"] = cfg["destination"] + "/L_" + cfg["name"] + "_Preview"
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, report["preview_level"])
    report["complete"] = True
    report["intactness_guards"] = {
        "all_glb_meshes_imported": True,
        "all_glb_instances_wrapped": True,
        "all_material_slots_bound": True,
        "two_sided_materials": True,
        "double_sided_collision": True,
        "complex_collision": True,
    }
    _write(report_path, report)
    print("V5_PACKAGE_COMPLETE", json.dumps({
        "name": cfg["name"],
        "blueprint": report["blueprint"],
        "meshes": scene["mesh_count"],
        "components": report["hism_component_count"],
        "instances": report["instance_count"],
    }))


if __name__ == "__main__":
    for package_index in range(3):
        cfg = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))[package_index]
        report_path = ROOT / (cfg["name"] + "-import.json")
        if not report_path.exists() or not json.loads(report_path.read_text(encoding="utf-8")).get("complete"):
            import_package(package_index)
            break
    else:
        print("V5_ALL_PACKAGES_COMPLETE")
