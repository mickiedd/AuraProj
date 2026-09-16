"""Inspect Zhengximen V4 primary mesh materials and Blueprint component templates."""
import json
import unreal

DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen"
MESH_PATH = DEST + "/Meshes/SM_Zhengximen_LOD0/SM_Zhengximen_LOD0"
BP_PATH = DEST + "/BP_Zhengximen_V4"

def p(x):
    return x.get_path_name() if x else ""

def main():
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    print("Zhengximen mesh", p(mesh))
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        print("slot", i, str(slot.material_slot_name), p(slot.material_interface))
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    count = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if not library.is_component(data):
            continue
        component = library.get_object(data)
        if isinstance(component, unreal.StaticMeshComponent):
            count += 1
            mats = [p(component.get_material(i)) for i in range(component.get_num_materials())]
            print("component", component.get_name(), p(component.static_mesh), mats)
    print("component_count", count)

if __name__ == "__main__":
    main()
