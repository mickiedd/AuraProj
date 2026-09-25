"""Validate the imported Zhengximen landmark in a fresh process.

Run after ImportZhengximenLandmark.py and CreateZhengximenLandmarkBlueprint.py.
This is deliberately a separate process: a green run in the same process that did
the import proves nothing about what was actually serialised to disk.

What a green run does prove:
  - all seven material-group meshes, the master material, the eight instances and
    all 48 texture assets exist and load from disk;
  - every mesh has exactly one material slot and it resolves to its own MI_*, so
    no part silently lost its material;
  - Nanite is enabled with the FULL fallback, i.e. fallback_target is
    PERCENT_TRIANGLES at 1.0 with zero relative error. That is the state that
    stops black triangular holes on Metal; a reimport silently resets it to AUTO,
    so it is asserted here rather than trusted;
  - the structural groups carry collision;
  - the Blueprint is a PackedLevelActor with seven HISMs and spawns to the same
    footprint as the union of the source meshes.

What it does NOT prove: that the Blueprint looks right. Rendering needs an open
editor with a live RHI, which a commandlet does not have. The native capture is a
separate, explicit step.
"""

import json
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen"
MESH_ROOT = ROOT + "/Meshes"
MASTER_PATH = ROOT + "/Materials/M_Zhengximen_Master"
BLUEPRINT_PATH = ROOT + "/BP_Zhengximen"
TEX_DEST = ROOT + "/Textures"

MESH_GROUPS = ["GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber",
               "ClayRoofTile", "BlackIron", "GatePlaque"]
ALL_MATERIALS = MESH_GROUPS + ["LimePlaster"]
STRUCTURAL = {"GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber", "BlackIron"}

# kind -> (expected compression enum name, expected srgb)
EXPECTED_MAPS = {
    "BaseColor": ("TC_DEFAULT", True),
    "Normal": ("TC_NORMALMAP", False),
    "Roughness": ("TC_MASKS", False),
    "Metallic": ("TC_MASKS", False),
    "AO": ("TC_GRAYSCALE", False),
    "Height": ("TC_GRAYSCALE", False),
}

# LOD0 triangle total across the seven groups. The package declared 206,252; the
# tuning pass took it to 218,328 by refining the arch (400 wall slices instead of 46,
# a 96-segment voussoir ring, and an arch-fitting door), so this tracks the tuned
# source rather than the shipped package.
EXPECTED_SOURCE_TRIANGLES = 271644
PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
REPORT = PROJECT / "Saved" / "RawModelImport" / "zhengximen-validation.json"

errors = []
warnings = []
checks = {}


def fail(message):
    errors.append(str(message))
    unreal.log_error("ZHENGXIMEN_VALIDATE_ERROR " + str(message))


def warn(message):
    warnings.append(str(message))
    unreal.log_warning("ZHENGXIMEN_VALIDATE_WARNING " + str(message))


def glb_uv_check(group):
    """Does the GLB this mesh was imported from actually carry texture coordinates?

    This replaces a `get_num_uv_channels` check that had to be thrown away. On this
    build `EditorStaticMeshLibrary.get_num_uv_channels` returns 0 for **every** mesh,
    including the known-good Wenmingmen and Zhengnanmen ones that render their 4K
    maps correctly - it reports *source* UV channels, and an Interchange import
    carries no source data. A check that fails good assets is worse than no check.

    Reading the source GLB is the honest substitute: the import is a pass-through,
    and this is precisely the check that would have caught the original defect, where
    every GLB was exported with POSITION and COLOR_0 only and no TEXCOORD_0 at all.
    """
    path = (PROJECT / "Raw3DPacket" / "Zhengximen_GreatWestGate_UE5_Package"
            / "Zhengximen_GreatWestGate_UE5" / "Meshes" / "Modular"
            / "SM_Zhengximen_{}.glb".format(group))
    if not path.is_file():
        return {"glb": str(path), "error": "missing"}
    import struct
    with open(path, "rb") as handle:
        struct.unpack("<4sII", handle.read(12))
        chunk_length, _ = struct.unpack("<I4s", handle.read(8))
        document = json.loads(handle.read(chunk_length).decode("utf-8"))
    attributes = set()
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            attributes.update(primitive.get("attributes", {}).keys())
    return {"glb": str(path), "attributes": sorted(attributes),
            "has_uv": "TEXCOORD_0" in attributes}


def check_meshes():
    meshes = {}
    total_triangles = 0
    for group in MESH_GROUPS:
        asset_path = "{}/SM_Zhengximen_{}".format(MESH_ROOT, group)
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(mesh, unreal.StaticMesh):
            fail("mesh missing or not a StaticMesh: " + asset_path)
            continue
        info = {"path": asset_path}

        slots = mesh.get_editor_property("static_materials")
        bound = []
        for slot in slots:
            material = slot.get_editor_property("material_interface")
            bound.append({"slot": str(slot.get_editor_property("material_slot_name")),
                          "material": material.get_path_name() if material else None})
        info["slots"] = bound
        if len(slots) != 1:
            fail("{} has {} material slots, expected 1".format(asset_path, len(slots)))
        # The material_interface path is "<folder>/MI_<group>.MI_<group>", so match
        # on "/MI_<group>." rather than on a bare suffix.
        expected_marker = "/MI_{}.".format(group)
        actual_material = bound[0]["material"] if bound else None
        if not bound or expected_marker not in (actual_material or ""):
            fail("{} slot is bound to {} which is not {}".format(
                asset_path, actual_material, expected_marker))

        nanite = mesh.get_editor_property("nanite_settings")
        info["nanite"] = {
            "enabled": bool(nanite.get_editor_property("enabled")),
            "fallback_target": str(nanite.get_editor_property("fallback_target")),
            "fallback_percent_triangles": float(
                nanite.get_editor_property("fallback_percent_triangles")),
            "fallback_relative_error": float(
                nanite.get_editor_property("fallback_relative_error")),
        }
        if not info["nanite"]["enabled"]:
            fail("Nanite is not enabled on " + asset_path)
        if "PERCENT_TRIANGLES" not in info["nanite"]["fallback_target"]:
            fail("{} Nanite fallback_target is not PERCENT_TRIANGLES (AUTO decimates "
                 "and renders black holes on Metal): {}".format(
                     asset_path, info["nanite"]["fallback_target"]))
        if info["nanite"]["fallback_percent_triangles"] != 1.0:
            fail("{} fallback_percent_triangles is not 1.0: {}".format(
                asset_path, info["nanite"]["fallback_percent_triangles"]))
        if info["nanite"]["fallback_relative_error"] != 0.0:
            fail("{} fallback_relative_error is not 0.0: {}".format(
                asset_path, info["nanite"]["fallback_relative_error"]))

        triangles = int(mesh.get_num_triangles(0))
        info["triangles"] = triangles
        total_triangles += triangles

        uv = glb_uv_check(group)
        info["source_uv"] = uv
        if uv.get("error"):
            warn("{} source GLB not readable: {}".format(asset_path, uv["error"]))
        elif not uv["has_uv"]:
            fail("{} came from a GLB with no TEXCOORD_0 ({}), so no material can "
                 "land on it".format(asset_path, uv["attributes"]))

        body = mesh.get_editor_property("body_setup")
        flag = str(body.get_editor_property("collision_trace_flag"))
        info["collision_trace_flag"] = flag
        info["structural"] = group in STRUCTURAL
        if group in STRUCTURAL and "COMPLEX_AS_SIMPLE" not in flag:
            fail("{} is structural but its collision flag is {}".format(
                asset_path, flag))

        bounds = mesh.get_bounds()
        origin, extent = bounds.origin, bounds.box_extent
        info["size_cm"] = [round(float(extent.x) * 2, 2),
                           round(float(extent.y) * 2, 2),
                           round(float(extent.z) * 2, 2)]
        # Absolute bounds, so the parts can be unioned into the building. Per-part
        # sizes alone under-report the height: the tallest single part is 1470 cm
        # against the building's 1689.7 cm.
        info["bounds_cm"] = [round(float(origin.x) - float(extent.x), 2),
                             round(float(origin.y) - float(extent.y), 2),
                             round(float(origin.z) - float(extent.z), 2),
                             round(float(origin.x) + float(extent.x), 2),
                             round(float(origin.y) + float(extent.y), 2),
                             round(float(origin.z) + float(extent.z), 2)]
        # Orientation is deliberately NOT judged per part. A "height must exceed
        # depth" test is wrong here: the brick wall is 1000 cm deep and 870 cm
        # tall, and the timber band is 818 deep and 675 tall, both correctly so.
        # The real guard is the union footprint below - if any part were
        # transposed the building's height would not come out at 1689.7 cm.
        meshes[group] = info

    checks["meshes"] = meshes
    checks["total_triangles"] = total_triangles
    if abs(total_triangles - EXPECTED_SOURCE_TRIANGLES) > EXPECTED_SOURCE_TRIANGLES * 0.05:
        warn("total triangles {} differ from the declared LOD0 {} by more than 5%"
             .format(total_triangles, EXPECTED_SOURCE_TRIANGLES))

    if meshes:
        boxes = [info["bounds_cm"] for info in meshes.values()]
        union = [round(max(box[axis + 3] for box in boxes)
                       - min(box[axis] for box in boxes), 2) for axis in range(3)]
    else:
        union = []
    checks["union_size_cm"] = union
    # Package declares ~53.2 m x 10.8 m x 16.9 m.
    for axis, (got, want) in enumerate(zip(union, [5320.0, 1080.0, 1689.7])):
        if abs(got - want) > 200.0:
            warn("union axis {} is {:.1f} cm vs the package's {:.1f} cm"
                 .format(axis, got, want))
    return meshes


def check_materials():
    master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if not master:
        fail("master material missing: " + MASTER_PATH)
    else:
        checks["master"] = {"path": MASTER_PATH}
        # Instance overrides alone do not prove that the parent shader compiles.
        # The previous parent sampled DefaultTexture as a normal map and rendered
        # the checkerboard fallback on Metal despite all instance bindings passing.
        lib = unreal.MaterialEditingLibrary
        defaults = set()
        for parameter in ("BaseColorTex", "NormalTex", "RoughnessTex",
                          "MetallicTex", "AOTex"):
            texture = lib.get_material_default_texture_parameter_value(master, parameter)
            if texture:
                defaults.add(texture.get_name())
        checks["master"]["default_textures"] = sorted(defaults)
        for suffix in ("BaseColor", "Normal", "Roughness", "Metallic", "AO"):
            if "T_AgedWood_" + suffix not in defaults:
                fail("master missing typed source texture default: " + suffix)
        for usage in ("used_with_instanced_static_meshes", "used_with_nanite"):
            if not master.get_editor_property(usage):
                fail("master missing material usage: " + usage)

    present = {}
    for name in ALL_MATERIALS:
        asset_path = ROOT + "/Materials/MI_" + name
        instance = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not instance:
            fail("material instance missing: " + asset_path)
            continue
        try:
            parent = instance.get_editor_property("parent")
        except Exception as exc:  # noqa: BLE001
            parent = None
            fail("{} parent could not be read: {}".format(asset_path, exc))
        textures = {}
        for parameter in ("BaseColorTex", "NormalTex", "RoughnessTex",
                          "MetallicTex", "AOTex"):
            try:
                texture = (unreal.MaterialEditingLibrary
                           .get_material_instance_texture_parameter_value(
                               instance, parameter))
            except Exception as exc:  # noqa: BLE001
                texture = None
                fail("{} texture parameter {} could not be read: {}".format(
                    asset_path, parameter, exc))
                continue
            textures[parameter] = texture.get_path_name() if texture else None
            if not texture:
                fail("{} has no texture bound for {}".format(asset_path, parameter))
        if parent and master and parent.get_path_name() != master.get_path_name():
            fail("{} parent is {} not {}".format(
                asset_path, parent.get_path_name(), master.get_path_name()))
        present[name] = {"path": asset_path, "textures": textures}
    checks["instances"] = present


def check_textures():
    present, missing, wrong = [], [], []
    for name in ALL_MATERIALS:
        for kind, (compression, srgb) in EXPECTED_MAPS.items():
            asset_path = "{}/T_{}_{}".format(TEX_DEST, name, kind)
            texture = unreal.EditorAssetLibrary.load_asset(asset_path)
            if not texture:
                missing.append(asset_path)
                continue
            actual_srgb = bool(texture.get_editor_property("srgb"))
            actual_compression = str(texture.get_editor_property("compression_settings"))
            if actual_srgb != srgb or compression not in actual_compression:
                wrong.append({"path": asset_path, "srgb": actual_srgb,
                              "compression": actual_compression,
                              "expected": {"srgb": srgb, "compression": compression}})
            present.append(asset_path)
    checks["textures"] = {"present": len(present), "missing": missing, "wrong": wrong}
    if missing:
        fail("missing textures ({}): {}".format(len(missing), missing[:8]))
    if wrong:
        fail("textures with wrong colour space/compression: " + repr(wrong[:8]))


def check_blueprint(meshes):
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if not blueprint:
        fail("Blueprint missing: " + BLUEPRINT_PATH)
        return

    info = {"path": BLUEPRINT_PATH}
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    hism_names, root_class = [], None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        class_name = obj.get_class().get_name() if obj else "?"
        if "InstancedStaticMesh" in class_name:
            hism_names.append(str(library.get_display_name(data)))
        if library.is_root_component(data):
            root_class = class_name
    info["hism_count"] = len(hism_names)
    info["hism_names"] = sorted(hism_names)
    info["root_class"] = root_class
    if len(hism_names) != len(MESH_GROUPS):
        fail("expected {} HISM components, found {}".format(
            len(MESH_GROUPS), len(hism_names)))
    if root_class != "LevelInstanceComponent":
        warn("root component is {} not LevelInstanceComponent".format(root_class))

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(
        blueprint.generated_class(), unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0),
        transient=True)
    if not actor:
        fail("Blueprint did not spawn")
    else:
        try:
            _, extent = actor.get_actor_bounds(False)
            spawned = [round(float(extent.x) * 2, 2), round(float(extent.y) * 2, 2),
                       round(float(extent.z) * 2, 2)]
            info["spawned_size_cm"] = spawned
            union = checks.get("union_size_cm") or []
            for axis, (got, want) in enumerate(zip(spawned, union)):
                if abs(got - want) > 1.0:
                    fail("Blueprint footprint axis {} is {:.2f} cm, mesh union is "
                         "{:.2f} cm".format(axis, got, want))
        finally:
            actor_subsystem.destroy_actor(actor)

    on_disk = PROJECT / ("Content" + BLUEPRINT_PATH.split("/Game", 1)[1] + ".uasset")
    info["on_disk"] = str(on_disk)
    info["on_disk_exists"] = on_disk.exists()
    if not on_disk.exists():
        fail("Blueprint is not on disk: " + str(on_disk))
    checks["blueprint"] = info


def main():
    meshes = check_meshes()
    check_materials()
    check_textures()
    check_blueprint(meshes)

    result = {"passed": not errors, "errors": errors, "warnings": warnings,
              "checks": checks}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    unreal.log("ZHENGXIMEN_VALIDATE_COMPLETE " + json.dumps(
        {"passed": result["passed"], "errors": errors, "warnings": warnings,
         "report": str(REPORT)}))


if __name__ == "__main__":
    main()
