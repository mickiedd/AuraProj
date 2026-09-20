"""Import the DabeiMen Great North Gate GLB as Nanite static meshes.

Source: C:/Works/Raw3DModels/V2/GreatNorthGate_DabeiMen_UE5_Nanite_6M.glb (192 MB).

Structure decoded offline from the glTF JSON (Saved/RawModelImport/dabeimen-source-analysis.json):

  * 5 meshes / 5 PBR materials / 15 textures, 6,005,624 triangles total.
      GNG_STONE     3,420 tris   M_Stone_AgedGuangzhou
      GNG_PLASTER     216 tris   M_Plaster_Weathered
      GNG_WOOD      8,952 tris   M_Wood_DarkTimber
      GNG_ROOF  5,993,024 tris   M_Roof_GrayClayTile   <- the "6M" Nanite roof
      GNG_PLAQUE       12 tris   M_Plaque_DabeiMen
  * No instancing: every mesh is referenced by exactly one node, and all five
    nodes sit at identity under a single "world" root.  The five meshes therefore
    already live in one shared building space - no manual reassembly needed.
  * The material names map one-to-one onto the design sheet's material block
    (青砖石材 / 风化灰泥 / 木构件 / 灰瓦).  In particular M_Roof_GrayClayTile
    replaces the green glazed roof of the older landmark, which is exactly what
    the design reference calls for.

Authored space is Z-up, the same convention as the previous V2 model:
    stone base      Z  0.000 .. 13.575   (ground plane sits at Z = 0)
    plaster         Z 12.825 .. 16.375
    wood            Z 12.550 .. 21.830
    roof            Z 16.710 .. 24.500
    plaque          Y -3.830 .. -3.610   (front face, negative Y)
    overall         68.0 x 12.1 x 24.5 m

Interchange assumes Y-up, so import_offset_rotation roll -90 is required to stand
it back up, and bake_meshes MUST stay True or Interchange silently ignores that
offset rotation (verified on the previous GreatNorthGate import, where a False
value produced byte-identical, still-sideways bounds).
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
SOURCE = Path("C:/Works/Raw3DModels/V2/GreatNorthGate_DabeiMen_UE5_Nanite_6M.glb")
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_DabeiMen"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatNorthGate_DabeiMen.json"

IMPORT_ROLL = -90.0
RESET_EXISTING = True

EXPECTED_MESH_COUNT = 5
# Source triangle counts straight from the glTF index accessors.  These are the
# authoritative fidelity figures; mesh.get_num_triangles() reports the Nanite
# fallback instead and must not be used to judge whether the import is complete.
EXPECTED_SOURCE_TRIANGLES = {
    "GNG_STONE": 3420,
    "GNG_PLASTER": 216,
    "GNG_WOOD": 8952,
    "GNG_ROOF": 5993024,
    "GNG_PLAQUE": 12,
}
# Expected upright footprint in cm: 68.0 m wide (X) x 12.1 m deep (Y) x 24.5 m tall (Z).
EXPECTED_SIZE_CM = {"x": 6800.0, "y": 1210.0, "z": 2450.0}
SIZE_TOLERANCE_CM = 60.0
ROOF_MESH = "GNG_ROOF"

assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT_ROOT.resolve(), \
    "Wrong Unreal project"
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


def source_key(mesh_name):
    """Map an imported asset name back onto its glTF source mesh key."""
    for key in EXPECTED_SOURCE_TRIANGLES:
        if key in mesh_name:
            return key
    return None


def mesh_report(mesh):
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    try:
        vertex_count = int(unreal.get_editor_subsystem(
            unreal.StaticMeshEditorSubsystem).get_number_verts(mesh, 0))
    except Exception:
        vertex_count = None
    key = source_key(mesh.get_name())
    return {
        "name": mesh.get_name(),
        "source_key": key,
        "path": mesh.get_path_name(),
        "num_triangles_reported": int(mesh.get_num_triangles(0)),
        "num_verts_reported": vertex_count,
        "expected_source_triangles": EXPECTED_SOURCE_TRIANGLES.get(key) if key else None,
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
            "Partial DabeiMen import found ({}); refusing to mix assets".format(len(existing))
        return existing, False, []

    if existing:
        assert unreal.EditorAssetLibrary.delete_directory(DEST), "Failed to clear previous DabeiMen import"
        unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False)

    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = "SM_GreatNorthGate_DabeiMen"
    pipeline.import_offset_rotation = unreal.Rotator(roll=IMPORT_ROLL)
    pipeline.import_offset_uniform_scale = 1.0
    # bake_meshes must stay True: with it False Interchange ignores
    # import_offset_rotation entirely and this Z-up source lands on its side.
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
    # Nanite is required: the roof alone is 6M triangles.
    pipeline.mesh_pipeline.build_nanite = True
    # No collision generated here, matching the other landmark imports.
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(DEST, manager.create_source_data(str(SOURCE)), params)
    assert objects, "Import returned no DabeiMen objects"
    assert unreal.EditorAssetLibrary.save_directory(DEST), "DabeiMen asset save failed"
    meshes = discover_meshes()
    assert len(meshes) == EXPECTED_MESH_COUNT, \
        "Expected {} imported mesh parts, found {}".format(EXPECTED_MESH_COUNT, len(meshes))
    return meshes, True, [obj.get_path_name() for obj in objects]


meshes, imported, imported_objects = import_meshes()
reports = [mesh_report(mesh) for mesh in meshes]

assert all(item["nanite_enabled"] for item in reports), "Nanite not enabled on every imported mesh"
assert all(item["source_key"] for item in reports), \
    "Unrecognised mesh name(s): {}".format([item["name"] for item in reports if not item["source_key"]])
assert sorted(item["source_key"] for item in reports) == sorted(EXPECTED_SOURCE_TRIANGLES), \
    "Imported mesh set does not match the source: {}".format(sorted(item["source_key"] for item in reports))
assert all(slot for item in reports for slot in item["material_slots"]), \
    "Unassigned DabeiMen material"

union_min = [min(item["bounds_min"][axis] for item in reports) for axis in range(3)]
union_max = [max(item["bounds_max"][axis] for item in reports) for axis in range(3)]
union_size = [union_max[axis] - union_min[axis] for axis in range(3)]

# Upright test: 68.0 m wide, 12.1 m deep, 24.5 m tall.  If the import is lying on
# its side, height and depth swap and the Y/Z ratio inverts.
assert union_size[0] > union_size[2] > union_size[1], \
    "DabeiMen import is not upright: size_cm {}".format([round(v, 1) for v in union_size])
for axis, expected in enumerate([EXPECTED_SIZE_CM["x"], EXPECTED_SIZE_CM["y"], EXPECTED_SIZE_CM["z"]]):
    assert abs(union_size[axis] - expected) <= SIZE_TOLERANCE_CM, \
        "DabeiMen axis {} is {} cm, expected {} cm".format(axis, round(union_size[axis], 1), expected)

roof = [item for item in reports if item["source_key"] == ROOF_MESH][0]

import_report = {
    "source": str(SOURCE),
    "source_bytes": SOURCE.stat().st_size,
    "destination": DEST,
    "imported_this_run": imported,
    "imported_objects": imported_objects,
    "import_roll": IMPORT_ROLL,
    "mesh_count": len(meshes),
    "nanite": True,
    "source_total_triangles": sum(EXPECTED_SOURCE_TRIANGLES.values()),
    "roof_source_triangles": EXPECTED_SOURCE_TRIANGLES[ROOF_MESH],
    "union_bounds_cm": {"min": union_min, "max": union_max, "size": union_size},
    "materials": sorted({slot for item in reports for slot in item["material_slots"]}),
    "meshes": reports,
}
IMPORT_REPORT.parent.mkdir(parents=True, exist_ok=True)
IMPORT_REPORT.write_text(json.dumps(import_report, indent=2), encoding="utf-8")

print("GREAT_NORTH_GATE_DABEIMEN_IMPORT", json.dumps({
    "imported_this_run": imported,
    "import_roll": IMPORT_ROLL,
    "mesh_count": len(meshes),
    "union_size_cm": [round(v, 1) for v in union_size],
    "source_total_triangles": import_report["source_total_triangles"],
    "materials": len(import_report["materials"]),
}))
for item in reports:
    print("  {:<22} nanite={} size_cm={} verts={} tris_reported={} (source {})".format(
        item["name"], item["nanite_enabled"],
        [round(v, 1) for v in item["size_cm"]],
        item["num_verts_reported"], item["num_triangles_reported"],
        item["expected_source_triangles"]))
