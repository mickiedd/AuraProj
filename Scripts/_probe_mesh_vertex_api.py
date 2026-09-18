"""Find a Python route to static mesh vertex positions."""
import unreal

for cls_name in ("StaticMeshEditorSubsystem", "EditorStaticMeshLibrary", "StaticMesh"):
    cls = getattr(unreal, cls_name, None)
    if cls is None:
        print("PROBE_MISSING", cls_name)
        continue
    hits = [n for n in dir(cls)
            if any(t in n.lower() for t in ("vertex", "description", "lod", "positions", "vertices", "mesh_data"))]
    print("PROBE_API", cls_name, hits)

print("PROBE_CLASSES", [n for n in dir(unreal) if "MeshDescription" in n or "StaticMeshDescription" in n])
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
print("PROBE_SUB", [n for n in dir(sub) if "description" in n.lower() or "vertex" in n.lower()])
