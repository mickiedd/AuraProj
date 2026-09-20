"""Import the supplied Guidemen high-poly GLB into AuraProj via Interchange.

Run this file inside the Unreal Editor with Python Remote Execution enabled:
    python Scripts/remote_run.py Scripts/ImportGuidemen.py

The requested nested source layout is accepted when present; the current
workspace also contains the same source as a flat file in Raw3DModels.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve()
assert PROJECT_ROOT == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"

REQUESTED_SOURCE = Path("C:/Works/Raw3DModels/Guidemen/_HighPoly/_UE5/_4K.glb")
FLAT_SOURCE = Path("C:/Works/Raw3DModels/Guidemen_HighPoly_UE5_4K.glb")
SOURCE = next((path for path in (REQUESTED_SOURCE, FLAT_SOURCE) if path.is_file()), None)
assert SOURCE is not None, (REQUESTED_SOURCE, FLAT_SOURCE)

DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen"
REPORT_DIR = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
) / "RawModelImport"
REPORT_PATH = REPORT_DIR / "Guidemen.json"

assert not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION), (
    "Refusing to overwrite existing content: " + DESTINATION
)

pipeline = unreal.InterchangeGenericAssetsPipeline()
pipeline.asset_name = "SM_Guidemen"
pipeline.import_offset_uniform_scale = 1.0
# --- import memory guard ---------------------------------------------------------------
# A bake on a shared-mesh instanced source writes every node out as its own mesh. Guidemen
# would have materialised ~148 GB that way and crashed the editor with an out-of-memory
# fatal error. See Scripts/ImportPreflight.py for the whole project's risk table.
def _import_guard(path, bake, budget_gb=24.0):
    import json as _json, struct as _struct
    with open(path, 'rb') as _handle:
        _magic, _, _length = _struct.unpack('<III', _handle.read(12))
        _doc = None
        while _handle.tell() < _length:
            _chunk_length, _chunk_type = _struct.unpack('<II', _handle.read(8))
            _payload = _handle.read(_chunk_length)
            if _chunk_type == 0x4E4F534A and _doc is None:
                _doc = _json.loads(_payload.decode('utf-8'))
    _unique = 0
    for _mesh in _doc.get('meshes', []):
        for _primitive in _mesh.get('primitives', []):
            if 'indices' in _primitive:
                _unique += _doc['accessors'][_primitive['indices']]['count'] // 3
    _baked = 0
    for _node in _doc.get('nodes', []):
        if 'mesh' in _node:
            for _primitive in _doc['meshes'][_node['mesh']].get('primitives', []):
                if 'indices' in _primitive:
                    _baked += _doc['accessors'][_primitive['indices']]['count'] // 3
    _materialised = _baked if bake else _unique
    _gb = _materialised * 220.0 / (1024 ** 3)
    _nodes = len([_node for _node in _doc.get('nodes', []) if 'mesh' in _node])
    _geoms = max(1, len(_doc.get('meshes', [])))
    assert _gb <= budget_gb, (
        'refusing import of {}: bake_meshes={} would materialise {:,} triangles (~{:.1f} GB) '
        'across {} nodes and {} geometries, over the {:.0f} GB budget. Use bake_meshes=False '
        'and keep instancing, or split the source. See Scripts/ImportPreflight.py'.format(
            path, bake, _materialised, _gb, _nodes, _geoms, budget_gb))
    print('IMPORT_GUARD ok: {:,} unique / {:,} materialised triangles (~{:.2f} GB), '
          'bake={}'.format(_unique, _materialised, _gb, bake))

_import_guard(str(SOURCE), True)
# --- end import memory guard ------------------------------------------------------------
pipeline.common_meshes_properties.bake_meshes = True
pipeline.mesh_pipeline.combine_static_meshes = True
pipeline.mesh_pipeline.build_nanite = True
pipeline.mesh_pipeline.set_editor_property("collision", False)
pipeline.mesh_pipeline.generate_lightmap_u_vs = False

params = unreal.ImportAssetParameters()
params.is_automated = True
params.replace_existing = False
params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]

manager = unreal.InterchangeManager.get_interchange_manager_scripted()
objects = manager.import_asset(
    DESTINATION,
    manager.create_source_data(str(SOURCE)),
    params,
)
assert objects, "Import returned no objects"

meshes = [obj for obj in objects if isinstance(obj, unreal.StaticMesh)]
assert len(meshes) == 1, [obj.get_path_name() for obj in objects]
mesh = meshes[0]
assert all(slot.material_interface for slot in mesh.static_materials), (
    "Imported mesh contains an unassigned material slot"
)
assert mesh.get_editor_property("nanite_settings").enabled, "Nanite was not enabled"

assert unreal.EditorAssetLibrary.save_directory(DESTINATION), "Save failed"

bounds = mesh.get_bounds()
materials = [
    slot.material_interface.get_path_name()
    for slot in mesh.static_materials
    if slot.material_interface
]
asset_paths = [obj.get_path_name() for obj in objects]
result = {
    "requested_source": str(REQUESTED_SOURCE),
    "resolved_source": str(SOURCE),
    "destination": DESTINATION,
    "mesh": mesh.get_path_name(),
    "size_cm": [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2],
    "origin_cm": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
    "nanite": mesh.get_editor_property("nanite_settings").enabled,
    "material_slots": len(materials),
    "materials": materials,
    "asset_count": len(asset_paths),
    "assets": asset_paths,
    "collision": "disabled for separate gameplay collision authoring",
    "normal_tangent_policy": "recompute normals/tangents with MikkTSpace",
}
REPORT_DIR.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("IMPORT_VALIDATED", json.dumps(result))
