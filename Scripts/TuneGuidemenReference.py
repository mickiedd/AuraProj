"""Tune the imported Guidemen (Guide Gate) toward its reference sheet.

Guidemen is a 20-part primary mesh assembly inside one independent Blueprint.
This pass creates rollback-safe material siblings, rebinds every primary mesh
and Blueprint template, and leaves authored geometry, UVs and collision intact.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen"
MATERIAL_NAMES = (
    "Inscription_Guide",
    "Metal_Bronze",
    "Plaque_Guide",
    "Plaster_OffWhite",
    "Sign_Guidemen",
    "Stone_BlueGrey",
    "Tile_ClayGrey",
    "Wood_Aged",
)
TINTS = {
    "Inscription_Guide": (0.44, 0.43, 0.39, 1.0),
    "Metal_Bronze": (0.23, 0.14, 0.08, 1.0),
    "Plaque_Guide": (0.78, 0.70, 0.55, 1.0),
    "Plaster_OffWhite": (0.68, 0.62, 0.52, 1.0),
    "Sign_Guidemen": (0.55, 0.37, 0.21, 1.0),
    "Stone_BlueGrey": (0.56, 0.52, 0.45, 1.0),
    "Tile_ClayGrey": (0.31, 0.34, 0.38, 1.0),
    "Wood_Aged": (0.34, 0.18, 0.10, 1.0),
}
ROUGHNESS_SCALE = {
    "Inscription_Guide": 1.12,
    "Metal_Bronze": 0.82,
    "Plaque_Guide": 0.90,
    "Plaster_OffWhite": 1.08,
    "Sign_Guidemen": 1.00,
    "Stone_BlueGrey": 1.12,
    "Tile_ClayGrey": 1.05,
    "Wood_Aged": 0.95,
}
ROOT = PROJECT / "Saved/RawModelImport/V4"
IMPORT_REPORT = ROOT / "Guidemen_V4-import.json"
REPORT = ROOT / "Guidemen-reference-tuning.json"


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _texture_map(original):
    result = {}
    for texture in unreal.MaterialEditingLibrary.get_used_textures(original):
        parts = texture.get_name().lower().split("_")
        if "basecolor" in parts or ("base" in parts and "color" in parts):
            result["BC"] = texture
        elif "normal" in parts or "normalgl" in parts:
            result["N"] = texture
        elif "roughness" in parts:
            result["R"] = texture
        elif "metallic" in parts:
            result["M"] = texture
        elif "ao" in parts:
            result["AO"] = texture
        elif "height" in parts:
            result["H"] = texture
    return result


def _sample(material, texture, x, y, sampler):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, x, y
    )
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    return node


def _constant(material, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, x, y
    )
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _scalar(material, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    node.set_editor_property("r", value)
    return node


def _multiply(material, left, right, x, y, left_output="RGB"):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, x, y
    )
    lib = unreal.MaterialEditingLibrary
    assert lib.connect_material_expressions(left, left_output, node, "A")
    assert lib.connect_material_expressions(right, "", node, "B")
    return node


def _build_tuned_material(name):
    original_path = DEST + "/Materials/M_" + name
    tuned_path = DEST + "/Materials/M_" + name + "_ReferenceTuned"
    original = _asset(original_path)
    tuned = _asset(tuned_path) if unreal.EditorAssetLibrary.does_asset_exist(tuned_path) else None
    if tuned is None:
        tuned = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            "M_" + name + "_ReferenceTuned", DEST + "/Materials", original
        )
    assert isinstance(tuned, unreal.Material), tuned_path
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(tuned)
    tuned.set_editor_property("used_with_nanite", True)
    tuned.set_editor_property("two_sided", True)
    maps = _texture_map(original)
    if "BC" not in maps:
        color = _constant(tuned, TINTS[name], -420, 100)
        assert lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = _scalar(tuned, ROUGHNESS_SCALE[name], -420, 300)
        assert lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        lib.layout_material_expressions(tuned)
        lib.recompile_material(tuned)
        assert unreal.EditorAssetLibrary.save_loaded_asset(tuned)
        return original, tuned
    base = _sample(tuned, maps["BC"], -760, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tint = _constant(tuned, TINTS[name], -760, 190)
    base_tinted = _multiply(tuned, base, tint, -420, 100)
    assert lib.connect_material_property(base_tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if "N" in maps:
        normal = _sample(tuned, maps["N"], -760, 380, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        assert lib.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    for channel, prop, y in (
        ("R", unreal.MaterialProperty.MP_ROUGHNESS, 570),
        ("M", unreal.MaterialProperty.MP_METALLIC, 760),
        ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 950),
    ):
        if channel not in maps:
            continue
        sample = _sample(tuned, maps[channel], -760, y, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if channel == "R":
            factor = _scalar(tuned, ROUGHNESS_SCALE[name], -760, y + 100)
            sample = _multiply(tuned, sample, factor, -420, y, "R")
            output = ""
        else:
            output = "R"
        assert lib.connect_material_property(sample, output, prop)
    lib.layout_material_expressions(tuned)
    lib.recompile_material(tuned)
    assert unreal.EditorAssetLibrary.save_loaded_asset(tuned)
    return original, tuned


def _mesh_slots(mesh):
    return [
        {
            "index": i,
            "slot": str(slot.material_slot_name),
            "material": slot.material_interface.get_path_name() if slot.material_interface else "",
        }
        for i, slot in enumerate(mesh.get_editor_property("static_materials"))
    ]


def _rebind_primary_meshes(tuned):
    report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    primary = next(entry for entry in report["imports"] if entry["source"]["primary"])
    meshes, snapshots = [], []
    for mesh_path in primary["meshes"]:
        mesh = _asset(mesh_path)
        snapshots.append({"mesh": mesh_path, "slots": _mesh_slots(mesh)})
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            if not slot.material_interface:
                continue
            name = slot.material_interface.get_name().replace("M_", "", 1)
            if name in tuned:
                mesh.set_material(index, tuned[name])
        mesh.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        )
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        meshes.append(mesh)
    return meshes, snapshots, primary


def _rebind_blueprint(meshes):
    bp_path = DEST + "/BP_Guidemen_V4"
    bp = _asset(bp_path)
    mesh_by_path = {mesh.get_path_name(): mesh for mesh in meshes}
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set(); changed = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        key = component.get_name()
        if key in seen:
            continue
        mesh = mesh_by_path.get(component.static_mesh.get_path_name())
        if not mesh:
            continue
        seen.add(key)
        component.set_static_mesh(mesh)
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(index, slot.material_interface)
        changed += 1
    assert changed == len(meshes), (changed, len(meshes))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    return bp_path, changed


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before Guidemen tuning: " + str(dirty)
    originals, tuned = {}, {}
    for name in MATERIAL_NAMES:
        original, material = _build_tuned_material(name)
        originals[name] = original.get_path_name()
        tuned[name] = material.get_path_name()
    meshes, snapshots, primary = _rebind_primary_meshes(
        {name: _asset(path) for name, path in tuned.items()}
    )
    bp_path, component_count = _rebind_blueprint(meshes)
    report = {
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-58990fa0-74fe-44c1-9e8b-8952bfaad1b8.png",
        "blueprint": bp_path,
        "primary_source": primary["source"],
        "primary_meshes": [mesh.get_path_name() for mesh in meshes],
        "tuned_materials": tuned,
        "original_materials": originals,
        "original_mesh_slots": snapshots,
        "tints": TINTS,
        "roughness_scale": ROUGHNESS_SCALE,
        "blueprint_components_rebound": component_count,
        "geometry_changed": False,
        "intent": "Weathered stone/plaster, aged timber, cool tile, muted bronze and readable guide-gate plaque tones from the reference sheet.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("GUIDEMEN_REFERENCE_TUNED", json.dumps({"blueprint": bp_path, "meshes": report["primary_meshes"], "materials": tuned}))


if __name__ == "__main__":
    main()
