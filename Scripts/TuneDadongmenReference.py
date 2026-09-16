"""Tune the imported Dadongmen materials and Blueprint toward the supplied reference.

The source is intentionally kept intact.  This creates sibling tuned materials,
rebinds the Dadongmen mesh slots and updates the existing Blueprint component.
The original materials and mesh remain available for rollback.
Run from Unreal Editor with a saved map.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
MATERIAL_NAMES = ("Stone", "Plaster", "Wood", "RoofClay", "Dirt", "Water", "Vegetation")
TINTS = {
    # Reference: weathered, warm gray stone with darker mortar.
    "Stone": (0.62, 0.55, 0.45, 1.0),
    # Reference: aged lime plaster, warmer and less blown out than the import.
    "Plaster": (0.72, 0.62, 0.48, 1.0),
    # Reference: deep walnut timber with readable grain.
    "Wood": (0.42, 0.25, 0.14, 1.0),
    # Reference: cool, charcoal blue-gray clay tile.
    "RoofClay": (0.44, 0.48, 0.53, 1.0),
    # Reference: desaturated earth around the approach.
    "Dirt": (0.58, 0.48, 0.34, 1.0),
    # Reference: darker blue-green water at the left edge.
    "Water": (0.28, 0.43, 0.45, 1.0),
    "Vegetation": (0.18, 0.30, 0.10, 1.0),
}
ROUGHNESS_SCALE = {
    "Stone": 1.12,
    "Plaster": 1.08,
    "Wood": 0.92,
    "RoofClay": 1.05,
    "Dirt": 1.12,
    "Water": 0.68,
    "Vegetation": 0.90,
}
ROOT = Path("C:/Git/AuraProj/Saved/RawModelImport/V4")
REPORT = ROOT / "Dadongmen-reference-tuning.json"


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _texture_map(original):
    result = {}
    for texture in unreal.MaterialEditingLibrary.get_used_textures(original):
        name = texture.get_name()
        for channel in ("BC", "N", "R", "M", "AO", "H"):
            if ("_" + channel + "_") in name or name.endswith("_" + channel):
                result[channel] = texture
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
    assert unreal.MaterialEditingLibrary.connect_material_expressions(left, left_output, node, "A")
    if isinstance(right, unreal.MaterialExpressionConstant):
        assert unreal.MaterialEditingLibrary.connect_material_expressions(right, "", node, "B")
    else:
        assert unreal.MaterialEditingLibrary.connect_material_expressions(right, "", node, "B")
    return node


def _build_tuned_material(name):
    original_path = DEST + "/Materials/M_Dadongmen_" + name
    tuned_path = DEST + "/Materials/M_Dadongmen_" + name + "_ReferenceTuned"
    original = _asset(original_path)
    tuned = _asset(tuned_path) if unreal.EditorAssetLibrary.does_asset_exist(tuned_path) else None
    if tuned is None:
        tuned = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            "M_Dadongmen_" + name + "_ReferenceTuned",
            DEST + "/Materials",
            original,
        )
    assert isinstance(tuned, unreal.Material), tuned_path
    unreal.MaterialEditingLibrary.delete_all_material_expressions(tuned)
    tuned.set_editor_property("used_with_nanite", True)
    tuned.set_editor_property("two_sided", True)
    maps = _texture_map(original)
    lib = unreal.MaterialEditingLibrary
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
    # Multiply uses its default output pin (no named output like TextureSample's RGB).
    assert lib.connect_material_property(base_tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal = _sample(tuned, maps["N"], -760, 380, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    assert lib.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    for channel, prop, y in (("R", unreal.MaterialProperty.MP_ROUGHNESS, 570),
                             ("M", unreal.MaterialProperty.MP_METALLIC, 760),
                             ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 950)):
        if channel not in maps:
            continue
        sample = _sample(tuned, maps[channel], -760, y, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if channel == "R":
            factor = _scalar(tuned, ROUGHNESS_SCALE[name], -760, y + 100)
            sample = _multiply(tuned, sample, factor, -420, y, "R")
        assert lib.connect_material_property(sample, "" if channel == "R" else "R", prop)
    if name == "Water":
        tuned.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        opacity = _scalar(tuned, 0.48, -420, 1120)
        assert lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    lib.layout_material_expressions(tuned)
    lib.recompile_material(tuned)
    assert unreal.EditorAssetLibrary.save_loaded_asset(tuned)
    return original, tuned


def _rebind_mesh(materials, original_paths):
    mesh_path = DEST + "/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0"
    mesh = _asset(mesh_path)
    original_slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name)
        suffix = slot_name.replace("M_Dadongmen_", "")
        original_slots.append({"index": index, "slot": slot_name,
                               "material": original_paths.get(suffix, slot.material_interface.get_path_name() if slot.material_interface else "")})
        if suffix in materials:
            mesh.set_material(index, materials[suffix])
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    )
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh, original_slots


def _rebind_blueprint(mesh):
    bp_path = DEST + "/BP_Dadongmen_V4"
    bp = _asset(bp_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    changed = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if isinstance(component, unreal.StaticMeshComponent) and component.static_mesh:
            component.set_static_mesh(mesh)
            for index, material in enumerate(mesh.get_editor_property("static_materials")):
                component.set_material(index, material.material_interface)
            changed += 1
    assert changed == 2, changed
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    return bp_path, changed


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before Dadongmen tuning: " + str(dirty)
    originals, tuned = {}, {}
    for name in MATERIAL_NAMES:
        original, material = _build_tuned_material(name)
        originals[name] = original.get_path_name()
        tuned[name] = material.get_path_name()
    mesh, slots = _rebind_mesh({name: _asset(path) for name, path in tuned.items()}, originals)
    bp_path, component_count = _rebind_blueprint(mesh)
    report = {
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-5ea059eb-7975-4c7f-9156-c4dddb1d47d8.png",
        "blueprint": bp_path,
        "mesh": mesh.get_path_name(),
        "tuned_materials": tuned,
        "original_materials": originals,
        "original_mesh_slots": slots,
        "tints": TINTS,
        "roughness_scale": ROUGHNESS_SCALE,
        "water_opacity": 0.48,
        "blueprint_components_rebound": component_count,
        "geometry_changed": False,
        "intent": "Warm weathered masonry, deeper timber, cool clay tile, grounded earth and water tones from the reference sheet.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("DADONGMEN_REFERENCE_TUNED", json.dumps({"blueprint": bp_path, "mesh": mesh.get_path_name(), "materials": tuned}))


if __name__ == "__main__":
    main()
