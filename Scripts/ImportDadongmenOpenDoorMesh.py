"""Import and bind the Dadongmen source variant with the baked closed gate removed."""
import json
from pathlib import Path
import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
SOURCE = str((ROOT / "Dadongmen_GreatEastGate_UE5/Meshes/SM_Dadongmen_OpenDoor_LOD0.gltf").resolve())
MESH_DEST = DEST + "/Meshes/SM_Dadongmen_OpenDoor_LOD0"
BP_PATH = DEST + "/BP_Dadongmen_V4"
PREVIEW_PATH = DEST + "/L_Dadongmen_V4_Preview"
REPORT_PATH = ROOT / "Dadongmen-open-door-import.json"


def asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before importing the open-door mesh: " + str(dirty)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    eal = unreal.EditorAssetLibrary
    if not eal.does_directory_exist(MESH_DEST):
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.import_offset_rotation = unreal.Rotator(roll=-90)
        pipeline.common_meshes_properties.bake_meshes = False
        pipeline.common_meshes_properties.bake_pivot_meshes = False
        pipeline.mesh_pipeline.combine_static_meshes = False
        pipeline.mesh_pipeline.build_nanite = True
        pipeline.mesh_pipeline.set_editor_property("collision", False)
        pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
        pipeline.mesh_pipeline.generate_lightmap_u_vs = False
        pipeline.material_pipeline.import_materials = False
        pipeline.material_pipeline.texture_pipeline.import_textures = False
        scene = unreal.InterchangeGenericLevelPipeline()
        scene.scene_hierarchy_type = unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
        params = unreal.ImportAssetParameters()
        params.is_automated, params.replace_existing = True, False
        params.import_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
        params.override_pipelines = [
            unreal.SoftObjectPath(pipeline.get_path_name()),
            unreal.SoftObjectPath(scene.get_path_name()),
        ]
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        before = {a.get_path_name() for a in actors.get_all_level_actors()}
        assert manager.import_scene(MESH_DEST, manager.create_source_data(SOURCE), params), SOURCE
        imported = [a for a in actors.get_all_level_actors() if a.get_path_name() not in before]
        meshes = []
        for actor in imported:
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if component and component.static_mesh and component.static_mesh not in meshes:
                meshes.append(component.static_mesh)
        assert len(meshes) == 1, [m.get_path_name() for m in meshes]
        mesh = meshes[0]
        tuned = {
            "M_Dadongmen_Stone": asset(DEST + "/Materials/M_Dadongmen_Stone_ReferenceTuned"),
            "M_Dadongmen_Plaster": asset(DEST + "/Materials/M_Dadongmen_Plaster_ReferenceTuned"),
            "M_Dadongmen_Wood": asset(DEST + "/Materials/M_Dadongmen_Wood_ReferenceTuned"),
            "M_Dadongmen_RoofClay": asset(DEST + "/Materials/M_Dadongmen_RoofClay_ReferenceTuned"),
            "M_Dadongmen_Dirt": asset(DEST + "/Materials/M_Dadongmen_Dirt_ReferenceTuned"),
            "M_Dadongmen_Water": asset(DEST + "/Materials/M_Dadongmen_Water_ReferenceTuned"),
            "M_Dadongmen_Vegetation": asset(DEST + "/Materials/M_Dadongmen_Vegetation_ReferenceTuned"),
        }
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.material_slot_name)
            slots.append({"index": index, "slot": slot_name, "before": slot.material_interface.get_path_name() if slot.material_interface else ""})
            if slot_name in tuned:
                mesh.set_material(index, tuned[slot_name])
        mesh.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        )
        assert eal.save_loaded_asset(mesh)
        eal.save_directory(MESH_DEST, only_if_is_dirty=False, recursive=True)
        for actor in imported:
            actors.destroy_actor(actor)
    else:
        mesh = asset(MESH_DEST + "/SM_Dadongmen_OpenDoor_LOD0")

    # Rebind the visible base component while preserving the authored collision
    # proxy and every existing Blueprint detail component.
    bp = asset(BP_PATH)
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    changed = 0
    for handle in ss.k2_gather_subobject_data_for_blueprint(bp):
        data = ss.k2_find_subobject_data_from_handle(handle)
        component = lib.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        path = component.static_mesh.get_path_name()
        if "SM_Dadongmen_LOD0.SM_Dadongmen_LOD0" not in path:
            continue
        component.set_static_mesh(mesh)
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(index, slot.material_interface)
        changed += 1
    assert changed == 1, changed
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, PREVIEW_PATH)
    report = {
        "source": SOURCE,
        "mesh": mesh.get_path_name(),
        "blueprint": BP_PATH,
        "preview_level": PREVIEW_PATH,
        "rebound_components": changed,
        "closed_source_leaves_removed": True,
        "collision_preserved": True,
        "source_mesh_rollback": DEST + "/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0",
        "material_slots": slots,
    }
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("DADONGMEN_OPEN_DOOR_IMPORTED", json.dumps(report))


if __name__ == "__main__":
    main()
