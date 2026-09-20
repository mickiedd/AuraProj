"""Import the V2 GreatNorthGate high-detail GLB as Nanite static meshes.

Source: C:/Works/Raw3DModels/V2/GreatNorthGate_UE5_HighDetail.glb (156 MB).

Structure of the source, decoded from the glTF JSON before writing this script:
  * 6 meshes / 5 PBR materials / 15 textures.
  * 5 detail meshes (Stone, Plaster, Woodwork, Sign, RoofTrim) sit at identity in
    the shared building space.  That space is authored Z-up (UE convention): the
    stone base spans Z 0..9.7 m, the roof trim Z 13.2..17.9 m, and the gate sign
    sits at Y ~= -3 m on the front face.  Interchange assumes Y-up, so an
    import_offset_rotation of -90 roll is required to stand it back up (the same
    correction the GreatWestGate import uses).
  * The roof is a single 6,000,000-triangle / 3,000,000-vertex unit tile mesh
    (1.0 x 1.0 x 0.098 m) instanced by 9 nodes with individual transforms.
    Interchange collapses those 9 instances into that one mesh asset and bakes
    their combined placement into its vertices, so the imported Roof_Upper_Front
    asset already contains all nine roof sections in building space (measured
    1943.4 x 1006.3 x 716.0 cm, matching the union of the 9 node transforms).
    Nanite keeps the resulting ~54M rendered triangles performant.

Meshes are imported separately (combine_static_meshes=False) so the 6M-triangle
roof tile stays its own asset, exactly as authored.  The five detail meshes and
the assembled roof all end up at identity in one shared building space.

The existing /Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate folder
holds a different, older model and is deliberately left untouched.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
SOURCE = Path("C:/Works/Raw3DModels/V2/GreatNorthGate_UE5_HighDetail.glb")
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_HighDetail"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatNorthGate_HighDetail.json"

IMPORT_ROLL = -90.0
RESET_EXISTING = True
ROOF_TILE_MESH = "Roof_Upper_Front"
EXPECTED_MESH_COUNT = 6
EXPECTED_TRIANGLES = {
    "SM_GreatNorthGate_Stone_Node": 33264,
    "SM_GreatNorthGate_Plaster_Node": 24,
    "SM_GreatNorthGate_Woodwork_Node": 7356,
    "SM_GreatNorthGate_Sign_Node": 12,
    "SM_GreatNorthGate_RoofTrim_Node": 48,
    "Roof_Upper_Front": 6000000,
}

assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT_ROOT.resolve(), "Wrong Unreal project"
assert SOURCE.exists(), SOURCE


def discover_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEST], True)
    meshes = []
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            meshes.append(asset)
    return sorted(meshes, key=lambda mesh: mesh.get_name())


def mesh_report(mesh):
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    try:
        vertex_count = int(unreal.get_editor_subsystem(
            unreal.StaticMeshEditorSubsystem).get_number_verts(mesh, 0))
    except Exception:
        vertex_count = None
    return {
        "name": mesh.get_name(),
        "path": mesh.get_path_name(),
        # get_num_triangles reports the Nanite fallback for very dense meshes, so
        # the source triangle count is asserted separately below.
        "num_triangles_reported": int(mesh.get_num_triangles(0)),
        "num_verts_reported": vertex_count,
        "expected_source_triangles": EXPECTED_TRIANGLES.get(mesh.get_name()),
        "lod_count": int(mesh.get_num_lods()),
        "nanite_enabled": bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")),
        "bounds_min": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "bounds_max": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "size_cm": [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0],
        "material_slots": [slot.material_interface.get_path_name() if slot.material_interface else None
                           for slot in mesh.static_materials],
    }


def import_meshes():
    existing = discover_meshes()
    if existing and not RESET_EXISTING:
        assert len(existing) == EXPECTED_MESH_COUNT, \
            "Partial GreatNorthGate HighDetail import found ({}); refusing to mix assets".format(len(existing))
        return existing, False, []

    if existing:
        assert unreal.EditorAssetLibrary.delete_directory(DEST), "Failed to clear previous import"
        unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False)

    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = "SM_GreatNorthGate"
    pipeline.import_offset_rotation = unreal.Rotator(roll=IMPORT_ROLL)
    pipeline.import_offset_uniform_scale = 1.0
    # bake_meshes must stay True: with it False Interchange ignores
    # import_offset_rotation entirely and the Z-up source lands on its side.
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
    pipeline.mesh_pipeline.combine_static_meshes = False
    # Nanite is required: the roof tile alone is 6M triangles and is instanced 9
    # times.  Keeping meshes separate avoids the combined-Nanite build path that
    # proved unstable on the GreatWestGate import.
    pipeline.mesh_pipeline.build_nanite = True
    # No collision is generated here, matching the GreatWestGate/Guidemen imports.
    # The roof asset is 54M triangles, so building collision is deliberately left
    # as a separate gameplay step rather than generated blindly.
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(DEST, manager.create_source_data(str(SOURCE)), params)
    assert objects, "Import returned no GreatNorthGate HighDetail objects"
    assert unreal.EditorAssetLibrary.save_directory(DEST), "GreatNorthGate HighDetail asset save failed"
    meshes = discover_meshes()
    assert len(meshes) == EXPECTED_MESH_COUNT, \
        "Expected {} imported mesh parts, found {}".format(EXPECTED_MESH_COUNT, len(meshes))
    return meshes, True, [obj.get_path_name() for obj in objects]


meshes, imported, imported_objects = import_meshes()
reports = [mesh_report(mesh) for mesh in meshes]

assert all(item["nanite_enabled"] for item in reports), "Nanite not enabled on every imported mesh"
assert all(slot for item in reports for slot in item["material_slots"]), \
    "Unassigned GreatNorthGate HighDetail material"

# The five detail meshes must share the upright building space: ~24.6 m wide (X),
# ~8.0 m deep (Y) and ~18.1 m tall (Z).  A Y/Z swap here means the import is
# lying on its side and IMPORT_ROLL has the wrong sign.
detail = [item for item in reports if item["name"] != ROOF_TILE_MESH]
union_min = [min(item["bounds_min"][axis] for item in detail) for axis in range(3)]
union_max = [max(item["bounds_max"][axis] for item in detail) for axis in range(3)]
detail_size = [union_max[axis] - union_min[axis] for axis in range(3)]

roof = [item for item in reports if item["name"] == ROOF_TILE_MESH][0]
roof_verts = roof["num_verts_reported"]

# Upright means the gate is taller (Z) than it is deep (Y): ~24.6 x 8.0 x 18.1 m.
# If Y and Z are the other way round the import is lying on its side and
# IMPORT_ROLL has the wrong sign.
assert detail_size[2] > detail_size[1], \
    "GreatNorthGate HighDetail import is not upright: depth {} vs height {}".format(
        detail_size[1], detail_size[2])

import_report = {
    "source": str(SOURCE),
    "source_bytes": SOURCE.stat().st_size,
    "destination": DEST,
    "imported_this_run": imported,
    "imported_objects": imported_objects,
    "import_roll": IMPORT_ROLL,
    "mesh_count": len(meshes),
    "nanite": True,
    "source_total_triangles": sum(EXPECTED_TRIANGLES.values()),
    "roof_instance_count": 9,
    "rendered_triangles_with_instances": sum(EXPECTED_TRIANGLES.values()) + EXPECTED_TRIANGLES[ROOF_TILE_MESH] * 8,
    "roof_tile_vertex_count": roof_verts,
    "detail_union_bounds_cm": {"min": union_min, "max": union_max, "size": detail_size},
    "materials": sorted({slot for item in reports for slot in item["material_slots"]}),
    "meshes": reports,
}
IMPORT_REPORT.parent.mkdir(parents=True, exist_ok=True)
IMPORT_REPORT.write_text(json.dumps(import_report, indent=2), encoding="utf-8")

print("GREAT_NORTH_GATE_HIGHDETAIL_IMPORT", json.dumps({
    "imported_this_run": imported,
    "import_roll": IMPORT_ROLL,
    "mesh_count": len(meshes),
    "detail_union_size_cm": [round(v, 1) for v in detail_size],
    "roof_verts": roof_verts,
    "materials": len(import_report["materials"]),
}))
for item in reports:
    print("  {:<34} nanite={} size_cm={} verts={} tris_reported={} (source {})".format(
        item["name"], item["nanite_enabled"],
        [round(v, 1) for v in item["size_cm"]],
        item["num_verts_reported"], item["num_triangles_reported"],
        item["expected_source_triangles"]))
