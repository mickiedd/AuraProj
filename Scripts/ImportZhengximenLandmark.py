"""Import the Zhengximen / Great West Gate package as a project landmark.

Source package (extracted, not committed): Raw3DPacket/Zhengximen_GreatWestGate_UE5_Package/
  Zhengximen_GreatWestGate_UE5/
    Meshes/SM_Zhengximen_LOD0.fbx      authored path: UV0 + UV1 lightmap + UCX proxies
    Meshes/SM_Zhengximen_LOD1..4.fbx   authored LODs
    Textures/T_<Material>_<Map>.png    8 materials x 6 maps, all 4096x4096
    Materials/MI_*.json                material-instance binding specs

Destination mirrors the Wenmingmen / Xiaobeimen landmark layout:
  /Game/Assets/Environment/GuangzhouLandmarks/Zhengximen/{Meshes,Textures,Materials}

Why this script exists rather than the package's own Unreal/*.py: those hardcode
'/Game/Zhengximen' and derive ROOT from __file__, which lands outside the project.
Everything here is derived from unreal.Paths.project_dir().

Nanite fallback is forced to the full source mesh. On Metal a Nanite mesh renders
from its *fallback* mesh, and the default fallback_target=AUTO decimates it, which
shows up as black triangular holes on walls and roofs. Reimporting a mesh resets
the target to AUTO, so it is re-asserted after the LOD imports, and the validator
asserts it again in a fresh process.

Idempotent: re-running reimports over the existing assets. Progress and every
decision are recorded in Saved/RawModelImport/Zhengximen-import.json so an
interrupted run can be resumed without guessing what already happened.
"""

import json
import shutil
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

MESH_ASSET_NAME = "SM_Zhengximen"
LOD_SOURCE_NAME = "SM_Zhengximen_LOD0"
LOD_COUNT = 5

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

MATERIALS = ["GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber",
             "ClayRoofTile", "LimePlaster", "BlackIron", "GatePlaque"]

MASTER_NAME = "M_Zhengximen_Master"
# (parameter name, material property, output, sampler type)
MASTER_PARAMS = [
    ("BaseColorTex", "MP_BASE_COLOR", "RGB", None),
    ("NormalTex", "MP_NORMAL", "RGB", "SAMPLERTYPE_NORMAL"),
    ("RoughnessTex", "MP_ROUGHNESS", "R", None),
    ("MetallicTex", "MP_METALLIC", "R", None),
    ("AOTex", "MP_AMBIENT_OCCLUSION", "R", None),
]
HEIGHT_PARAM = "HeightTex"  # documented by the package; left unconnected on purpose

MIN_FREE_BYTES = 1_500_000_000

report = {"steps": [], "textures": {}, "meshes": {}, "materials": {}, "problems": []}


def note(step, **fields):
    entry = {"step": step}
    entry.update(fields)
    report["steps"].append(entry)
    print("ZHENGXIMEN_IMPORT_STEP " + json.dumps(entry, default=str))


def problem(message):
    report["problems"].append(str(message))
    print("ZHENGXIMEN_IMPORT_PROBLEM " + str(message))


def save_manifest():
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")


def preflight():
    assert PROJECT.is_dir(), PROJECT
    assert PACKAGE.is_dir(), PACKAGE
    for relative in ("Meshes/SM_Zhengximen_LOD0.fbx", "Textures", "Materials"):
        assert (PACKAGE / relative).exists(), relative
    free = shutil.disk_usage(PROJECT).free
    note("preflight", project=str(PROJECT), package=str(PACKAGE),
         free_bytes=free, free_gib=round(free / 1024 ** 3, 2))
    if free < MIN_FREE_BYTES:
        raise RuntimeError(
            "only {:.2f} GiB free; 48 4K PBR maps plus their DDC entries need "
            "roughly 1.2 GiB. Free space before importing.".format(free / 1024 ** 3))


# ---------------------------------------------------------------- textures

def import_textures():
    """Import every PBR map and set colour space + compression per map kind."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    paths = sorted(p for p in (PACKAGE / "Textures").glob("*.png"))
    note("textures_begin", count=len(paths))

    for path in paths:
        stem = path.stem  # T_<Material>_<Map>
        parts = stem.split("_")
        material, kind = parts[1], "_".join(parts[2:])
        if kind not in MAP_KINDS:
            problem("unexpected map kind in {} (skipped)".format(path.name))
            continue

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(path))
        task.set_editor_property("destination_path", TEX_DEST)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", True)
        tools.import_asset_tasks([task])
        imported = [str(p) for p in task.get_editor_property("imported_object_paths")]
        if not imported:
            problem("no asset produced for " + path.name)
            continue

        asset_path = imported[0]
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            problem("{} imported as {} not Texture2D".format(
                path.name, type(texture).__name__))
            continue

        compression_name, srgb = MAP_KINDS[kind]
        applied = {}
        try:
            texture.set_editor_property(
                "compression_settings",
                getattr(unreal.TextureCompressionSettings, compression_name))
            applied["compression"] = compression_name
        except Exception as exc:  # noqa: BLE001
            problem("{} compression: {}".format(path.name, exc))
        try:
            texture.set_editor_property("srgb", srgb)
            applied["srgb"] = srgb
        except Exception as exc:  # noqa: BLE001
            problem("{} srgb: {}".format(path.name, exc))
        if kind == "Normal":
            try:
                # Package states DirectX (Y-) == Unreal convention: no flip.
                texture.set_editor_property("flip_green_channel", False)
                applied["flip_green_channel"] = False
            except Exception as exc:  # noqa: BLE001
                problem("{} flip_green_channel: {}".format(path.name, exc))

        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        report["textures"][asset_path] = {
            "source": path.name, "material": material, "kind": kind, **applied}

    note("textures_done", imported=len(report["textures"]))


# ------------------------------------------------------------------- meshes

def set_nanite(mesh, label):
    """Nanite ON with the full source mesh as the fallback (Metal-safe)."""
    try:
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        settings.set_editor_property(
            "fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        settings.set_editor_property("fallback_percent_triangles", 1.0)
        settings.set_editor_property("fallback_relative_error", 0.0)
        mesh.set_editor_property("nanite_settings", settings)
        note("nanite_set", mesh=label)
    except Exception as exc:  # noqa: BLE001
        problem("{} nanite settings: {}".format(label, exc))


def import_mesh():
    """Import the authored LOD0 FBX: combine meshes, no auto lightmap UVs, no
    generated collision (the FBX carries UCX proxies), materials/textures off."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    source = PACKAGE / "Meshes" / "SM_Zhengximen_LOD0.fbx"

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", MESH_DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    try:
        options.set_editor_property(
            "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("generate_lightmap_u_vs", False)  # UV1 authored
        data.set_editor_property("auto_generate_collision", False)  # UCX embedded
        data.set_editor_property("remove_degenerates", True)
        options.set_editor_property("static_mesh_import_data", data)
    except Exception as exc:  # noqa: BLE001
        problem("FBX option compatibility: " + str(exc))
    task.set_editor_property("options", options)
    tools.import_asset_tasks([task])

    imported = [str(p) for p in task.get_editor_property("imported_object_paths")]
    meshes = [p for p in imported
              if isinstance(unreal.EditorAssetLibrary.load_asset(p), unreal.StaticMesh)]
    note("mesh_imported", source=source.name, imported=imported, static_meshes=meshes)
    assert meshes, "LOD0 produced no StaticMesh: " + repr(imported)

    asset_path = meshes[0]
    target_path = MESH_DEST + "/" + MESH_ASSET_NAME
    if asset_path != target_path:
        if unreal.EditorAssetLibrary.does_asset_exist(target_path):
            unreal.EditorAssetLibrary.delete_asset(target_path)
        assert unreal.EditorAssetLibrary.rename_asset(asset_path, target_path), \
            "could not rename {} -> {}".format(asset_path, target_path)
        asset_path = target_path
    report["meshes"]["lod0"] = asset_path
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def import_lods(mesh):
    """Import LOD1..LOD4. Failure is non-fatal: Nanite renders the source mesh and
    the fallback is forced to full density, so LODs are platform/fallback only."""
    for index in range(1, LOD_COUNT):
        source = PACKAGE / "Meshes" / "SM_Zhengximen_LOD{}.fbx".format(index)
        try:
            result = unreal.EditorStaticMeshLibrary.import_lod(mesh, index, str(source))
            report["meshes"]["lod{}".format(index)] = {"source": source.name,
                                                       "result": int(result)}
        except Exception as exc:  # noqa: BLE001
            problem("LOD{} import: {}".format(index, exc))
    note("lods_done")


# ---------------------------------------------------------------- materials

def property_enum(name):
    return getattr(unreal.MaterialProperty, name)


def build_master():
    master_path = MAT_DEST + "/" + MASTER_NAME
    master = unreal.EditorAssetLibrary.load_asset(master_path)
    if master:
        note("master_exists", path=master_path)
        return master

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    master = tools.create_asset(MASTER_NAME, MAT_DEST, unreal.Material,
                                unreal.MaterialFactoryNew())
    assert master, master_path

    library = unreal.MaterialEditingLibrary
    for index, (name, prop, output, sampler) in enumerate(MASTER_PARAMS):
        expression = library.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D,
            -600, -240 + 160 * index)
        expression.set_editor_property("parameter_name", name)
        if sampler:
            try:
                expression.set_editor_property(
                    "sampler_type", getattr(unreal.MaterialSamplerType, sampler))
            except Exception as exc:  # noqa: BLE001
                problem("{} sampler type: {}".format(name, exc))
        library.connect_material_property(expression, output, property_enum(prop))

    # HeightTex is documented by the package but intentionally not wired: the
    # master is opaque, and POM/displacement is a per-project decision.
    try:
        height = library.create_material_expression(
            master, unreal.MaterialExpressionTextureSampleParameter2D, -600, 560)
        height.set_editor_property("parameter_name", HEIGHT_PARAM)
    except Exception as exc:  # noqa: BLE001
        problem("HeightTex parameter: " + str(exc))

    library.recompile_material(master)
    unreal.EditorAssetLibrary.save_loaded_asset(master)
    note("master_created", path=master_path)
    return master


def build_instances(master):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    library = unreal.MaterialEditingLibrary
    bindings = [("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"),
                ("RoughnessTex", "Roughness"), ("MetallicTex", "Metallic"),
                ("AOTex", "AO")]

    for name in MATERIALS:
        instance_path = MAT_DEST + "/MI_" + name
        instance = unreal.EditorAssetLibrary.load_asset(instance_path)
        if not instance:
            instance = tools.create_asset(
                "MI_" + name, MAT_DEST, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
            assert instance, instance_path
            library.set_material_instance_parent(instance, master)

        bound = {}
        for parameter, suffix in bindings:
            texture_path = "{}/T_{}_{}".format(TEX_DEST, name, suffix)
            texture = unreal.EditorAssetLibrary.load_asset(texture_path)
            if not texture:
                problem("missing texture {} for {}".format(texture_path, name))
                continue
            library.set_material_instance_texture_parameter_value(
                instance, parameter, texture)
            bound[parameter] = texture_path
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
        report["materials"][instance_path] = bound

    note("instances_done", count=len(report["materials"]))


def normalise_slot(name):
    key = str(name).lower()
    for prefix in ("mi_", "m_", "mat_"):
        if key.startswith(prefix):
            key = key[len(prefix):]
    return key.strip()


def assign_materials(mesh):
    """Bind each slot to its MI by slot name. The combined FBX names its slots
    after the source materials, so match on a normalised key rather than a raw
    string equality that a prefix would break."""
    instances = {}
    for name in MATERIALS:
        instance = unreal.EditorAssetLibrary.load_asset(MAT_DEST + "/MI_" + name)
        if instance:
            instances[normalise_slot(name)] = instance

    slots = mesh.get_editor_property("static_materials")
    assigned = {}
    unassigned = []
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        key = normalise_slot(slot_name)
        instance = instances.get(key)
        if instance is None:
            # fall back to a substring match (slots may be named M_MI_GrayBrick)
            for candidate, value in instances.items():
                if candidate and candidate in key:
                    instance = value
                    break
        if instance is None:
            unassigned.append(slot_name)
            continue
        mesh.set_material(index, instance)
        assigned[slot_name] = instance.get_path_name()

    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    report["meshes"]["material_slots"] = [str(s.get_editor_property("material_slot_name"))
                                          for s in slots]
    report["meshes"]["slots_assigned"] = assigned
    report["meshes"]["slots_unassigned"] = unassigned
    note("materials_assigned", slots=len(slots), assigned=len(assigned),
         unassigned=unassigned)
    if unassigned:
        problem("unassigned material slots: " + repr(unassigned))


def main():
    preflight()
    import_textures()

    mesh = import_mesh()
    import_lods(mesh)
    set_nanite(mesh, MESH_ASSET_NAME)

    master = build_master()
    build_instances(master)
    assign_materials(mesh)

    # Re-assert Nanite last: importing LODs resets fallback_target to AUTO, and
    # that is exactly the state that renders black holes on Metal.
    set_nanite(mesh, MESH_ASSET_NAME)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    bounds = mesh.get_bounds()
    report["meshes"]["bounds_cm"] = {
        "origin": [round(float(bounds.origin.x), 2), round(float(bounds.origin.y), 2),
                   round(float(bounds.origin.z), 2)],
        "extent": [round(float(bounds.box_extent.x), 2),
                   round(float(bounds.box_extent.y), 2),
                   round(float(bounds.box_extent.z), 2)],
    }
    report["meshes"]["triangles"] = int(
        mesh.get_num_triangles(0)) if hasattr(mesh, "get_num_triangles") else None

    report["passed"] = not report["problems"]
    save_manifest()
    print("ZHENGXIMEN_IMPORT_COMPLETE " + json.dumps(
        {"passed": report["passed"], "problems": report["problems"],
         "manifest": str(MANIFEST)}))


if __name__ == "__main__":
    main()
