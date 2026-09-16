"""Tune the imported Zhengximen (Great West Gate) toward its reference sheet.

The imported building remains one independent Blueprint and one authored mesh
assembly. This post-import pass creates rollback-safe sibling materials,
rebinds the primary mesh and Blueprint templates, and leaves geometry, UVs and
collision unchanged.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen"
MATERIAL_NAMES = (
    "AgedWood",
    "BlackIron",
    "ClayRoofTile",
    "DarkTimber",
    "GatePlaque",
    "GrayBrick",
    "LimePlaster",
    "StoneFoundation",
)
TINTS = {
    "AgedWood": (0.38, 0.22, 0.12, 1.0),
    "BlackIron": (0.08, 0.09, 0.08, 1.0),
    "ClayRoofTile": (0.34, 0.37, 0.41, 1.0),
    "DarkTimber": (0.25, 0.13, 0.075, 1.0),
    "GatePlaque": (0.78, 0.71, 0.57, 1.0),
    "GrayBrick": (0.54, 0.51, 0.44, 1.0),
    "LimePlaster": (0.70, 0.64, 0.53, 1.0),
    "StoneFoundation": (0.56, 0.49, 0.39, 1.0),
}
ROUGHNESS_SCALE = {
    "AgedWood": 0.96,
    "BlackIron": 0.82,
    "ClayRoofTile": 1.05,
    "DarkTimber": 0.92,
    "GatePlaque": 0.88,
    "GrayBrick": 1.12,
    "LimePlaster": 1.10,
    "StoneFoundation": 1.10,
}
ROOT = PROJECT / "Saved/RawModelImport/V4"
REPORT = ROOT / "Zhengximen-reference-tuning.json"


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _texture_map(original):
    """Map the conventional V4 texture suffixes to their texture assets."""
    result = {}
    for texture in unreal.MaterialEditingLibrary.get_used_textures(original):
        name = texture.get_name().lower()
        if name.endswith("_basecolor") or name.endswith("_base_color") or name.endswith("_bc"):
            result["BC"] = texture
        elif name.endswith("_normal") or name.endswith("_normalgl") or name.endswith("_n"):
            result["N"] = texture
        elif name.endswith("_roughness") or name.endswith("_r"):
            result["R"] = texture
        elif name.endswith("_metallic") or name.endswith("_m"):
            result["M"] = texture
        elif name.endswith("_ao"):
            result["AO"] = texture
        elif name.endswith("_height") or name.endswith("_h"):
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
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        sample = _sample(tuned, maps[channel], -760, y, sampler)
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


def _rebind_mesh(materials, original_paths):
    mesh_path = DEST + "/Meshes/SM_Zhengximen_LOD0/SM_Zhengximen_LOD0"
    mesh = _asset(mesh_path)
    original_slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name)
        suffix = slot_name.replace("M_", "")
        original_slots.append(
            {
                "index": index,
                "slot": slot_name,
                "material": original_paths.get(
                    suffix,
                    slot.material_interface.get_path_name() if slot.material_interface else "",
                ),
            }
        )
        if suffix in materials:
            mesh.set_material(index, materials[suffix])
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    )
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh, original_slots


def _rebind_blueprint(mesh):
    bp_path = DEST + "/BP_Zhengximen_V4"
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
    assert not dirty, "Save the current map before Zhengximen tuning: " + str(dirty)
    originals, tuned = {}, {}
    for name in MATERIAL_NAMES:
        original, material = _build_tuned_material(name)
        originals[name] = original.get_path_name()
        tuned[name] = material.get_path_name()
    mesh, slots = _rebind_mesh({name: _asset(path) for name, path in tuned.items()}, originals)
    bp_path, component_count = _rebind_blueprint(mesh)
    report = {
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-f1f30b9a-1142-4263-9093-1227ff7a4d73.png",
        "blueprint": bp_path,
        "mesh": mesh.get_path_name(),
        "tuned_materials": tuned,
        "original_materials": originals,
        "original_mesh_slots": slots,
        "tints": TINTS,
        "roughness_scale": ROUGHNESS_SCALE,
        "blueprint_components_rebound": component_count,
        "geometry_changed": False,
        "intent": "Weathered gray brick, aged warm timber, cool clay tiles and restrained plaster/iron/plaque tones from the reference sheet.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("ZHENGXIMEN_REFERENCE_TUNED", json.dumps({"blueprint": bp_path, "mesh": mesh.get_path_name(), "materials": tuned}))


if __name__ == "__main__":
    main()
