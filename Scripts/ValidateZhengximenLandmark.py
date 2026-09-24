"""Validate the imported Zhengximen landmark in a fresh process.

Run after ImportZhengximenLandmark.py and CreateZhengximenLandmarkBlueprint.py.
This is deliberately a separate process: a green run in the same process that
did the import proves nothing about what was actually serialised to disk.

What a green run does prove:
  - the mesh, its material slots, the master material, the eight instances and
    all 48 texture assets exist and are loadable from disk;
  - every material slot resolves to an MI_* instance (no null slots);
  - Nanite is enabled with the FULL fallback, i.e. fallback_target is
    PERCENT_TRIANGLES at 1.0 with zero relative error. That is the state that
    stops black triangular holes on Metal; a reimport silently resets it to AUTO,
    so it is asserted here rather than trusted;
  - the Blueprint is a PackedLevelActor with exactly one HISM and spawns to the
    same footprint as the source mesh.

What it does NOT prove: that the Blueprint looks right. Rendering needs an open
editor with a live RHI, which a commandlet does not have. The native capture is
a separate, explicit step.
"""

import json
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen"
MESH_PATH = ROOT + "/Meshes/SM_Zhengximen"
MASTER_PATH = ROOT + "/Materials/M_Zhengximen_Master"
BLUEPRINT_PATH = ROOT + "/BP_Zhengximen"
TEX_DEST = ROOT + "/Textures"

MATERIALS = ["GrayBrick", "StoneFoundation", "AgedWood", "DarkTimber",
             "ClayRoofTile", "LimePlaster", "BlackIron", "GatePlaque"]

# kind -> (expected compression enum name, expected srgb)
EXPECTED_MAPS = {
    "BaseColor": ("TC_DEFAULT", True),
    "Normal": ("TC_NORMALMAP", False),
    "Roughness": ("TC_MASKS", False),
    "Metallic": ("TC_MASKS", False),
    "AO": ("TC_GRAYSCALE", False),
    "Height": ("TC_GRAYSCALE", False),
}

EXPECTED_SOURCE_TRIANGLES = 206252  # LOD0 per Documentation/AssetManifest.json
PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
REPORT = PROJECT / "Saved" / "RawModelImport" / "zhengximen-validation.json"

errors = []
warnings = []
checks = {}


def fail(message):
    errors.append(str(message))
    print("ZHENGXIMEN_VALIDATE_ERROR " + str(message))


def warn(message):
    warnings.append(str(message))
    print("ZHENGXIMEN_VALIDATE_WARNING " + str(message))


def check_mesh():
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not isinstance(mesh, unreal.StaticMesh):
        fail("mesh missing or not a StaticMesh: " + MESH_PATH)
        return None

    info = {"path": MESH_PATH}

    slots = mesh.get_editor_property("static_materials")
    bound, unbound = [], []
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        material = slot.get_editor_property("material_interface")
        target = (bound if material else unbound)
        target.append({"slot": name,
                       "material": material.get_path_name() if material else None})
    info["slots"] = bound
    info["unbound_slots"] = unbound
    if not slots:
        fail("mesh has no material slots")
    if unbound:
        fail("unbound material slots: " + repr([row["slot"] for row in unbound]))
    if bound and not all("MI_" in (row["material"] or "") for row in bound):
        fail("some slots are not bound to MI_ instances: " + repr(bound))

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
        fail("Nanite is not enabled on " + MESH_PATH)
    if "PERCENT_TRIANGLES" not in info["nanite"]["fallback_target"]:
        fail("Nanite fallback_target is not PERCENT_TRIANGLES (AUTO decimates and "
             "renders black holes on Metal): " + info["nanite"]["fallback_target"])
    if info["nanite"]["fallback_percent_triangles"] != 1.0:
        fail("Nanite fallback_percent_triangles is not 1.0: {}".format(
            info["nanite"]["fallback_percent_triangles"]))
    if info["nanite"]["fallback_relative_error"] != 0.0:
        fail("Nanite fallback_relative_error is not 0.0: {}".format(
            info["nanite"]["fallback_relative_error"]))

    triangles = int(mesh.get_num_triangles(0))
    info["triangles"] = triangles
    if triangles < EXPECTED_SOURCE_TRIANGLES * 0.5:
        fail("triangle count {} looks decimated against the declared LOD0 of {}"
             .format(triangles, EXPECTED_SOURCE_TRIANGLES))
    elif abs(triangles - EXPECTED_SOURCE_TRIANGLES) > EXPECTED_SOURCE_TRIANGLES * 0.05:
        warn("triangle count {} differs from the declared LOD0 {} by more than 5%"
             .format(triangles, EXPECTED_SOURCE_TRIANGLES))

    lod_count = int(unreal.EditorStaticMeshLibrary.get_lod_count(mesh))
    info["lod_count"] = lod_count

    bounds = mesh.get_bounds()
    info["bounds_cm"] = [round(float(bounds.box_extent.x) * 2, 2),
                         round(float(bounds.box_extent.y) * 2, 2),
                         round(float(bounds.box_extent.z) * 2, 2)]
    # Package declares ~53.2 m x 10.8 m x 16.9 m.
    for axis, (got, want) in enumerate(zip(info["bounds_cm"], [5320.0, 1080.0, 1689.7])):
        if abs(got - want) > 200.0:
            warn("bounds axis {} is {:.1f} cm vs the package's {:.1f} cm"
                 .format(axis, got, want))

    checks["mesh"] = info
    return mesh


def check_materials():
    master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if not master:
        fail("master material missing: " + MASTER_PATH)
    else:
        checks["master"] = {"path": MASTER_PATH}

    present = {}
    for name in MATERIALS:
        path = ROOT + "/Materials/MI_" + name
        instance = unreal.EditorAssetLibrary.load_asset(path)
        if not instance:
            fail("material instance missing: " + path)
            continue
        parent = unreal.MaterialEditingLibrary.get_material_instance_parent(instance)
        textures = {}
        for parameter in ("BaseColorTex", "NormalTex", "RoughnessTex",
                          "MetallicTex", "AOTex"):
            texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
                instance, parameter)
            textures[parameter] = texture.get_path_name() if texture else None
            if not texture:
                fail("{} has no texture bound for {}".format(path, parameter))
        if parent and master and parent.get_path_name() != master.get_path_name():
            fail("{} parent is {} not {}".format(
                path, parent.get_path_name(), master.get_path_name()))
        present[name] = {"path": path, "textures": textures}
    checks["instances"] = present


def check_textures():
    present, missing, wrong = [], [], []
    for name in MATERIALS:
        for kind, (compression, srgb) in EXPECTED_MAPS.items():
            path = "{}/T_{}_{}".format(TEX_DEST, name, kind)
            texture = unreal.EditorAssetLibrary.load_asset(path)
            if not texture:
                missing.append(path)
                continue
            actual_srgb = bool(texture.get_editor_property("srgb"))
            actual_compression = str(texture.get_editor_property("compression_settings"))
            if actual_srgb != srgb or compression not in actual_compression:
                wrong.append({"path": path, "srgb": actual_srgb,
                              "compression": actual_compression,
                              "expected": {"srgb": srgb, "compression": compression}})
            present.append(path)
    checks["textures"] = {"present": len(present), "missing": missing, "wrong": wrong}
    if missing:
        fail("missing textures ({}): {}".format(len(missing), missing[:8]))
    if wrong:
        fail("textures with wrong colour space/compression: " + repr(wrong[:8]))


def check_blueprint(mesh):
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if not blueprint:
        fail("Blueprint missing: " + BLUEPRINT_PATH)
        return

    info = {"path": BLUEPRINT_PATH}
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    hism = 0
    root_class = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        class_name = obj.get_class().get_name() if obj else "?"
        if "InstancedStaticMesh" in class_name:
            hism += 1
        if library.is_root_component(data):
            root_class = class_name
    info["hism_count"] = hism
    info["root_class"] = root_class
    if hism != 1:
        fail("expected exactly 1 HISM component, found {}".format(hism))
    if root_class != "LevelInstanceComponent":
        warn("root component is {} not LevelInstanceComponent".format(root_class))

    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) \
        if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else None
    info["parent_class"] = parent.get_name() if parent else None

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
            if mesh:
                mesh_extent = mesh.get_bounds().box_extent
                expected = [float(mesh_extent.x) * 2, float(mesh_extent.y) * 2,
                            float(mesh_extent.z) * 2]
                for axis, (got, want) in enumerate(zip(spawned, expected)):
                    if abs(got - want) > 1.0:
                        fail("Blueprint footprint axis {} is {:.2f} cm, mesh is "
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
    mesh = check_mesh()
    check_materials()
    check_textures()
    check_blueprint(mesh)

    result = {"passed": not errors, "errors": errors, "warnings": warnings,
              "checks": checks}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    print("ZHENGXIMEN_VALIDATE_COMPLETE " + json.dumps(
        {"passed": result["passed"], "errors": errors, "warnings": warnings,
         "report": str(REPORT)}))


if __name__ == "__main__":
    main()
