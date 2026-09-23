"""Import the Wenmingmen package and create its independent Actor Blueprint.

Run after PrepareWenmingmen.py, from a UE 5.5 editor or Python commandlet.
The eight GLB mesh nodes become eight components on BP_Wenmingmen. Source
textures stay in ContentSource and are imported as native Texture2D assets.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "ContentSource/GuangzhouLandmarks/Wenmingmen"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen"
REPORT = PROJECT / "Saved/RawModelImport/Wenmingmen-import.json"
IMPORT_MANIFEST = SOURCE / "unreal-import-manifest.json"
PARTS = ("stone", "limestone", "roof", "wood", "iron", "water", "foliage", "plaque_inscription")
STRUCTURAL = {"stone", "limestone", "wood", "iron"}
NANITE = {"stone", "limestone", "roof", "wood", "iron"}
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def path(obj):
    return obj.get_path_name() if obj else ""


def save_report(data):
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(data, indent=2, ensure_ascii=False) + "\n"
    REPORT.write_text(serialized, encoding="utf-8")
    if data.get("validated"):
        IMPORT_MANIFEST.write_text(serialized, encoding="utf-8")


def import_textures(data):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    textures = {}
    for source in sorted((SOURCE / "textures").iterdir()):
        if source.suffix.lower() not in (".jpg", ".png"):
            continue
        name = source.stem
        asset_path = DEST + "/Textures/" + name
        texture = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if texture is None:
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = DEST + "/Textures"
            task.destination_name = name
            task.automated = True
            task.save = True
            task.replace_existing = False
            tools.import_asset_tasks([task])
            assert task.imported_object_paths, str(source)
            texture = EAL.load_asset(task.imported_object_paths[0])
        assert isinstance(texture, unreal.Texture2D), asset_path
        normal = name.endswith("_Normal")
        orm = name.endswith("_ORM")
        texture.set_editor_property("srgb", not (normal or orm))
        texture.set_editor_property("compression_settings", (
            unreal.TextureCompressionSettings.TC_NORMALMAP if normal else
            unreal.TextureCompressionSettings.TC_MASKS if orm else
            unreal.TextureCompressionSettings.TC_DEFAULT
        ))
        if normal:
            texture.set_editor_property("flip_green_channel", True)
        assert EAL.save_loaded_asset(texture)
        textures[name] = texture
    assert len(textures) == 25, sorted(textures)
    data["textures"] = {name: path(tex) for name, tex in textures.items()}
    save_report(data)
    return textures


def sample(material, texture, sampler, y):
    expression = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -600, y)
    expression.set_editor_property("texture", texture)
    expression.set_editor_property("sampler_type", sampler)
    return expression


def create_materials(textures, data):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    result = {}
    for part in PARTS:
        name = "M_" + ("inscription_decal" if part == "plaque_inscription" else part)
        asset_path = DEST + "/Materials/" + name
        material = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if material is None:
            material = tools.create_asset(name, DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
            assert isinstance(material, unreal.Material), asset_path
            material.set_editor_property("two_sided", part in ("roof", "foliage", "plaque_inscription"))
            material.set_editor_property("used_with_nanite", part in NANITE)
            if part == "plaque_inscription":
                material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
                base = sample(material, textures["plaque_inscription_BaseColor"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 0)
                assert MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
                assert MEL.connect_material_property(base, "A", unreal.MaterialProperty.MP_OPACITY)
            else:
                base = sample(material, textures[part + "_BaseColor"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 0)
                normal = sample(material, textures[part + "_Normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 250)
                orm = sample(material, textures[part + "_ORM"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 500)
                assert MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
                assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
                assert MEL.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
                assert MEL.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
                assert MEL.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
            MEL.layout_material_expressions(material)
            MEL.recompile_material(material)
            assert EAL.save_loaded_asset(material)
        result[name] = material
    data["materials"] = {name: path(mat) for name, mat in result.items()}
    save_report(data)
    return result


def import_meshes(materials, data):
    destination = DEST + "/Meshes"
    assert not EAL.does_directory_exist(destination), "Mesh import already exists; inspect the partial import before retrying"
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Save the current level before importing"
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.import_offset_rotation = unreal.Rotator(roll=-90.0)
    pipeline.common_meshes_properties.bake_meshes = False
    pipeline.common_meshes_properties.bake_pivot_meshes = False
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = False
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    pipeline.material_pipeline.import_materials = False
    pipeline.material_pipeline.texture_pipeline.import_textures = False
    scene_pipeline = unreal.InterchangeGenericLevelPipeline()
    scene_pipeline.scene_hierarchy_type = unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = False
    params.import_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    params.override_pipelines = [
        unreal.SoftObjectPath(pipeline.get_path_name()),
        unreal.SoftObjectPath(scene_pipeline.get_path_name()),
    ]
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    before = {path(actor) for actor in actor_subsystem.get_all_level_actors()}
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = manager.create_source_data(str(SOURCE / "Wenmingmen_HighDetail.glb"))
    assert manager.import_scene(destination, source, params), "GLB scene import failed"
    actors = [actor for actor in actor_subsystem.get_all_level_actors() if path(actor) not in before]
    records = []
    found = set()
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component is None or component.static_mesh is None:
            continue
        mesh = component.static_mesh
        part = next((name for name in PARTS if mesh.get_name().lower().endswith("wm_" + name)), None)
        assert part, path(mesh)
        assert part not in found, (part, path(mesh))
        found.add(part)
        expected = "M_" + ("inscription_decal" if part == "plaque_inscription" else part)
        slots = mesh.get_editor_property("static_materials")
        assert len(slots) == 1 and str(slots[0].material_slot_name) == expected, (part, [str(s.material_slot_name) for s in slots])
        mesh.set_material(0, materials[expected])
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", part in NANITE)
        mesh.set_editor_property("nanite_settings", nanite)
        if part in STRUCTURAL:
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            body.set_editor_property("double_sided_geometry", True)
        assert EAL.save_loaded_asset(mesh)
        transform = actor.get_actor_transform()
        rot = transform.rotation.rotator()
        records.append({
            "part": part,
            "mesh": path(mesh),
            "material": path(materials[expected]),
            "location": [transform.translation.x, transform.translation.y, transform.translation.z],
            "rotation": [rot.pitch, rot.yaw, rot.roll],
            "scale": [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z],
            "nanite": part in NANITE,
            "collision": part in STRUCTURAL,
        })
    assert found == set(PARTS), (sorted(found), PARTS)
    assert EAL.save_directory(destination, only_if_is_dirty=False, recursive=True)
    for actor in actors:
        actor_subsystem.destroy_actor(actor)
    records.sort(key=lambda row: PARTS.index(row["part"]))
    data["meshes"] = records
    save_report(data)
    return records


def create_blueprint(records, data):
    asset_path = DEST + "/BP_Wenmingmen"
    assert not EAL.does_asset_exist(asset_path), asset_path
    blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(asset_path, unreal.Actor)
    assert blueprint, asset_path
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root = next((h for h in handles if library.is_root_component(subsystem.k2_find_subobject_data_from_handle(h))), None)
    if root is None:
        root, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=handles[0], new_class=unreal.SceneComponent, blueprint_context=blueprint
        ))
        assert library.is_handle_valid(root), str(reason)
    root_component = library.get_object(subsystem.k2_find_subobject_data_from_handle(root))
    root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    for row in records:
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root,
            new_class=unreal.StaticMeshComponent,
            blueprint_context=blueprint,
            conform_transform_to_parent=False,
        ))
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, unreal.Text("SMC_" + row["part"]))
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        component.set_static_mesh(EAL.load_asset(row["mesh"]))
        component.set_material(0, EAL.load_asset(row["material"]))
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        component.set_editor_property("relative_location", unreal.Vector(*row["location"]))
        component.set_editor_property("relative_rotation", unreal.Rotator(
            pitch=row["rotation"][0], yaw=row["rotation"][1], roll=row["rotation"][2]
        ))
        component.set_editor_property("relative_scale3d", unreal.Vector(*row["scale"]))
        component.set_collision_profile_name("BlockAll" if row["collision"] else "NoCollision")
        component.set_editor_property("cast_shadow", row["part"] != "water")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    data["blueprint"] = asset_path
    save_report(data)
    return blueprint


def validate(blueprint, data):
    assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
    assert len(data["textures"]) == 25 and len(data["materials"]) == 8 and len(data["meshes"]) == 8
    for name, asset_path in data["textures"].items():
        texture = EAL.load_asset(asset_path)
        assert isinstance(texture, unreal.Texture2D), asset_path
        assert texture.get_editor_property("srgb") == (not name.endswith(("_Normal", "_ORM"))), name
        if name.endswith("_Normal"):
            assert texture.get_editor_property("flip_green_channel"), name
    for name, asset_path in data["materials"].items():
        assert isinstance(EAL.load_asset(asset_path), unreal.Material), (name, asset_path)
    for row in data["meshes"]:
        mesh = EAL.load_asset(row["mesh"])
        assert isinstance(mesh, unreal.StaticMesh) and mesh.get_num_sections(0) > 0, row["mesh"]
        assert path(mesh.get_material(0)) == row["material"], row["part"]
        assert bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")) == row["nanite"], row["part"]
        if row["collision"]:
            body = mesh.get_editor_property("body_setup")
            assert body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE, row["part"]
            assert body.get_editor_property("double_sided_geometry"), row["part"]
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    components = []
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        if isinstance(obj, unreal.StaticMeshComponent) and path(obj) not in seen:
            seen.add(path(obj))
            components.append(obj)
    assert len(components) == len(PARTS), [c.get_name() for c in components]
    by_part = {row["part"]: row for row in data["meshes"]}
    for component in components:
        part = component.get_name().split("SMC_", 1)[-1].split("_GEN_VARIABLE", 1)[0]
        assert part in by_part, component.get_name()
        row = by_part[part]
        assert path(component.static_mesh) == row["mesh"], part
        assert path(component.get_material(0)) == row["material"], part
        assert str(component.get_collision_profile_name()) == ("BlockAll" if row["collision"] else "NoCollision"), part
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector())
    assert actor, "Blueprint failed to spawn"
    try:
        _, extent = actor.get_actor_bounds(False)
        size = [extent.x * 2, extent.y * 2, extent.z * 2]
        assert 5000 < max(size) < 8500, size
        assert 1600 < size[2] < 3300, size
        assert min(size) > 1000, size
        data["bounds_cm"] = size
    finally:
        actor_subsystem.destroy_actor(actor)
    data["component_count"] = len(components)
    data["validated"] = True
    save_report(data)
    print("WENMINGMEN_IMPORT_COMPLETE", json.dumps({
        "blueprint": data["blueprint"], "components": len(components), "bounds_cm": data["bounds_cm"]
    }))


def main():
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
    assert (SOURCE / "Wenmingmen_HighDetail.glb").is_file()
    report_path = REPORT if REPORT.exists() else IMPORT_MANIFEST
    data = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else {}
    if data.get("validated") and EAL.does_asset_exist(data["blueprint"]):
        print("WENMINGMEN_ALREADY_COMPLETE", data["blueprint"])
        return
    textures = import_textures(data)
    materials = create_materials(textures, data)
    records = data.get("meshes") or import_meshes(materials, data)
    blueprint = EAL.load_asset(data["blueprint"]) if data.get("blueprint") else create_blueprint(records, data)
    validate(blueprint, data)


if __name__ == "__main__":
    main()
