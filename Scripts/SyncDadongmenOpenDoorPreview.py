"""Refresh the saved Dadongmen preview actor after replacing its Blueprint mesh."""
import unreal

DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
PREVIEW = DEST + "/L_Dadongmen_V4_Preview"
OPEN = DEST + "/Meshes/SM_Dadongmen_OpenDoor_LOD0/SM_Dadongmen_OpenDoor_LOD0"
DOOR_MATERIAL = DEST + "/Materials/M_Dadongmen_DoorWood_AAA"
IRON_MATERIAL = DEST + "/Materials/M_Dadongmen_Iron_AAA"
STONE_MATERIAL = DEST + "/Materials/M_Dadongmen_Stone_AAA"
APPROACH_MATERIAL = DEST + "/Materials/M_Dadongmen_Approach_ReferenceTuned"
WATER_MATERIAL = DEST + "/Materials/M_Dadongmen_Water_AAA"

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level(PREVIEW)
open_mesh = unreal.EditorAssetLibrary.load_asset(OPEN)
assert open_mesh, OPEN
door_material = unreal.EditorAssetLibrary.load_asset(DOOR_MATERIAL)
assert door_material, DOOR_MATERIAL
iron_material = unreal.EditorAssetLibrary.load_asset(IRON_MATERIAL)
assert iron_material, IRON_MATERIAL
stone_material = unreal.EditorAssetLibrary.load_asset(STONE_MATERIAL)
approach_material = unreal.EditorAssetLibrary.load_asset(APPROACH_MATERIAL)
water_material = unreal.EditorAssetLibrary.load_asset(WATER_MATERIAL)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
changed = 0
door_changed = 0
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() != "Preview_Dadongmen_V4":
        continue
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.static_mesh and "SM_Dadongmen_LOD0.SM_Dadongmen_LOD0" in component.static_mesh.get_path_name():
            component.set_static_mesh(open_mesh)
            for index, slot in enumerate(open_mesh.get_editor_property("static_materials")):
                if slot.material_interface:
                    component.set_material(index, slot.material_interface)
            changed += 1
        comp_name = component.get_name()
        if comp_name.startswith("Detail_Dadongmen_Door_"):
            if any(token in comp_name for token in ("Threshold", "Interior_Floor")):
                component.set_material(0, stone_material)
            elif "Approach_Path" in comp_name:
                component.set_material(0, approach_material)
            elif "Approach_Water" in comp_name:
                component.set_material(0, water_material)
            elif "IronStrap" in comp_name or "Stud" in comp_name:
                component.set_material(0, iron_material)
            else:
                component.set_material(0, door_material)
                door_changed += 1
    break
assert changed <= 1 and door_changed >= 1, (changed, door_changed)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world, PREVIEW)
print("DADONGMEN_PREVIEW_OPEN_MESH_SYNCED", {"base_mesh": changed, "door_details": door_changed})
