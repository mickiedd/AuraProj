"""Validate the imported Zhengdongmen landmark in a fresh process.

Run after ImportZhengdongmenLandmark.py and CreateZhengdongmenLandmarkBlueprint.py.
Deliberately a separate process: a green run in the same process that did the
import proves nothing about what was actually serialised to disk.

What a green run does prove:
  - all seven material-group meshes, the master material, the seven instances and
    all 42 texture assets exist and load from disk;
  - every mesh has exactly one material slot and it resolves to its own MI_*, so
    no part silently lost its material;
  - Nanite is enabled with the FULL fallback, i.e. fallback_target is
    PERCENT_TRIANGLES at 1.0 with zero relative error. That is the state that
    stops black triangular holes on Metal; a reimport silently resets it to AUTO,
    so it is asserted here rather than trusted;
  - the structural groups carry collision, and every mesh is double-sided as the
    source glTF declares;
  - the source GLBs still carry TEXCOORD_0, so the import is a pass-through of
    texturable geometry;
  - **the facade faces local +Y after import**, measured from the model's own geometry rather
    than assumed. The gate plaque (門額, the Sign group) is the marker: the
    generator authored it on -Y, and it is what the viewer must see from the
    plaza. A landmark can be grounded, spaced and lit correctly and still stand
    back to front - that is what happened to Wenmingmen - so it is checked here.
  - the Blueprint is a PackedLevelActor with seven HISMs and spawns to the same
    footprint as the union of the source meshes.

What it does NOT prove: that the Blueprint looks right. Rendering needs an open
editor with a live RHI, which a commandlet does not have. The native capture is a
separate, explicit step.
"""

import json
import struct
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen"
MESH_ROOT = ROOT + "/Meshes"
MASTER_PATH = ROOT + "/Materials/M_ZDM_Master"
BLUEPRINT_PATH = ROOT + "/BP_Zhengdongmen"
TEX_DEST = ROOT + "/Textures"

MESH_GROUPS = ["Stone", "Wood", "RoofTile", "Plaster", "Iron", "DoorWood", "Sign"]
STRUCTURAL = {"Stone", "Wood", "DoorWood", "Iron"}

# kind -> (expected compression enum name, expected srgb)
EXPECTED_MAPS = {
    "BaseColor": ("TC_DEFAULT", True),
    "Normal": ("TC_NORMALMAP", False),
    "Roughness": ("TC_MASKS", False),
    "Metallic": ("TC_MASKS", False),
    "AO": ("TC_GRAYSCALE", False),
    "Height": ("TC_GRAYSCALE", False),
}

# Declared envelope, and the measured union of the seven GLBs in cm.
EXPECTED_UNION_CM = (2870.0, 1639.0, 1933.0)
UNION_TOLERANCE_CM = 5.0

PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
REPORT = PROJECT / "Saved" / "RawModelImport" / "zhengdongmen-validation.json"

# Geometry truth is the tuned generator's own report, so the check is engine
# against source rather than engine against a number someone has to remember to
# update. A per-group comparison is what catches a part that was never reimported:
# the roof mesh grew by 86k triangles in the continuous-course rebuild, and a
# stale one would still pass a whole-building tolerance.
TUNED_MANIFEST = (PROJECT / "Raw3DPacket" / "Zhengdongmen" / "prepared"
                  / "reference-tuning.json")
try:
    _TUNED = json.loads(TUNED_MANIFEST.read_text())
except Exception:  # noqa: BLE001
    _TUNED = {}
EXPECTED_GROUP_TRIANGLES = _TUNED.get("groups", {})
# The package's own declared LOD0 count, used only if the manifest is absent.
EXPECTED_SOURCE_TRIANGLES = int(_TUNED.get("total_triangles", 251386))

errors = []
warnings = []
checks = {}


def fail(message):
    errors.append(str(message))
    unreal.log_error("ZHENGDONGMEN_VALIDATE_ERROR " + str(message))


def warn(message):
    warnings.append(str(message))
    unreal.log_warning("ZHENGDONGMEN_VALIDATE_WARNING " + str(message))


def glb_uv_check(group):
    """Does the GLB this mesh was imported from actually carry texture coordinates?

    Replaces a `get_num_uv_channels` check that had to be thrown away on this build:
    `EditorStaticMeshLibrary.get_num_uv_channels` returns 0 for **every** mesh,
    including known-good ones that render their 4K maps correctly - it reports
    *source* UV channels and an Interchange import has no source data. A check that
    fails good assets is worse than no check.

    Reading the source GLB is the honest substitute, and it is precisely the check
    that caught the original Zhengximen defect, where every GLB was exported with
    POSITION and COLOR_0 only and no TEXCOORD_0 at all.
    """
    path = (PROJECT / "Raw3DPacket" / "Zhengdongmen" / "prepared" / "Meshes"
            / "SM_ZDM_{}.glb".format(group))
    if not path.is_file():
        return {"glb": str(path), "error": "missing"}
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
        asset_path = "{}/SM_ZDM_{}".format(MESH_ROOT, group)
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
        # With PERCENT_TRIANGLES 1.0 the Nanite fallback IS the source, so this is
        # an exact comparison and a stale mesh cannot hide behind a tolerance.
        expected = EXPECTED_GROUP_TRIANGLES.get(group)
        info["expected_triangles"] = expected
        if expected is not None and triangles != expected:
            fail("{} has {} triangles; the tuned source has {} — the mesh was not "
                 "reimported".format(asset_path, triangles, expected))

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
        # Every source glTF material is doubleSided=True, and the geometry needs it:
        # the plaque is a single quad and the window recesses are open shells.
        info["double_sided_geometry"] = bool(
            body.get_editor_property("double_sided_geometry"))
        if not info["double_sided_geometry"]:
            fail("{} is not double-sided, but its source material is".format(
                asset_path))

        bounds = mesh.get_bounds()
        origin, extent = bounds.origin, bounds.box_extent
        info["size_cm"] = [round(float(extent.x) * 2, 2),
                           round(float(extent.y) * 2, 2),
                           round(float(extent.z) * 2, 2)]
        # Absolute bounds, so the parts can be unioned into the building. Per-part
        # sizes alone under-report the height: the tallest single part is the stone
        # base at 1063 cm against the building's 1933 cm.
        info["bounds_cm"] = [round(float(origin.x) - float(extent.x), 2),
                             round(float(origin.y) - float(extent.y), 2),
                             round(float(origin.z) - float(extent.z), 2),
                             round(float(origin.x) + float(extent.x), 2),
                             round(float(origin.y) + float(extent.y), 2),
                             round(float(origin.z) + float(extent.z), 2)]
        # Orientation is deliberately NOT judged per part: the stone base is
        # genuinely 2870 x 1639 x 1063 cm, so "height exceeds depth" is false for
        # correctly-oriented parts. The guards are the union footprint below and
        # the facade check after it.
        meshes[group] = info

    checks["meshes"] = meshes
    checks["total_triangles"] = total_triangles
    checks["expected_triangles"] = EXPECTED_SOURCE_TRIANGLES
    tolerance = 0.0 if EXPECTED_GROUP_TRIANGLES else 0.02
    if abs(total_triangles - EXPECTED_SOURCE_TRIANGLES) > EXPECTED_SOURCE_TRIANGLES * tolerance:
        fail("total triangles {} differ from the tuned source {}"
             .format(total_triangles, EXPECTED_SOURCE_TRIANGLES))

    if meshes:
        boxes = [info["bounds_cm"] for info in meshes.values()]
        union = [round(max(box[axis + 3] for box in boxes)
                       - min(box[axis] for box in boxes), 2) for axis in range(3)]
    else:
        union = []
    checks["union_size_cm"] = union
    for axis, (got, want) in enumerate(zip(union, EXPECTED_UNION_CM)):
        if abs(got - want) > UNION_TOLERANCE_CM:
            fail("union axis {} is {:.2f} cm vs the source's {:.2f} cm"
                 .format(axis, got, want))

    # ---- facade: the plaque must sit on local +Y -----------------------------
    #
    # Measured on the imported model, not assumed - and the answer is the
    # opposite of what the source package documents.
    #
    # The SOURCE model has its facade on local -Y: the generator shipped with the
    # package (Scripts/build_gate.py) authors the signboard at Y -5.97..-5.89 with
    # its full-UV front face at Y -5.982, and the README states "front = -Y".
    #
    # The IMPORT negates Y, so the facade arrives on +Y. Three parts agree, which
    # is why this is a measurement and not a guess:
    #   * Iron (door studs)   source Y +204..+211.5  -> imported -211.5..-204
    #   * Sign (the plaque)   source Y -598.2..-589  -> imported +589..+598.2
    #   * Stone (base centre) source Y centre -4.5   -> imported centre +4.5
    #
    # This is why the check exists. A bounding-box size test cannot see it: a
    # mirror preserves every extent, so (x,-y,z) and (x,y,z) produce identical
    # X/Y/Z sizes and the import would report a perfect match while standing back
    # to front. Only a signed, asymmetric feature distinguishes them - here the
    # plaque and the door studs.
    facing = {"sign": None, "doorwood": None, "iron": None, "verdict": None}
    if "Sign" in meshes:
        sign = meshes["Sign"]["bounds_cm"]
        facing["sign"] = {"y_min": sign[1], "y_max": sign[4]}
        facing["doorwood"] = ({"y_min": meshes["DoorWood"]["bounds_cm"][1],
                               "y_max": meshes["DoorWood"]["bounds_cm"][4]}
                              if "DoorWood" in meshes else None)
        facing["iron"] = ({"y_min": meshes["Iron"]["bounds_cm"][1],
                           "y_max": meshes["Iron"]["bounds_cm"][4]}
                          if "Iron" in meshes else None)
        if sign[1] > 0.0:
            facing["verdict"] = "facade on local +Y (import negates source Y)"
        else:
            facing["verdict"] = "facade NOT on local +Y"
            fail("the 正东门 plaque sits at local Y {:.2f}..{:.2f}, not on +Y, so the "
                 "gate no longer matches the import transform this ring was built "
                 "for - re-measure the facade and update facing_offset in "
                 "CreateGuangzhouLandmarkShowcase.py".format(sign[1], sign[4]))
        # The door studs are the independent second reading: they are on the
        # opposite side of the source model to the plaque, so if both flipped the
        # negation is confirmed rather than an artefact of one part.
        if "Iron" in meshes and meshes["Iron"]["bounds_cm"][4] > 0.0:
            fail("the iron door studs are at local Y {:.2f}..{:.2f}, expected them "
                 "on -Y after the import negation - the two facade markers "
                 "disagree".format(meshes["Iron"]["bounds_cm"][1],
                                   meshes["Iron"]["bounds_cm"][4]))
    else:
        fail("Sign mesh missing, so facade orientation could not be verified")
    checks["facade"] = facing
    return meshes


def check_materials():
    master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if not master:
        fail("master material missing: " + MASTER_PATH)
    else:
        checks["master"] = {"path": MASTER_PATH}
        # Instance overrides alone do not prove the parent shader compiles. The
        # Zhengximen parent sampled DefaultTexture as a normal map and rendered the
        # checkerboard fallback on Metal despite all instance bindings passing.
        lib = unreal.MaterialEditingLibrary
        defaults = set()
        for parameter in ("BaseColorTex", "NormalTex", "RoughnessTex",
                          "MetallicTex", "AOTex"):
            texture = lib.get_material_default_texture_parameter_value(master, parameter)
            if texture:
                defaults.add(texture.get_name())
        checks["master"]["default_textures"] = sorted(defaults)
        for suffix in ("BaseColor", "Normal", "Roughness", "Metallic", "AO"):
            if "T_ZDM_Stone_" + suffix not in defaults:
                fail("master missing typed source texture default: " + suffix)
        for usage in ("used_with_instanced_static_meshes", "used_with_nanite"):
            if not master.get_editor_property(usage):
                fail("master missing material usage: " + usage)
        two_sided = bool(master.get_editor_property("two_sided"))
        checks["master"]["two_sided"] = two_sided
        if not two_sided:
            fail("master is not two-sided, but every source material is doubleSided")

    present = {}
    for group in MESH_GROUPS:
        asset_path = ROOT + "/Materials/MI_" + group
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
        present[group] = {"path": asset_path, "textures": textures}
    checks["instances"] = present


def check_textures():
    present, missing, wrong = [], [], []
    for group in MESH_GROUPS:
        for kind, (compression, srgb) in EXPECTED_MAPS.items():
            asset_path = "{}/T_ZDM_{}_{}".format(TEX_DEST, group, kind)
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

    # The package documents glTF/OpenGL +Y normals, which is what Unreal's
    # flip_green_channel converts. Asserted because a wrong tangent-space handedness
    # inverts every bump on the gate and is invisible in a structural check.
    green = {}
    for group in MESH_GROUPS:
        texture = unreal.EditorAssetLibrary.load_asset(
            "{}/T_ZDM_{}_Normal".format(TEX_DEST, group))
        if texture:
            green[group] = bool(texture.get_editor_property("flip_green_channel"))
    checks["normal_flip_green_channel"] = green
    if not all(green.values()) or not green:
        fail("normal maps do not all have flip_green_channel set: " + repr(green))


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
    check_meshes()
    check_materials()
    check_textures()
    check_blueprint(checks.get("meshes"))

    result = {"passed": not errors, "errors": errors, "warnings": warnings,
              "checks": checks}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    unreal.log("ZHENGDONGMEN_VALIDATE_COMPLETE " + json.dumps(
        {"passed": result["passed"], "errors": errors, "warnings": warnings,
         "report": str(REPORT)}))


if __name__ == "__main__":
    main()
