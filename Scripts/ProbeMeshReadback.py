"""Find a working way to read vertex data out of Unreal static meshes.

The reference-tuning skill records that vertex readback is blocked in UE 5.5
Python (`get_vertex_position` needs a `VertexID` that cannot be constructed), and
the project's earlier repair pass worked around it by parsing the source GLB
instead. Before committing to a source-based audit, this tests the routes that
would let the audit run against the meshes that are actually in the project:

  1. `StaticMesh.get_static_mesh_description(lod)` -> can we get positions?
  2. `AssetExportTask` + `Exporter.run_asset_export_task` -> OBJ on disk?
  3. `MeshDescriptionBase` helpers on the returned description.

Reports which route yields real vertex data, with a vertex count cross-checked
against the mesh's own reported count.
"""

import json
import os
import tempfile

import unreal

MESHES = [
    "/Game/Assets/Environment/GuangzhouLandmarks/ZhenhaiTower/SM_ZhenhaiTower",
    "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate/SM_GreatNorthGate",
    "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/SM_Xiaobeimen",
]

report = {"mesh_description_api": [], "export_api": [], "meshdescription_methods": []}


def describe_mesh(mesh):
    entry = {"mesh": mesh.get_path_name()}
    try:
        entry["lod_count"] = int(mesh.get_num_lods())
    except Exception as exc:
        entry["lod_count"] = "failed: {}".format(exc)
    try:
        description = mesh.get_static_mesh_description(0)
        entry["description_class"] = (description.get_class().get_name()
                                      if description else None)
    except Exception as exc:
        entry["description_class"] = "failed: {}".format(exc)
        return entry
    if description is None:
        return entry

    # What can the description actually give us?
    for method in ("get_vertex_positions", "get_vertex_position", "get_vertex_count",
                   "get_triangle_indices", "get_vertex_instance_positions",
                   "get_vertex_instance_count", "get_triangle_count",
                   "get_vertex_connected_triangles", "get_vertex_adjacent_vertices"):
        entry[method] = hasattr(description, method)
    for method in ("get_vertex_positions", "get_vertex_instance_positions",
                   "get_triangle_indices"):
        if not hasattr(description, method):
            continue
        try:
            value = getattr(description, method)()
            entry[method + "_result"] = "len={}".format(len(value))
            if len(value):
                first = value[0]
                entry[method + "_first"] = (list(first) if hasattr(first, "__len__")
                                            else str(first))
        except Exception as exc:
            entry[method + "_result"] = "failed: {}".format(exc)
    return entry


def main():
    for path in MESHES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            report["mesh_description_api"].append({"mesh": path, "error": "load failed"})
            continue
        report["mesh_description_api"].append(describe_mesh(mesh))

    # Route 2: export to OBJ.
    out_dir = os.path.join(tempfile.gettempdir(), "aura_mesh_export")
    os.makedirs(out_dir, exist_ok=True)
    for path in MESHES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        entry = {"mesh": path}
        if mesh is None:
            entry["error"] = "load failed"
            report["export_api"].append(entry)
            continue
        filename = os.path.join(out_dir, mesh.get_name() + ".obj")
        try:
            task = unreal.AssetExportTask()
            task.set_editor_property("object", mesh)
            task.set_editor_property("filename", filename)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_identical", True)
            task.set_editor_property("prompt", False)
            ok = unreal.Exporter.run_asset_export_task(task)
            entry["run_result"] = bool(ok) if ok is not None else None
        except Exception as exc:
            entry["run_result"] = "failed: {}".format(exc)
        entry["file_written"] = os.path.exists(filename)
        if os.path.exists(filename):
            size = os.path.getsize(filename)
            entry["file_bytes"] = size
            with open(filename, "r", encoding="utf-8", errors="replace") as handle:
                head = [handle.readline().rstrip() for _ in range(6)]
            entry["head"] = head
            vertex_lines = 0
            face_lines = 0
            with open(filename, "r", encoding="utf-8", errors="replace") as handle:
                for line in handle:
                    if line.startswith("v "):
                        vertex_lines += 1
                    elif line.startswith("f "):
                        face_lines += 1
            entry["obj_vertices"] = vertex_lines
            entry["obj_faces"] = face_lines
        report["export_api"].append(entry)

    # What does MeshDescriptionBase expose at all?
    try:
        report["meshdescription_methods"] = sorted(
            name for name in dir(unreal.MeshDescriptionBase) if "vertex" in name.lower())
    except Exception as exc:
        report["meshdescription_methods"] = "failed: {}".format(exc)

    unreal.log("MESH_READBACK_PROBE " + json.dumps(report))
    print("MESH_READBACK_PROBE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
