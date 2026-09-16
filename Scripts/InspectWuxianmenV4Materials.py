"""Inspect Wuxianmen V4 primary Blueprint component meshes and materials."""
import unreal

DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen"
BP_PATH = DEST + "/BP_Wuxianmen_V4"

def p(x):
    return x.get_path_name() if x else ""

def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    count = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if not library.is_component(data):
            continue
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent):
            continue
        count += 1
        print("component", component.get_name(), p(component.static_mesh))
        for i in range(component.get_num_materials()):
            print(" material", i, p(component.get_material(i)))
    print("component_count", count)

if __name__ == "__main__":
    main()
