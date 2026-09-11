"""Import the supplied Xiaobeimen v4 repair and retarget the placed landmark.

The previous v3 mesh is intentionally kept in the project for rollback.  The
existing actor is updated in place, so its transform, label, and placement tags
remain unchanged while its StaticMeshComponent points at the repaired asset.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT_ROOT.resolve(), "Wrong Unreal project"

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen"
NEW_ASSET = DEST + "/SM_Xiaobeimen_GeometryFixed"
OLD_ASSET = DEST + "/SM_Xiaobeimen"
REPORT_PATH = PROJECT_ROOT / "Saved/RawModelImport/xiaobeimen-v4-reimport.json"
PLACEMENT_MANIFEST = PROJECT_ROOT / "Saved/RawModelImport/guangzhou-landmark-placement.json"
ROOT = Path(
    "C:/Works/Raw3DModels/Extracted/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed"
)
SOURCE_OBJ = ROOT / "SM_Xiaobeimen_Combined_GeometryFixed.obj"
SOURCE_ZIP = Path("C:/Works/Raw3DModels/Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed.zip")
ACTOR_TAG = "ImportedGuangzhouLandmark"
ACTOR_LABEL = "GuangzhouLandmark_Xiaobeimen"

assert SOURCE_OBJ.exists(), "Run PrepareXiaobeimenV4Obj.py before this import"
assert SOURCE_ZIP.exists(), "Supplied v4 ZIP is missing"
if unreal.EditorAssetLibrary.does_asset_exist(NEW_ASSET):
    # A previous run may have completed the import and actor mutation before
    # failing while saving the map.  Resume from the saved mesh in that case.
    mesh = unreal.EditorAssetLibrary.load_asset(NEW_ASSET)
    assert isinstance(mesh, unreal.StaticMesh), NEW_ASSET
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE_OBJ)
    task.destination_path = DEST
    task.destination_name = "SM_Xiaobeimen_GeometryFixed"
    task.automated = True
    task.save = True
    task.replace_existing = False

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.automated_import_should_detect_type = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    options.static_mesh_import_data.generate_lightmap_u_vs = False
    options.static_mesh_import_data.import_uniform_scale = 1.0
    options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    options.static_mesh_import_data.build_nanite = True
    task.options = options
    tools.import_asset_tasks([task])

    meshes = [obj for obj in task.get_objects() if isinstance(obj, unreal.StaticMesh)]
    assert len(meshes) == 1, "Expected one imported v4 static mesh, got {}".format(len(meshes))
    mesh = meshes[0]
    assert mesh.get_path_name().startswith(NEW_ASSET + "."), mesh.get_path_name()

# Reuse the already-authored v3 PBR materials.  v4 changes geometry only and
# keeps the same material slot names and texture set.
material_paths = {
    "Stone": DEST + "/Materials/M_StoneWall",
    "StoneLight": DEST + "/Materials/M_StoneLight",
    "StoneDark": DEST + "/Materials/M_StoneDark",
    "WoodDark": DEST + "/Materials/M_WoodDark",
    "Metal": DEST + "/Materials/M_MetalDark",
    "StoneVar": DEST + "/Materials/M_StoneWall",
    "Plaster": DEST + "/Materials/M_Plaster",
    "WoodRed": DEST + "/Materials/M_WoodRed",
    "RoofGreen": DEST + "/Materials/M_RoofTileGreen",
    "RoofTile": DEST + "/Materials/M_RoofTileGreen",
    "RoofRidge": DEST + "/Materials/M_StoneDark",
    "StoneRoad": DEST + "/Materials/M_StoneRoad",
}
slot_names = []
for index, slot in enumerate(mesh.static_materials):
    slot_name = str(slot.material_slot_name)
    slot_names.append(slot_name)
    assert slot_name in material_paths, "Unexpected v4 material slot: " + slot_name
    material = unreal.EditorAssetLibrary.load_asset(material_paths[slot_name])
    assert material, "Missing existing Xiaobeimen material: " + material_paths[slot_name]
    material.set_editor_property("used_with_nanite", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    mesh.set_material(index, material)

nanite = mesh.get_editor_property("nanite_settings")
assert nanite.enabled, "Imported v4 mesh did not build with Nanite enabled"

loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, "Could not load showcase level"
placement = json.loads(PLACEMENT_MANIFEST.read_text(encoding="utf-8"))
placement_entry = next(item for item in placement["actors"] if item["label"] == ACTOR_LABEL)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
matches = [actor for actor in actors if actor.get_actor_label() == ACTOR_LABEL and ACTOR_TAG in [str(tag) for tag in actor.tags]]
assert len(matches) == 1, "Expected one placed Xiaobeimen actor, got {}".format(len(matches))
actor = matches[0]
component = actor.get_component_by_class(unreal.StaticMeshComponent)
assert component and component.static_mesh, "Placed Xiaobeimen actor has no static mesh component"
assert component.static_mesh.get_path_name() in (
    OLD_ASSET + ".SM_Xiaobeimen",
    NEW_ASSET + ".SM_Xiaobeimen_GeometryFixed",
), component.static_mesh.get_path_name()

old_location = actor.get_actor_location()
old_rotation = actor.get_actor_rotation()
old_scale = actor.get_actor_scale3d()
component.set_editor_property("static_mesh", mesh)
# The repaired v4 active Y bounds are narrower and have a different local
# origin than v3.  Recompute the actor location from the original placement
# center/ground so the landmark remains in the same world footprint.
local_bounds = mesh.get_bounds()
target_x, target_y = placement_entry["center_xy"]
target_ground = placement_entry["ground_z"]
new_location = unreal.Vector(
    target_x - local_bounds.origin.x * old_scale.x,
    target_y - local_bounds.origin.y * old_scale.y,
    target_ground - (local_bounds.origin.z - local_bounds.box_extent.z) * old_scale.z,
)
actor.set_actor_location(new_location, False, True)
actor.set_actor_rotation(old_rotation, False)
actor.set_actor_scale3d(old_scale)
actor.modify()
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.EditorAssetLibrary.save_directory(DEST)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Failed to save updated showcase level"

origin, extent = actor.get_actor_bounds(False)
bounds = {
    "min": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
    "max": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
}
mesh_bounds = mesh.get_bounds()
report = {
    "source_zip": str(SOURCE_ZIP),
    "source_obj": str(SOURCE_OBJ),
    "level": LEVEL_PATH,
    "actor": actor.get_name(),
    "label": actor.get_actor_label(),
    "old_mesh": OLD_ASSET,
    "new_mesh": NEW_ASSET,
    "previous_location": [old_location.x, old_location.y, old_location.z],
    "location": [new_location.x, new_location.y, new_location.z],
    "placement_center_xy": [target_x, target_y],
    "ground_z": target_ground,
    "rotation": [old_rotation.roll, old_rotation.pitch, old_rotation.yaw],
    "scale": [old_scale.x, old_scale.y, old_scale.z],
    "actor_bounds": bounds,
    "mesh_size_cm": [mesh_bounds.box_extent.x * 2.0, mesh_bounds.box_extent.y * 2.0, mesh_bounds.box_extent.z * 2.0],
    "material_slots": slot_names,
    "nanite": bool(nanite.enabled),
    "passed": True,
}
REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("XIAOBEIMEN_V4_REIMPORT", json.dumps(report))
