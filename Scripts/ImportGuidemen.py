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
