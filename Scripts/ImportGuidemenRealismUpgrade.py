"""Import the ten aligned GLB pieces from the Guidemen realism upgrade pack.

Run through Unreal Editor Python Remote Execution:
    python Scripts/remote_run.py Scripts/ImportGuidemenRealismUpgrade.py
"""
import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve()
assert PROJECT_ROOT == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"

SOURCE_DIR = Path("C:/Works/Raw3DModels/Guidemen_UE5_RealismUpgrade_20260911")
ZIP_PATH = Path("C:/Works/Raw3DModels/Guidemen_UE5_RealismUpgrade.zip")
MANIFEST_SOURCE = SOURCE_DIR / "Guidemen_Realism_Manifest.json"
DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen/RealismUpgrade"
REPORT_DIR = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
) / "RawModelImport"
REPORT_PATH = REPORT_DIR / "GuidemenRealismUpgrade.json"

assert SOURCE_DIR.is_dir(), SOURCE_DIR
assert MANIFEST_SOURCE.is_file(), MANIFEST_SOURCE
assert ZIP_PATH.is_file(), ZIP_PATH
assert not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION), (
    "Refusing to overwrite existing content: " + DESTINATION
)

source_manifest = json.loads(MANIFEST_SOURCE.read_text(encoding="utf-8"))
piece_specs = source_manifest["pieces"]
assert len(piece_specs) == 10, len(piece_specs)

zip_hash = hashlib.sha256()
with ZIP_PATH.open("rb") as stream:
    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
        zip_hash.update(chunk)

manager = unreal.InterchangeManager.get_interchange_manager_scripted()
imported = []

for index, spec in enumerate(piece_specs, start=1):
    source = SOURCE_DIR / spec["file"]
    assert source.is_file(), source
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = source.stem
    pipeline.import_offset_uniform_scale = 1.0
    # The two high-density facade meshes exceed this editor's Nanite build
    # memory budget. Keep their full source geometry and leave Nanite off;
    # the smaller detail pieces still receive Nanite data.
    nanite_requested = spec["triangles"] <= 2000000
    pipeline.common_meshes_properties.bake_meshes = False
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = nanite_requested
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = False
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]

    objects = manager.import_asset(
        DESTINATION,
        manager.create_source_data(str(source)),
        params,
    )
    assert objects, "Import returned no objects for " + source.name
    meshes = [obj for obj in objects if isinstance(obj, unreal.StaticMesh)]
    assert len(meshes) == 1, [obj.get_path_name() for obj in objects]
    mesh = meshes[0]
    nanite_enabled = mesh.get_editor_property("nanite_settings").enabled
    assert nanite_enabled == nanite_requested, (
        "Unexpected Nanite state for {}: expected {}, got {}".format(
            mesh.get_path_name(), nanite_requested, nanite_enabled
        )
    )
    assert all(slot.material_interface for slot in mesh.static_materials), (
        "Unassigned material slot in " + mesh.get_path_name()
    )
    bounds = mesh.get_bounds()
    imported.append(
        {
            "source": str(source),
            "mesh": mesh.get_path_name(),
            "size_cm": [
                bounds.box_extent.x * 2,
                bounds.box_extent.y * 2,
                bounds.box_extent.z * 2,
            ],
            "origin_cm": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
            "nanite_requested": nanite_requested,
            "nanite": nanite_enabled,
            "material_slots": len(mesh.static_materials),
            "materials": [
                slot.material_interface.get_path_name()
                for slot in mesh.static_materials
                if slot.material_interface
            ],
            "asset_count": len(objects),
            "assets": [obj.get_path_name() for obj in objects],
        }
    )
    unreal.log("GUIDEMEN_UPGRADE_IMPORTED {}/10 {}".format(index, mesh.get_path_name()))

assert unreal.EditorAssetLibrary.save_directory(DESTINATION), "Save failed"
result = {
    "package": "Guidemen_UE5_RealismUpgrade.zip",
    "package_sha256": zip_hash.hexdigest(),
    "manifest": str(MANIFEST_SOURCE),
    "destination": DESTINATION,
    "piece_count": len(imported),
    "combined_vertex_count": source_manifest["combined_vertex_count"],
    "combined_triangle_count": source_manifest["combined_triangle_count"],
    "previous_triangle_count": source_manifest["previous_triangle_count"],
    "triangle_multiplier_vs_previous": source_manifest["triangle_multiplier_vs_previous"],
    "collision": "disabled for separate gameplay collision authoring",
    "pieces": imported,
}
REPORT_DIR.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GUIDEMEN_UPGRADE_IMPORTED", json.dumps(result))
