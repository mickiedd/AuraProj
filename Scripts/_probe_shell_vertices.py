"""Read one roof shell's vertices to confirm which edge is high."""
import unreal

BP = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
shell = unreal.EditorAssetLibrary.load_asset(
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/Meshes/UprightSource/R1_RoofShellFront.R1_RoofShellFront")
print("SHELL_MESH", shell.get_path_name() if shell else None)
desc = shell.get_static_mesh_description(0)
print("VERTEX_COUNT", desc.get_vertex_count())
print("HAS_VertexID", hasattr(unreal, "VertexID"))
for attempt in ("int", "VertexID"):
    try:
        if attempt == "int":
            value = desc.get_vertex_position(0)
        else:
            vid = unreal.VertexID()
            vid.set_editor_property("id", 0)
            value = desc.get_vertex_position(vid)
        print("POS_OK", attempt, value)
        break
    except Exception as error:
        print("POS_FAIL", attempt, repr(error)[:140])
# fall back: vertex instance positions via triangle walk
try:
    print("TRI_COUNT", desc.get_triangle_count() if hasattr(desc, "get_triangle_count") else "n/a")
    vi = desc.get_triangle_vertex_instances(unreal.TriangleID()) if hasattr(unreal, "TriangleID") else None
    print("TRI_VI", vi)
except Exception as error:
    print("TRI_FAIL", repr(error)[:140])
