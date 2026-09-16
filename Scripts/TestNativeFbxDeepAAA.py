"""Temporary native FBX smoke test; imports only into a disposable test folder."""
import unreal


def test(source, name):
    destination = "/Game/Temp_DeepAAA_FbxSmoke/" + name
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.automated, task.save, task.replace_existing = True, True, True
    ui = unreal.FbxImportUI()
    ui.import_mesh, ui.import_materials, ui.import_textures = True, False, False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.automated_import_should_detect_type = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.generate_lightmap_u_vs = False
    ui.static_mesh_import_data.auto_generate_collision = False
    task.options = ui
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_flag = unreal.SystemLibrary.get_console_variable_bool_value("Interchange.FeatureFlags.Import.FBX")
    unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        task.factory = unreal.FbxFactory()
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX " + str(int(old_flag)))
    objects = task.get_objects()
    print("NATIVE_FBX", name, "objects", [str(x.get_path_name()) for x in objects], "paths", task.imported_object_paths)


def main():
    test(r"C:/Git/AuraProj/Saved/RawModelImport/V4/Zhengximen_GreatWestGate_UE5/Meshes/SM_Zhengximen_LOD0.fbx", "known")
    test(r"C:/Git/AuraProj/Saved/RawModelImport/V4/Prepared/Zhengximen/SM_Zhengximen_LOD0.fbx", "prepared")
    test(r"C:/Git/AuraProj/Saved/RawModelImport/V4/DeepAAA/Wuxianmen_V4_DeepAAA.fbx", "deep")
    print("SMOKE_CLEANUP", unreal.EditorAssetLibrary.delete_directory("/Game/Temp_DeepAAA_FbxSmoke"))


main()
