import unreal

BP_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4"
MESH_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0"

bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
print("BP", BP_PATH, type(bp).__name__)
print("MESH", MESH_PATH, type(mesh).__name__)
if mesh:
    bounds = mesh.get_bounds()
    print("MESH_BOUNDS", bounds.origin, bounds.box_extent, "sphere", bounds.sphere_radius)
    print("MESH_SLOTS", [(str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else None) for s in mesh.get_editor_property("static_materials")])

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
seen = set()
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    component = library.get_object(data)
    if not component or component.get_name() in seen:
        continue
    seen.add(component.get_name())
    if not isinstance(component, unreal.StaticMeshComponent):
        print("COMPONENT", component.get_name(), type(component).__name__)
        continue
    static_mesh = component.static_mesh
    try:
        collision = component.get_editor_property("collision_profile_name")
    except Exception:
        collision = "<unavailable>"
    print(
        "COMPONENT",
        component.get_name(),
        "MESH", static_mesh.get_path_name() if static_mesh else None,
        "LOC", component.get_editor_property("relative_location"),
        "ROT", component.get_editor_property("relative_rotation"),
        "SCALE", component.get_editor_property("relative_scale3d"),
        "COLLISION", collision,
        "VISIBLE", component.get_editor_property("visible"),
    )
    if static_mesh:
        bounds = static_mesh.get_bounds()
        print("COMPONENT_MESH_BOUNDS", component.get_name(), bounds.origin, bounds.box_extent)
