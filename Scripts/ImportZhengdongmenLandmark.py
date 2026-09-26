"""Import the Zhengdongmen / Great East Gate package as a project landmark.

Source package (extracted, not committed):
Raw3DPacket/Zhengdongmen/prepared/ - staged by Scripts/PrepareZhengdongmenPackage.py
    Meshes/SM_ZDM_<Group>.glb          one GLB per material group, split from LOD0
    Textures/T_ZDM_<Group>_<Kind>.png  BaseColor 4096, other channels 1024

Destination mirrors the Wenmingmen / Zhengximen landmark layout:
  /Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/{Meshes,Textures,Materials}

WHY THE SPLIT GLBs
------------------
The package ships one combined `Zhengdongmen_LOD0.glb` holding seven meshes, one
per material. Imported combined it becomes one StaticMesh with seven slots whose
order is whatever Interchange produced; split, each becomes a StaticMesh with
exactly one slot, so the material binding is by construction. That is the same
reasoning ImportZhengximenLandmark.py used on that package's modular GLBs.

WHY THE COMPACT TEXTURES AT 4K
------------------------------
Both packages ship identical geometry (same 7 meshes, same 50,442 triangles, same
POSITION bounds) and differ only in artwork. The Compact set is the newer and far
more convincing illustration. Its BaseColor ships at 8192, but that is an upscale
of ~1254px art - an 8K->4K->8K round trip reproduces it to a mean absolute error
of 0.49-0.62 of 255, i.e. JPEG noise - so 4K loses nothing and matches the
project's existing 4K landmark convention. Support maps are the Compact 1K PNGs
as shipped; they were generated against the Compact artwork, so pairing them with
the Procedural package's 4K normals would mix two different illustration sets.

GREEN CHANNEL IS FLIPPED, UNLIKE ZHENGXIMEN
-------------------------------------------
The Compact package's README states its generated normals use the glTF/OpenGL +Y
convention, which is what Unreal's flip_green_channel exists to convert. This is
the opposite of Zhengximen, whose package documented DirectX (Y-) normals. The
maps are not flat - measured std of (green-128) is 5-20 of 255 - so the direction
is a real visual choice, not a no-op. It is set from the package's own
documentation rather than guessed.

WHY THE IMPORT IS ADAPTIVE
--------------------------
The GLBs are trimesh output with identity node transforms and Z-up positions,
which is not the glTF Y-up convention, so Interchange applies its Y-up-to-Z-up
conversion and tips the model onto its side. Each group is imported with
candidate (roll, scale) pairs and judged against the footprint read from its own
POSITION accessors - a size heuristic cannot be trusted here, because the stone
base (2870 x 1639 x 1063 cm) is genuinely wider and deeper than it is tall.

Nanite fallback is forced to the full source mesh. On Metal a Nanite mesh renders
from its *fallback* mesh, and the default fallback_target=AUTO decimates it,
which shows up as black triangular holes on walls and roofs.

Idempotent and resumable: progress is recorded in
Saved/RawModelImport/Zhengdongmen-import.json.
"""

import json
import shutil
import struct
from pathlib import Path

import unreal

PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
PACKAGE = PROJECT / "Raw3DPacket" / "Zhengdongmen" / "prepared"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen"
MESH_DEST = DEST + "/Meshes"
TEX_DEST = DEST + "/Textures"
MAT_DEST = DEST + "/Materials"
MANIFEST = PROJECT / "Saved" / "RawModelImport" / "Zhengdongmen-import.json"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

# Every material group that has geometry. Unlike Zhengximen there is no
# texture-only group: all eight have meshes. Ridge is authored here rather than
# taken from the supplied package — the ridges used to share RoofTile.
MESH_GROUPS = ["Stone", "Wood", "RoofTile", "Ridge", "Plaster", "Iron", "DoorWood", "Sign"]

# Collision on what a player can actually walk into or through. The gate has a
# real walk-through arch (see the package README), so the stone base carries it.
# Roof tiles, the plaster panels and the plaque are decorative.
STRUCTURAL = {"Stone", "Wood", "DoorWood", "Iron"}

# The source glTF declares every material doubleSided=True, and the geometry
# relies on it: the plaque is a single quad, and the window recesses are open
# shells. Both the render-side (master material) and the geometry-side
# (body_setup.double_sided_geometry) flags follow the source.
MAP_KINDS = {
    "BaseColor": ("TC_DEFAULT", True),
    "Normal": ("TC_NORMALMAP", False),
    "Roughness": ("TC_MASKS", False),
    "Metallic": ("TC_MASKS", False),
    "AO": ("TC_GRAYSCALE", False),
    "Height": ("TC_GRAYSCALE", False),
}
FLIP_NORMAL_GREEN = True   # package README: normals are glTF/OpenGL +Y

MASTER_NAME = "M_ZDM_Master"
MASTER_PARAMS = [
    ("BaseColorTex", "MP_BASE_COLOR", "RGB", None),
    ("NormalTex", "MP_NORMAL", "RGB", "SAMPLERTYPE_NORMAL"),
    ("RoughnessTex", "MP_ROUGHNESS", "R", "SAMPLERTYPE_MASKS"),
    ("MetallicTex", "MP_METALLIC", "R", "SAMPLERTYPE_MASKS"),
    ("AOTex", "MP_AMBIENT_OCCLUSION", "R", "SAMPLERTYPE_LINEAR_GRAYSCALE"),
]
# Documented by the package as a parallax/displacement input. Left unconnected
# on purpose: the height maps are 1K estimates and the project has no POM
# convention, so the parameter exists for a later tuning pass rather than being
# wired blind. Same treatment as M_Zhengximen_Master.
HEIGHT_PARAM = "HeightTex"

# (roll, uniform_scale) candidates, tried in order. See the module docstring.
# Roll -90 matches ImportZhengximenLandmark.py and ImportGreatNorthGateHighDetail.py.
IMPORT_CANDIDATES = [(-90.0, 1.0), (90.0, 1.0), (0.0, 1.0),
                     (-90.0, 100.0), (0.0, 100.0)]
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
    unreal.log_warning("ZHENGDONGMEN_IMPORT_PROBLEM " + str(message))


def save_manifest():
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")


def preflight():
    assert PROJECT.is_dir(), PROJECT
    assert PACKAGE.is_dir(), PACKAGE
    assert (PACKAGE / "Meshes").is_dir(), "split GLBs missing"
    assert (PACKAGE / "Textures").is_dir(), "staged textures missing"
    free = shutil.disk_usage(PROJECT).free
    note("preflight", project=str(PROJECT), free_gib=round(free / 1024 ** 3, 2))
    if free < MIN_FREE_BYTES:
        raise RuntimeError("only {:.2f} GiB free".format(free / 1024 ** 3))


# ---------------------------------------------------------------- textures

def import_textures():
    """Import every PBR map and set colour space + compression per map kind.

    Idempotent: an asset that already exists is re-tuned rather than reimported.
    """
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for path in sorted(p for p in (PACKAGE / "Textures").glob("*.png")):
        parts = path.stem.split("_")
        kind = "_".join(parts[3:])
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
                texture.set_editor_property("flip_green_channel", FLIP_NORMAL_GREEN)
            except Exception as exc:  # noqa: BLE001
                problem("{} flip_green_channel: {}".format(path.name, exc))
        EAL.save_loaded_asset(texture)
        report["textures"][asset_path] = {"source": path.name, "kind": kind,
                                          "size": [texture.blueprint_get_size_x(),
                                                   texture.blueprint_get_size_y()]}
    note("textures_done", imported=len(report["textures"]))


def refresh_textures(names):
    """Force-reimport named texture assets whose source PNG was re-authored.

    `import_textures` only imports a texture that does not exist yet, so a changed
    source image would otherwise keep its old pixels in the engine. Dimensions can
    change too — the plaque board moved from a square canvas to its own 4:1 aspect
    — so this reimports the asset rather than re-tuning the existing one.
    """
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rows = []
    for name in names:
        source = PACKAGE / "Textures" / (name + ".png")
        kind = "_".join(name.split("_")[3:])
        assert source.exists(), source
        assert kind in MAP_KINDS, (name, kind)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", TEX_DEST)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", True)
        tools.import_asset_tasks([task])
        texture = EAL.load_asset("{}/{}".format(TEX_DEST, name))
        assert isinstance(texture, unreal.Texture2D), name
        compression_name, srgb = MAP_KINDS[kind]
        texture.set_editor_property(
            "compression_settings",
            getattr(unreal.TextureCompressionSettings, compression_name))
        texture.set_editor_property("srgb", srgb)
        if kind == "Normal":
            texture.set_editor_property("flip_green_channel", FLIP_NORMAL_GREEN)
        EAL.save_loaded_asset(texture)
        size = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
        rows.append({"name": name, "kind": kind, "size": size})
        report["textures"]["{}/{}".format(TEX_DEST, name)] = {
            "source": source.name, "kind": kind, "size": size}
        unreal.log("ZHENGDONGMEN_TEXTURE_REFRESHED {} {}x{}".format(
            name, size[0], size[1]))
    return rows


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
    """The GLB's own footprint in cm, from its POSITION accessors.

    The ground truth the import is judged against. A "tallest axis is Z" test is
    not enough: the stone base really is 2870 x 1639 x 1063 cm, so a Y/Z swap
    passes such a test while still lying on its side.
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
    return tuple((high[axis] - low[axis]) * 100.0 for axis in range(3))


def interchange_import(source, asset_name, roll, scale):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = asset_name
    pipeline.import_offset_rotation = unreal.Rotator(roll=roll)
    pipeline.import_offset_uniform_scale = scale
    pipeline.common_meshes_properties.bake_meshes = True
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = True
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


def import_group(group):
    """Import one material group, resolving roll/scale against the GLB's own size."""
    source = PACKAGE / "Meshes" / ("SM_ZDM_{}.glb".format(group))
    asset_name = "SM_ZDM_" + group
    asset_path = MESH_DEST + "/" + asset_name
    expected = glb_expected_size_cm(source)
    attempts = []

    for attempt_index, (roll, scale) in enumerate(IMPORT_CANDIDATES):
        # Reimport straight over the existing asset on the first attempt.
        # delete_asset is a force-delete, and the Blueprint's HISMs reference
        # these meshes, so a reimport must not clear them on the happy path.
        if attempt_index and EAL.does_asset_exist(asset_path):
            EAL.delete_asset(asset_path)
        try:
            interchange_import(source, asset_name, roll, scale)
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
            body = mesh.get_editor_property("body_setup")
            if group in STRUCTURAL:
                body.set_editor_property(
                    "collision_trace_flag",
                    unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            # Every source material is doubleSided=True.
            body.set_editor_property("double_sided_geometry", True)
            EAL.save_loaded_asset(mesh)
            low, high = mesh_bounds_cm(mesh)
            return {"group": group, "mesh": asset_path, "roll": roll,
                    "scale": scale, "size_cm": [round(v, 2) for v in size],
                    "bounds_cm": [[round(v, 2) for v in low],
                                  [round(v, 2) for v in high]],
                    "expected_cm": [round(v, 2) for v in expected],
                    "triangles": int(mesh.get_num_triangles(0)),
                    "collision": group in STRUCTURAL, "attempts": attempts}

    problem("could not import {} to match its GLB size {}; attempts: {}".format(
        group, [round(v, 2) for v in expected], attempts))
    return None


def import_meshes():
    for group in MESH_GROUPS:
        row = import_group(group)
        if row:
            report["meshes"].append(row)
            note("mesh_imported", group=group, roll=row["roll"], scale=row["scale"],
                 size_cm=row["size_cm"])
        # Written per group, not once at the end: a run that stops partway still
        # leaves behind exactly what it managed to import.
        save_manifest()
    assert len(report["meshes"]) == len(MESH_GROUPS), \
        "imported {}/{} groups".format(len(report["meshes"]), len(MESH_GROUPS))


# ---------------------------------------------------------------- materials

def build_master():
    master_path = MAT_DEST + "/" + MASTER_NAME
    master = EAL.load_asset(master_path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    if not master:
        master = tools.create_asset(MASTER_NAME, MAT_DEST, unreal.Material,
                                    unreal.MaterialFactoryNew())
    assert master, master_path
    # Rebuild existing graphs too: a valid instance cannot repair an invalid
    # parent default texture/sampler pair on Metal.
    MEL.delete_all_material_expressions(master)
    master.set_editor_property("used_with_instanced_static_meshes", True)
    master.set_editor_property("used_with_nanite", True)
    # The source glTF declares every material doubleSided=True.
    master.set_editor_property("two_sided", True)
    defaults = {"BaseColorTex": "BaseColor", "NormalTex": "Normal",
                "RoughnessTex": "Roughness", "MetallicTex": "Metallic",
                "AOTex": "AO"}
    typed_nodes = []
    for index, (name, prop, output, sampler) in enumerate(MASTER_PARAMS):
        expression = MEL.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D,
            -600, -240 + 160 * index)
        texture = EAL.load_asset(
            "{}/T_ZDM_Stone_{}".format(TEX_DEST, defaults[name]))
        assert texture, name
        expression.set_editor_property("texture", texture)
        if sampler:
            try:
                expression.set_editor_property(
                    "sampler_type", getattr(unreal.MaterialSamplerType, sampler))
            except Exception as exc:  # noqa: BLE001
                problem("{} sampler type: {}".format(name, exc))
        expression.set_editor_property("parameter_name", name)
        typed_nodes.append((expression, texture, sampler))
        MEL.connect_material_property(
            expression, output, getattr(unreal.MaterialProperty, prop))
    try:
        height = MEL.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D, -600, 560)
        height.set_editor_property("parameter_name", HEIGHT_PARAM)
        height.set_editor_property(
            "texture", EAL.load_asset(TEX_DEST + "/T_ZDM_Stone_Height"))
        height.set_editor_property(
            "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    except Exception as exc:  # noqa: BLE001
        problem("HeightTex parameter: " + str(exc))
    # Finalize after all nodes have unique parameter names: editor property
    # changes can synchronize defaults while new parameters are still unnamed.
    for node, texture, sampler in typed_nodes:
        node.set_editor_property("texture", texture)
        if sampler:
            node.set_editor_property(
                "sampler_type", getattr(unreal.MaterialSamplerType, sampler))
    MEL.recompile_material(master)
    EAL.save_loaded_asset(master, only_if_is_dirty=False)
    note("master_created", path=master_path)
    return master


def build_instances(master):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    bindings = [("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"),
                ("RoughnessTex", "Roughness"), ("MetallicTex", "Metallic"),
                ("AOTex", "AO")]
    for group in MESH_GROUPS:
        asset_path = MAT_DEST + "/MI_" + group
        instance = EAL.load_asset(asset_path)
        if not instance:
            instance = tools.create_asset(
                "MI_" + group, MAT_DEST, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
            assert instance, asset_path
            MEL.set_material_instance_parent(instance, master)
        bound = {}
        for parameter, suffix in bindings:
            texture = EAL.load_asset(
                "{}/T_ZDM_{}_{}".format(TEX_DEST, group, suffix))
            if not texture:
                problem("missing texture T_ZDM_{}_{}".format(group, suffix))
                continue
            MEL.set_material_instance_texture_parameter_value(
                instance, parameter, texture)
            bound[parameter] = texture.get_path_name()
        EAL.save_loaded_asset(instance)
        report["materials"].append({"asset": asset_path, "parameters": bound})
    note("instances_done", count=len(report["materials"]))


def assign_materials():
    """One material group per mesh, so the binding is by construction."""
    for row in report["meshes"]:
        mesh = EAL.load_asset(row["mesh"])
        instance = EAL.load_asset(MAT_DEST + "/MI_" + row["group"])
        assert mesh and instance, row
        mesh.set_material(0, instance)
        EAL.save_loaded_asset(mesh)
        row["material_asset"] = instance.get_path_name()
        row["slots"] = [str(s.get_editor_property("material_slot_name"))
                        for s in mesh.get_editor_property("static_materials")]
    note("materials_assigned", count=len(report["meshes"]))


def mesh_bounds_cm(mesh):
    """Absolute bounds of a mesh in its own space, as (min_xyz, max_xyz)."""
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    low = [float(origin.x) - float(extent.x), float(origin.y) - float(extent.y),
           float(origin.z) - float(extent.z)]
    high = [float(origin.x) + float(extent.x), float(origin.y) + float(extent.y),
            float(origin.z) + float(extent.z)]
    return low, high


def union_size_cm(rows):
    """Union footprint of the parts, from their absolute bounds.

    Deliberately NOT the largest per-axis extent. Each part has its own origin, so
    the roof tiles (714.7 cm tall, sitting about 12 m up) and the stone base
    (1063 cm tall, on the ground) both under-report the building: the largest
    per-axis extent gives 1663 cm of height where the gate is really 1933 cm.
    """
    low = [float("inf")] * 3
    high = [float("-inf")] * 3
    for row in rows:
        box_low, box_high = row["bounds_cm"]
        for axis in range(3):
            low[axis] = min(low[axis], box_low[axis])
            high[axis] = max(high[axis], box_high[axis])
    return [round(high[axis] - low[axis], 2) for axis in range(3)]


def main():
    preflight()
    import_textures()
    import_meshes()
    master = build_master()
    build_instances(master)
    assign_materials()

    report["union_size_cm"] = union_size_cm(report["meshes"])
    report["total_triangles"] = sum(row["triangles"] for row in report["meshes"])
    report["passed"] = not report["problems"]
    save_manifest()
    unreal.log("ZHENGDONGMEN_IMPORT_COMPLETE " + json.dumps(
        {"passed": report["passed"], "meshes": len(report["meshes"]),
         "textures": len(report["textures"]), "materials": len(report["materials"]),
         "union_size_cm": report["union_size_cm"],
         "total_triangles": report["total_triangles"],
         "problems": report["problems"]}))


if __name__ == "__main__":
    main()
