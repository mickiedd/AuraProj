"""Transient probe: work out how the Zhengximen sources actually import.

The legacy AssetImportTask + FbxImportUI route produced no mesh at all (the
Interchange line appeared and then nothing), so this probes the project's proven
route instead - InterchangeManager.import_asset with an explicit
InterchangeGenericAssetsPipeline, as used by ImportGreatNorthGateHighDetail.py.

Two questions it answers, which cannot be settled by reading the package:
  1. Does the authored LOD0 FBX (UV0 + UV1 lightmap + UCX collision + sockets,
     centimetres) import, and what are its material slot names?
  2. Does the LOD0 GLB (trimesh, identity node transforms, Z-up positions,
     metres, NO materials) import, and is it upright or lying on its side?

Everything lands in a scratch folder that is deleted at the end.
"""

import json

import unreal

PACKAGE = ("/Volumes/M2/Works/AuraProj/Raw3DPacket/"
           "Zhengximen_GreatWestGate_UE5_Package/Zhengximen_GreatWestGate_UE5")
SCRATCH = "/Game/__ZhengximenImportProbe"

report = {}


def describe(objects):
    rows = []
    for obj in objects or []:
        path = obj.get_path_name() if obj else None
        if not path:
            continue
        asset = unreal.EditorAssetLibrary.load_asset(path.split(".")[0])
        row = {"path": path, "class": type(asset).__name__ if asset else None}
        if isinstance(asset, unreal.StaticMesh):
            bounds = asset.get_bounds()
            row["name"] = asset.get_name()
            row["triangles"] = int(asset.get_num_triangles(0))
            row["size_cm"] = [round(float(bounds.box_extent.x) * 2, 2),
                              round(float(bounds.box_extent.y) * 2, 2),
                              round(float(bounds.box_extent.z) * 2, 2)]
            row["slots"] = [str(s.get_editor_property("material_slot_name"))
                            for s in asset.get_editor_property("static_materials")]
            body = asset.get_editor_property("body_setup")
            row["simple_collision"] = int(
                unreal.EditorStaticMeshLibrary.get_simple_collision_count(asset))
            row["collision_flag"] = str(
                body.get_editor_property("collision_trace_flag"))
            row["lod_count"] = int(
                unreal.EditorStaticMeshLibrary.get_lod_count(asset))
            row["uv_channels"] = int(asset.get_num_uv_channels(0)) \
                if hasattr(asset, "get_num_uv_channels") else None
        rows.append(row)
    return rows


def try_import(label, source, roll, combine, collision, bake):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = "PROBE_" + label
    pipeline.import_offset_rotation = unreal.Rotator(roll=roll)
    pipeline.import_offset_uniform_scale = 1.0
    pipeline.common_meshes_properties.bake_meshes = bake
    pipeline.mesh_pipeline.combine_static_meshes = combine
    pipeline.mesh_pipeline.build_nanite = True
    pipeline.mesh_pipeline.set_editor_property("collision", collision)
    pipeline.mesh_pipeline.import_collision_according_to_mesh_name = collision
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    pipeline.material_pipeline.import_materials = False
    pipeline.material_pipeline.texture_pipeline.import_textures = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]

    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    try:
        objects = manager.import_asset(
            SCRATCH, manager.create_source_data(str(source)), params)
    except Exception as exc:  # noqa: BLE001
        return {"label": label, "roll": roll, "combine": combine,
                "collision": collision, "bake": bake,
                "error": "{}: {}".format(type(exc).__name__, exc)}
    return {"label": label, "roll": roll, "combine": combine,
            "collision": collision, "bake": bake,
            "object_count": len(objects or []), "objects": describe(objects)}


def clear():
    if unreal.EditorAssetLibrary.does_directory_exist(SCRATCH):
        unreal.EditorAssetLibrary.delete_directory(SCRATCH)


# --- FBX: the authored path (cm, UV0+UV1, UCX collision) ---------------------
report["fbx"] = try_import(
    "FBX", PACKAGE + "/Meshes/SM_Zhengximen_LOD0.fbx",
    roll=0.0, combine=True, collision=True, bake=True)
clear()

# --- GLB: trimesh, Z-up positions, metres, no materials ---------------------
for roll in (0.0, -90.0, 90.0):
    report["glb_roll_{:+.0f}".format(roll)] = try_import(
        "GLB{}".format(int(roll)), PACKAGE + "/Meshes/SM_Zhengximen_LOD0.glb",
        roll=roll, combine=True, collision=False, bake=True)
    clear()

# --- one modular GLB, to confirm the per-material split ---------------------
report["modular_graybrick"] = try_import(
    "MODGrayBrick", PACKAGE + "/Meshes/Modular/SM_Zhengximen_GrayBrick.glb",
    roll=0.0, combine=True, collision=False, bake=True)
clear()

report["scratch_removed"] = not unreal.EditorAssetLibrary.does_directory_exist(SCRATCH)
unreal.log("ZHENGXIMEN_PROBE_RESULT " + json.dumps(report, indent=2))
print("ZHENGXIMEN_PROBE_RESULT " + json.dumps(report, indent=2))
