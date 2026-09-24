"""Import the Zhengximen / Great West Gate package as a project landmark.

Source package (extracted, not committed): Raw3DPacket/Zhengximen_GreatWestGate_UE5_Package/
  Zhengximen_GreatWestGate_UE5/
    Meshes/Modular/SM_Zhengximen_<Material>.glb   LOD0 split by material group
    Textures/T_<Material>_<Map>.png              8 materials x 6 maps, all 4096x4096
    Materials/MI_*.json                           material-instance binding specs

Destination mirrors the Wenmingmen / Xiaobeimen landmark layout:
  /Game/Assets/Environment/GuangzhouLandmarks/Zhengximen/{Meshes,Textures,Materials}

WHY THE MODULAR GLBs AND NOT THE FBX
------------------------------------
The package documents SM_Zhengximen_LOD0.fbx (UV0 + UV1 lightmap + UCX collision +
sockets) as the preferred import. It does not import in this engine build: probed
twice, once through legacy AssetImportTask+FbxImportUI and once through
InterchangeManager.import_asset, and both times Interchange logs
"Interchange start importing source [...LOD0.fbx]" and then never logs the matching
"completed" line - no asset, no error. See Saved/RawModelImport/zhengximen-probe-stdout.log.

The GLB route does work, but SM_Zhengximen_LOD0.glb carries NO materials at all
(the glTF has "materials": []), so a combined import would give one material slot
for the whole gate and lose every architectural material. The per-material modular
GLBs avoid that: one mesh per material group, one material slot each.

The cost is the UCX collision and the UV1 lightmap atlas. Collision is rebuilt here
as CTF_USE_COMPLEX_AS_SIMPLE on the structural groups, which is exactly what
ImportWenmingmen.py does; the project lights with Lumen, so the lightmap atlas is
not on the critical path.

WHY THE IMPORT IS ADAPTIVE
--------------------------
The GLBs are trimesh output with identity node transforms and Z-up positions, which
is not the glTF Y-up convention. Interchange therefore applies its Y-up-to-Z-up
conversion and tips the model onto its side: at roll 0 every group comes back with
its source Y and Z transposed. Each group is imported with candidate (roll, scale)
pairs and judged against the footprint read from its own POSITION accessors, so a
transposed import cannot slip through - a "tallest axis is Z" test can be fooled,
because the timber pavilion band and the brick wall are both genuinely wider than
they are tall.

Nanite fallback is forced to the full source mesh. On Metal a Nanite mesh renders
from its *fallback* mesh, and the default fallback_target=AUTO decimates it, which
shows up as black triangular holes on walls and roofs.

Idempotent and resumable: progress is recorded in
Saved/RawModelImport/Zhengximen-import.json, so an interrupted run can be resumed
without guessing what already happened.
"""

import json
import shutil
import struct
from pathlib import Path

import unreal

PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
PACKAGE = (PROJECT / "Raw3DPacket" / "Zhengximen_GreatWestGate_UE5_Package"
           / "Zhengximen_GreatWestGate_UE5")
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen"
MESH_DEST = DEST + "/Meshes"
TEX_DEST = DEST + "/Textures"
MAT_DEST = DEST + "/Materials"
MANIFEST = PROJECT / "Saved" / "RawModelImport" / "Zhengximen-import.json"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

# The modular GLBs the package ships, i.e. every material group that has geometry.
# LimePlaster has textures but no geometry, so it gets an MI and no mesh.
MESH_GROUPS = ["GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber",
               "ClayRoofTile", "BlackIron", "GatePlaque"]
ALL_MATERIALS = MESH_GROUPS + ["LimePlaster"]

# Collision on what a player can actually walk into. Roof tiles and the plaque are
# decorative and stay NoCollision.
STRUCTURAL = {"GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber", "BlackIron"}

# The package documents DirectX (Y-) normals, which is what Unreal expects, so the
# green channel must NOT be flipped. flip_green_channel exists to convert OpenGL
# (Y+) maps; setting it here would invert every normal.
MAP_KINDS = {
    "BaseColor": ("TC_DEFAULT", True),
    "Normal": ("TC_NORMALMAP", False),
    "Roughness": ("TC_MASKS", False),
    "Metallic": ("TC_MASKS", False),
    "AO": ("TC_GRAYSCALE", False),
    "Height": ("TC_GRAYSCALE", False),
}

MASTER_NAME = "M_Zhengximen_Master"
MASTER_PARAMS = [
    ("BaseColorTex", "MP_BASE_COLOR", "RGB", None),
    ("NormalTex", "MP_NORMAL", "RGB", "SAMPLERTYPE_NORMAL"),
    ("RoughnessTex", "MP_ROUGHNESS", "R", None),
    ("MetallicTex", "MP_METALLIC", "R", None),
    ("AOTex", "MP_AMBIENT_OCCLUSION", "R", None),
]
HEIGHT_PARAM = "HeightTex"  # documented by the package; left unconnected on purpose

# (roll, uniform_scale) candidates, tried in order. See the module docstring.
#
# Measured, not guessed: the GLBs are Z-up while glTF says Y-up, so Interchange
# applies its Y-up-to-Z-up conversion and tips the model onto its side. At roll 0
# every group comes back with its source Y and Z transposed - AgedWood 18.86 x
# 8.64 x 14.70 m arrives as 18.86 x 14.70 x 8.64 - and roll -90 restores the
# source exactly. Uniform scale 1.0 is right: Interchange already converts the
# glTF metres to centimetres. This matches ImportGreatNorthGateHighDetail.py's
# IMPORT_ROLL = -90.0.
IMPORT_CANDIDATES = [(-90.0, 1.0), (90.0, 1.0), (0.0, 1.0),
                     (-90.0, 100.0), (0.0, 100.0)]

# Expected footprint of the whole gate, from Documentation/AssetManifest.json, cm.
EXPECTED_SIZE_CM = (5320.0, 1080.0, 1689.7)
SIZE_TOLERANCE_RATIO = 0.02

MIN_FREE_BYTES = 1_500_000_000

report = {"steps": [], "textures": {}, "meshes": [], "materials": [],
          "problems": []}


def note(step, **fields):
    entry = {"step": step}
    entry.update(fields)
    report["steps"].append(entry)


def problem(message):
    report["problems"].append(str(message))
    unreal.log_warning("ZHENGXIMEN_IMPORT_PROBLEM " + str(message))


def save_manifest():
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")


def preflight():
    assert PROJECT.is_dir(), PROJECT
    assert PACKAGE.is_dir(), PACKAGE
    assert (PACKAGE / "Meshes" / "Modular").is_dir(), "Modular GLBs missing"
    free = shutil.disk_usage(PROJECT).free
    note("preflight", project=str(PROJECT), free_gib=round(free / 1024 ** 3, 2))
    if free < MIN_FREE_BYTES:
        raise RuntimeError("only {:.2f} GiB free".format(free / 1024 ** 3))


# ---------------------------------------------------------------- textures

def import_textures():
    """Import every PBR map and set colour space + compression per map kind.

    Idempotent: an asset that already exists is re-tuned rather than reimported,
    which is what makes a resumed run cheap.
    """
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for path in sorted(p for p in (PACKAGE / "Textures").glob("*.png")):
        parts = path.stem.split("_")
        material, kind = parts[1], "_".join(parts[2:])
        if kind not in MAP_KINDS:
            problem("unexpected map kind in {} (skipped)".format(path.name))
            continue
        asset_path = "{}/{}".format(TEX_DEST, path.stem)
        texture = EAL.load_asset(asset_path)
        if texture is None:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(path))
            task.set_editor_property("destination_path", TEX_DEST)
            task.set_editor_property("destination_name", path.stem)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            task.set_editor_property("replace_existing", True)
            tools.import_asset_tasks([task])
            texture = EAL.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            problem("{} did not import as Texture2D".format(path.name))
            continue

        compression_name, srgb = MAP_KINDS[kind]
        try:
            texture.set_editor_property(
                "compression_settings",
                getattr(unreal.TextureCompressionSettings, compression_name))
        except Exception as exc:  # noqa: BLE001
            problem("{} compression: {}".format(path.name, exc))
        try:
            texture.set_editor_property("srgb", srgb)
        except Exception as exc:  # noqa: BLE001
            problem("{} srgb: {}".format(path.name, exc))
        if kind == "Normal":
            try:
                texture.set_editor_property("flip_green_channel", False)
            except Exception as exc:  # noqa: BLE001
                problem("{} flip_green_channel: {}".format(path.name, exc))
        EAL.save_loaded_asset(texture)
        report["textures"][asset_path] = {"source": path.name,
                                          "material": material, "kind": kind}
    note("textures_done", imported=len(report["textures"]))


# ------------------------------------------------------------------- meshes

def set_nanite(mesh):
    """Nanite ON with the full source mesh as the fallback (Metal-safe)."""
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    settings.set_editor_property(
        "fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
    settings.set_editor_property("fallback_percent_triangles", 1.0)
    settings.set_editor_property("fallback_relative_error", 0.0)
    mesh.set_editor_property("nanite_settings", settings)


def mesh_size(mesh):
    extent = mesh.get_bounds().box_extent
    return (float(extent.x) * 2.0, float(extent.y) * 2.0, float(extent.z) * 2.0)


def glb_expected_size_cm(source):
    """The GLB's own footprint, in cm, straight from the POSITION accessors.

    This is the ground truth the import is judged against. A size heuristic like
    "the tallest axis is Z" is not enough here: the timber pavilion band is
    genuinely wider than it is tall, and the brick wall is wider and deeper than
    it is tall, so a Y/Z swap passes such a test while still lying on its side.
    """
    with open(source, "rb") as handle:
        magic, _, _ = struct.unpack("<4sII", handle.read(12))
        assert magic == b"glTF", source
        chunk_length, _ = struct.unpack("<I4s", handle.read(8))
        document = json.loads(handle.read(chunk_length).decode("utf-8"))
    low = [1e30] * 3
    high = [-1e30] * 3
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            accessor = document["accessors"][primitive["attributes"]["POSITION"]]
            minimum, maximum = accessor.get("min"), accessor.get("max")
            if not minimum:
                continue
            for axis in range(3):
                low[axis] = min(low[axis], minimum[axis])
                high[axis] = max(high[axis], maximum[axis])
    # glTF is metres, Unreal is centimetres.
    return tuple((high[axis] - low[axis]) * 100.0 for axis in range(3))


def interchange_import(source, asset_name, roll, scale, build_nanite):
    """One Interchange asset import - the route ImportGreatNorthGateHighDetail.py
    uses and the only one that works on this machine."""
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = asset_name
    pipeline.import_offset_rotation = unreal.Rotator(roll=roll)
    pipeline.import_offset_uniform_scale = scale
    pipeline.common_meshes_properties.bake_meshes = True
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = build_nanite
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    pipeline.material_pipeline.import_materials = False
    pipeline.material_pipeline.texture_pipeline.import_textures = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    return manager.import_asset(MESH_DEST, manager.create_source_data(str(source)),
                                params)


def import_group(material):
    """Import one material group, resolving roll/scale against the GLB's own size."""
    source = PACKAGE / "Meshes" / "Modular" / ("SM_Zhengximen_{}.glb".format(material))
    asset_name = "SM_Zhengximen_" + material
    asset_path = MESH_DEST + "/" + asset_name
    expected = glb_expected_size_cm(source)
    attempts = []

    for roll, scale in IMPORT_CANDIDATES:
        if EAL.does_asset_exist(asset_path):
            EAL.delete_asset(asset_path)
        try:
            interchange_import(source, asset_name, roll, scale, build_nanite=True)
        except Exception as exc:  # noqa: BLE001
            attempts.append({"roll": roll, "scale": scale,
                             "error": "{}: {}".format(type(exc).__name__, exc)})
            continue
        mesh = EAL.load_asset(asset_path)
        if not isinstance(mesh, unreal.StaticMesh):
            attempts.append({"roll": roll, "scale": scale, "error": "no asset"})
            continue
        size = mesh_size(mesh)
        deviations = [abs(size[axis] - expected[axis]) / expected[axis]
                      for axis in range(3)]
        matches = max(deviations) <= SIZE_TOLERANCE_RATIO
        attempts.append({"roll": roll, "scale": scale,
                         "size_cm": [round(v, 2) for v in size],
                         "expected_cm": [round(v, 2) for v in expected],
                         "max_deviation": round(max(deviations), 4),
                         "matches": matches})
        if matches:
            set_nanite(mesh)
            if material in STRUCTURAL:
                body = mesh.get_editor_property("body_setup")
                body.set_editor_property(
                    "collision_trace_flag",
                    unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                body.set_editor_property("double_sided_geometry", True)
            EAL.save_loaded_asset(mesh)
            return {"material": material, "mesh": asset_path, "roll": roll,
                    "scale": scale, "size_cm": [round(v, 2) for v in size],
                    "expected_cm": [round(v, 2) for v in expected],
                    "triangles": int(mesh.get_num_triangles(0)),
                    "collision": material in STRUCTURAL, "attempts": attempts}

    problem("could not import {} to match its GLB size {}; attempts: {}".format(
        material, [round(v, 2) for v in expected], attempts))
    return None


def import_meshes():
    for material in MESH_GROUPS:
        row = import_group(material)
        if row:
            report["meshes"].append(row)
            note("mesh_imported", material=material, roll=row["roll"],
                 scale=row["scale"], size_cm=row["size_cm"])
        # Written per group, not once at the end: a run that stops partway still
        # leaves behind exactly what it managed to import.
        save_manifest()
    assert len(report["meshes"]) == len(MESH_GROUPS), \
        "imported {}/{} groups".format(len(report["meshes"]), len(MESH_GROUPS))


# ---------------------------------------------------------------- materials

def build_master():
    master_path = MAT_DEST + "/" + MASTER_NAME
    master = EAL.load_asset(master_path)
    if master:
        return master
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    master = tools.create_asset(MASTER_NAME, MAT_DEST, unreal.Material,
                                unreal.MaterialFactoryNew())
    assert master, master_path
    for index, (name, prop, output, sampler) in enumerate(MASTER_PARAMS):
        expression = MEL.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D,
            -600, -240 + 160 * index)
        expression.set_editor_property("parameter_name", name)
        if sampler:
            try:
                expression.set_editor_property(
                    "sampler_type", getattr(unreal.MaterialSamplerType, sampler))
            except Exception as exc:  # noqa: BLE001
                problem("{} sampler type: {}".format(name, exc))
        MEL.connect_material_property(
            expression, output, getattr(unreal.MaterialProperty, prop))
    try:
        height = MEL.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D, -600, 560)
        height.set_editor_property("parameter_name", HEIGHT_PARAM)
    except Exception as exc:  # noqa: BLE001
        problem("HeightTex parameter: " + str(exc))
    MEL.recompile_material(master)
    EAL.save_loaded_asset(master)
    note("master_created", path=master_path)
    return master


def build_instances(master):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    bindings = [("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"),
                ("RoughnessTex", "Roughness"), ("MetallicTex", "Metallic"),
                ("AOTex", "AO")]
    for name in ALL_MATERIALS:
        asset_path = MAT_DEST + "/MI_" + name
        instance = EAL.load_asset(asset_path)
        if not instance:
            instance = tools.create_asset(
                "MI_" + name, MAT_DEST, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
            assert instance, asset_path
            MEL.set_material_instance_parent(instance, master)
        bound = {}
        for parameter, suffix in bindings:
            texture = EAL.load_asset("{}/T_{}_{}".format(TEX_DEST, name, suffix))
            if not texture:
                problem("missing texture T_{}_{} for {}".format(name, suffix, name))
                continue
            MEL.set_material_instance_texture_parameter_value(
                instance, parameter, texture)
            bound[parameter] = texture.get_path_name()
        EAL.save_loaded_asset(instance)
        report["materials"].append({"asset": asset_path, "parameters": bound})
    note("instances_done", count=len(report["materials"]))


def assign_materials():
    """One material group per mesh, so the binding is by construction rather than
    by matching whatever the source happened to call its slot."""
    for row in report["meshes"]:
        mesh = EAL.load_asset(row["mesh"])
        instance = EAL.load_asset(MAT_DEST + "/MI_" + row["material"])
        assert mesh and instance, row
        mesh.set_material(0, instance)
        EAL.save_loaded_asset(mesh)
        row["material_asset"] = instance.get_path_name()
        row["slots"] = [str(s.get_editor_property("material_slot_name"))
                        for s in mesh.get_editor_property("static_materials")]
    note("materials_assigned", count=len(report["meshes"]))


def main():
    preflight()
    import_textures()
    import_meshes()
    master = build_master()
    build_instances(master)
    assign_materials()

    sizes = [row["size_cm"] for row in report["meshes"]]
    report["union_size_cm"] = [round(max(s[a] for s in sizes), 2) for a in range(3)]
    report["total_triangles"] = sum(row["triangles"] for row in report["meshes"])
    report["passed"] = not report["problems"]
    save_manifest()
    unreal.log("ZHENGXIMEN_IMPORT_COMPLETE " + json.dumps(
        {"passed": report["passed"], "meshes": len(report["meshes"]),
         "textures": len(report["textures"]), "materials": len(report["materials"]),
         "union_size_cm": report["union_size_cm"],
         "total_triangles": report["total_triangles"],
         "problems": report["problems"]}))


if __name__ == "__main__":
    main()
