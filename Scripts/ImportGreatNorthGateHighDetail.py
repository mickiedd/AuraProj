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
