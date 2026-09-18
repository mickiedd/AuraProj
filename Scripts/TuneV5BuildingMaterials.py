"""Create rollback-safe, reference-tuned materials for all three V5 buildings."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

TINTS = {
    "M_WeatheredStone_4K": (0.62, 0.59, 0.52, 1.0),
    "M_AgedTimber_4K": (0.43, 0.27, 0.17, 1.0),
    "M_GateDoor_4K": (0.38, 0.20, 0.11, 1.0),
    "M_GrayClayTile_2K": (0.43, 0.46, 0.50, 1.0),
    "M_AgedPlaster_2K": (0.72, 0.66, 0.56, 1.0),
    "M_AgedMetal_Solid": (0.25, 0.17, 0.10, 1.0),
    "M_Sign_Guidemen_2K": (0.76, 0.62, 0.40, 1.0),
    "M_Plaque_PorteDeGuide_2K": (0.76, 0.68, 0.52, 1.0),
    "M_DarkInterior": (0.018, 0.014, 0.011, 1.0),
    "M_FadedRedWood": (0.31, 0.075, 0.035, 1.0),
    "M_DefaultStone": (0.46, 0.43, 0.37, 1.0),
    "M_Stone": (0.61, 0.58, 0.51, 1.0),
    "M_Wood": (0.42, 0.25, 0.14, 1.0),
    "M_Plaster": (0.72, 0.66, 0.56, 1.0),
    "M_Iron": (0.22, 0.14, 0.08, 1.0),
    "M_RoofTile": (0.40, 0.43, 0.47, 1.0),
    "M_Plaque_Wuxianmen": (0.75, 0.59, 0.34, 1.0),
}

ROUGHNESS = {
    "M_WeatheredStone_4K": 1.10,
    "M_AgedTimber_4K": 0.96,
    "M_GateDoor_4K": 0.92,
    "M_GrayClayTile_2K": 1.06,
    "M_AgedPlaster_2K": 1.10,
    "M_AgedMetal_Solid": 0.88,
    "M_Sign_Guidemen_2K": 0.78,
    "M_Plaque_PorteDeGuide_2K": 0.82,
    "M_DarkInterior": 0.92,
    "M_FadedRedWood": 0.90,
    "M_DefaultStone": 0.88,
    "M_Stone": 1.10,
    "M_Wood": 0.94,
    "M_Plaster": 1.10,
    "M_Iron": 0.72,
    "M_RoofTile": 1.04,
    "M_Plaque_Wuxianmen": 0.80,
}


def _sample(material, texture, x, y, sampler):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    return node


def _constant(material, value, x, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _scalar(material, value, x, y):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def _multiply(material, left, right, x, y, left_output="RGB"):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, x, y)
    assert MEL.connect_material_expressions(left, left_output, node, "A")
    assert MEL.connect_material_expressions(right, "", node, "B")
    return node


def _build(cfg, material_name, texture_paths):
    original_path = cfg["destination"] + "/Materials/" + material_name
    tuned_name = material_name + "_ReferenceTuned"
    tuned_path = cfg["destination"] + "/Materials/" + tuned_name
    original = EAL.load_asset(original_path)
    assert isinstance(original, unreal.Material), original_path
    tuned = EAL.load_asset(tuned_path) if EAL.does_asset_exist(tuned_path) else None
    if tuned is None:
        tuned = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            tuned_name, cfg["destination"] + "/Materials", original
        )
    assert isinstance(tuned, unreal.Material), tuned_path
    MEL.delete_all_material_expressions(tuned)
    tuned.set_editor_property("two_sided", True)
    tuned.set_editor_property("used_with_nanite", True)
    maps = cfg["material_maps"].get(material_name, {})
    tint = TINTS[material_name]
    if "BaseColor" in maps:
        base = _sample(tuned, EAL.load_asset(texture_paths[maps["BaseColor"]]), -900, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tint_node = _constant(tuned, tint, -900, 180)
        result = _multiply(tuned, base, tint_node, -540, 60)
        assert MEL.connect_material_property(result, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        result = _constant(tuned, tint, -540, 60)
        assert MEL.connect_material_property(result, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if "Normal" in maps:
        normal = _sample(tuned, EAL.load_asset(texture_paths[maps["Normal"]]), -900, 380, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness_map = maps.get("Roughness") or maps.get("MetallicRoughness")
    if roughness_map:
        roughness_sample = _sample(tuned, EAL.load_asset(texture_paths[roughness_map]), -900, 620, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        factor = _scalar(tuned, ROUGHNESS[material_name], -900, 760)
        roughness = _multiply(tuned, roughness_sample, factor, -540, 660, "R" if "Roughness" in maps else "G")
        assert MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        if "MetallicRoughness" in maps:
            assert MEL.connect_material_property(roughness_sample, "B", unreal.MaterialProperty.MP_METALLIC)
    else:
        roughness = _scalar(tuned, ROUGHNESS[material_name], -540, 660)
        assert MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        if material_name == "M_Iron":
            metallic = _scalar(tuned, 0.88, -540, 840)
            assert MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    if "AO" in maps:
        ao = _sample(tuned, EAL.load_asset(texture_paths[maps["AO"]]), -900, 980, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        assert MEL.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.layout_material_expressions(tuned)
    MEL.recompile_material(tuned)
    assert EAL.save_loaded_asset(tuned)
    return original, tuned


def _blueprint_components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set()
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if component.get_path_name() in seen:
            continue
        seen.add(component.get_path_name())
        result.append(component)
    return result


def tune_package(cfg):
    import_path = ROOT / (cfg["name"] + "-import.json")
    imported = json.loads(import_path.read_text(encoding="utf-8"))
    assert imported.get("complete"), cfg["name"]
    originals, tuned = {}, {}
    for material_name in cfg["glb_material_names"]:
        original, material = _build(cfg, material_name, imported["textures"])
        originals[material_name] = original.get_path_name()
        tuned[material_name] = material.get_path_name()
    changed_meshes = []
    for group in imported["scene"]["groups"]:
        mesh = EAL.load_asset(group["mesh"])
        assert isinstance(mesh, unreal.StaticMesh), group["mesh"]
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            name = slot.material_interface.get_name().removesuffix("_ReferenceTuned") if slot.material_interface else ""
            assert name in tuned, (group["mesh"], name)
            mesh.set_material(index, EAL.load_asset(tuned[name]))
        assert EAL.save_loaded_asset(mesh)
        changed_meshes.append(group["mesh"])
    blueprint = EAL.load_asset(imported["blueprint"])
    assert isinstance(blueprint, unreal.Blueprint)
    components = _blueprint_components(blueprint)
    assert len(components) == imported["hism_component_count"], (len(components), imported["hism_component_count"])
    for component in components:
        mesh = component.static_mesh
        assert mesh
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(index, slot.material_interface)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    result = {
        "passed": True,
        "name": cfg["name"],
        "reference": cfg["reference"],
        "blueprint": imported["blueprint"],
        "tuned_materials": tuned,
        "rollback_materials": originals,
        "tints": {name: TINTS[name] for name in cfg["glb_material_names"]},
        "roughness": {name: ROUGHNESS[name] for name in cfg["glb_material_names"]},
        "changed_meshes": changed_meshes,
        "blueprint_components_rebound": len(components),
        "geometry_changed": False,
        "intent": "Reference-matched weathered gray stone, warm aged timber, cool charcoal clay roofs, restrained plaster, dark iron and readable historic plaques.",
    }
    (ROOT / (cfg["name"] + "-tuning.json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("V5_REFERENCE_TUNED", json.dumps({"name": cfg["name"], "materials": len(tuned), "meshes": len(changed_meshes)}))


def main():
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    for cfg in json.loads((ROOT / "packages.json").read_text(encoding="utf-8")):
        tune_package(cfg)


if __name__ == "__main__":
    main()
