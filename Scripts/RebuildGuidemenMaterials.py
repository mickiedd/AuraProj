"""Rebuild every Guidemen material with correct channel wiring.

The capture showed the stone wall rendering correctly as light grey textured masonry while
the timber, doors, plaster and metal render as saturated blue with visible relief — the
signature of a tangent-space normal map being read as base colour. The source maps are fine:
`M_AgedTimber_BaseColor.jpg` is brown (0.397, 0.249, 0.157), the tiles are grey
(0.236, 0.243, 0.232), and every `_Normal.jpg` is a proper (0.498, 0.498, 0.99) normal map.
The fault is in the older `_ReferenceTuned` material graphs.

Only the two materials rebuilt in the earlier pass — the stone and the tile — render
correctly, which is the tell. This rebuilds all of them with the wiring the source's packing
requires:

    BaseColor  -> BaseColor          (sRGB)
    Normal     -> Normal             (non-sRGB)
    MetallicRoughness.G -> Roughness, .B -> Metallic
    AO.R       -> Ambient Occlusion

Flat-colour materials (dark interior, faded red wood) get their tint with no sampler.
Stone and tile keep the per-instance weathering custom data already assigned; every material
gets a `_Rebuild20260918` sibling and the old graph is left in place for rollback.

Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/material-rebuild-20260918.json"
PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K"
BLUEPRINT = PACKAGE + "/BP_Guidemen_V5_4K"
MATERIALS = PACKAGE + "/Materials"
TEXTURES = PACKAGE + "/Textures"

# surface -> (base colour map, normal map, metallic-roughness map, AO map, tint, roughness scale)
SURFACES = {
    "WeatheredStone": ("M_WeatheredStone_BaseColor", "M_WeatheredStone_Normal",
                       "M_WeatheredStone_MetallicRoughness", "M_WeatheredStone_AO",
                       (0.53, 0.50, 0.44, 1.0), 1.10),
    "AgedTimber": ("M_AgedTimber_BaseColor", "M_AgedTimber_Normal",
                   "M_AgedTimber_MetallicRoughness", "M_AgedTimber_AO",
                   (0.80, 0.58, 0.42, 1.0), 0.94),
    "GateDoor": ("M_GateDoor_BaseColor", "M_GateDoor_Normal",
                 "M_GateDoor_MetallicRoughness", "M_GateDoor_AO",
                 (0.78, 0.56, 0.40, 1.0), 0.96),
    "GrayClayTile": ("M_GrayClayTile_BaseColor", "M_GrayClayTile_Normal",
                     "M_GrayClayTile_MetallicRoughness", "M_GrayClayTile_AO",
                     (0.62, 0.65, 0.68, 1.0), 1.06),
    "AgedPlaster": ("M_AgedPlaster_BaseColor", "M_AgedPlaster_Normal",
                    "M_AgedPlaster_MetallicRoughness", "M_AgedPlaster_AO",
                    (0.74, 0.70, 0.62, 1.0), 1.04),
    "AgedMetal": ("M_AgedMetal_BaseColor", "M_AgedMetal_Normal",
                  "M_AgedMetal_MetallicRoughness", "M_AgedMetal_AO",
                  (0.60, 0.58, 0.56, 1.0), 0.98),
}
FLAT = {
    "DarkInterior": (0.16, 0.15, 0.14, 1.0),
    "FadedRedWood": (0.42, 0.20, 0.15, 1.0),
}
SINGLE = {
    "Plaque_PorteDeGuide": ("M_Plaque_PorteDeGuide", (1.00, 1.00, 1.00, 1.0)),
    "Sign_Guidemen": ("M_Sign_Guidemen", (1.00, 1.00, 1.00, 1.0)),
}

# old material on the Blueprint -> the new material to bind
BINDINGS = {
    "M_WeatheredStone_4K_ReferenceTuned_RefTune4": "M_WeatheredStone_4K_Rebuild20260918",
    "M_GrayClayTile_2K_ReferenceTuned_RefTune4": "M_GrayClayTile_2K_Rebuild20260918",
    "M_AgedTimber_4K_ReferenceTuned": "M_AgedTimber_4K_Rebuild20260918",
    "M_GateDoor_4K_ReferenceTuned": "M_GateDoor_4K_Rebuild20260918",
    "M_AgedPlaster_2K_ReferenceTuned": "M_AgedPlaster_2K_Rebuild20260918",
    "M_AgedMetal_Solid_ReferenceTuned": "M_AgedMetal_Solid_Rebuild20260918",
    "M_DarkInterior_ReferenceTuned": "M_DarkInterior_Rebuild20260918",
    "M_FadedRedWood_ReferenceTuned": "M_FadedRedWood_Rebuild20260918",
    "M_Plaque_PorteDeGuide_2K_ReferenceTuned": "M_Plaque_PorteDeGuide_2K_Rebuild20260918",
    "M_Sign_Guidemen_2K_ReferenceTuned": "M_Sign_Guidemen_2K_Rebuild20260918",
}

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _node(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def _sample(material, name, x, y, sampler):
    texture = EAL.load_asset(TEXTURES + "/" + name)
    assert texture, TEXTURES + "/" + name
    node = _node(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    return node


def _scalar(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def _vec3(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _mul(material, a, b, x, y, a_out="", b_out=""):
    node = _node(material, unreal.MaterialExpressionMultiply, x, y)
    assert MEL.connect_material_expressions(a, a_out, node, "A")
    assert MEL.connect_material_expressions(b, b_out, node, "B")
    return node


def _fresh(name):
    target = MATERIALS + "/" + name
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    assert material, target
    return material


def _finish(material):
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    assert EAL.save_loaded_asset(material)


def build_pbr(name, surface):
    base_name, normal_name, mr_name, ao_name, tint, roughness = SURFACES[surface]
    material = _fresh(name)
    base = _sample(material, base_name, -900, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, normal_name, -900, 200, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mr = _sample(material, mr_name, -900, 600, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    ao = _sample(material, ao_name, -900, 1000, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    coloured = _mul(material, base, _vec3(material, tint, -1100, -400), -600, -200)
    rough = _mul(material, mr, _scalar(material, roughness, -1100, 600), -600, 600, "G")
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    assert MEL.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)
    assert MEL.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _finish(material)
    return material, {"surface": surface, "base": base_name, "normal": normal_name,
                      "metallic_roughness": mr_name, "ao": ao_name, "tint": list(tint),
                      "roughness_scale": roughness}


def build_flat(name, tint):
    material = _fresh(name)
    assert MEL.connect_material_property(_vec3(material, tint, -540, 60), "",
                                         unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(_scalar(material, 0.75, -540, 660), "",
                                         unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {"flat_tint": list(tint)}


def build_single(name, texture_name, tint):
    material = _fresh(name)
    base = _sample(material, texture_name, -900, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    coloured = _mul(material, base, _vec3(material, tint, -900, 220), -600, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(_scalar(material, 0.80, -600, 660), "",
                                         unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {"single_texture": texture_name, "tint": list(tint)}


def main():
    report = {"created": "2026-09-18", "materials": [], "bindings": [], "map_saved": False}

    built = {}
    for surface in SURFACES:
        name = f"M_{surface}_4K_Rebuild20260918" if surface not in ("AgedMetal",) else "M_AgedMetal_Solid_Rebuild20260918"
        if surface == "GrayClayTile":
            name = "M_GrayClayTile_2K_Rebuild20260918"
        elif surface == "AgedPlaster":
            name = "M_AgedPlaster_2K_Rebuild20260918"
        material, info = build_pbr(name, surface)
        built[name] = material
        report["materials"].append({"name": name, "path": _path(material), **info})
        print("MAT_REBUILT", name)
    for surface, tint in FLAT.items():
        name = f"M_{surface}_Rebuild20260918"
        material, info = build_flat(name, tint)
        built[name] = material
        report["materials"].append({"name": name, "path": _path(material), **info})
        print("MAT_REBUILT", name)
    for surface, (texture_name, tint) in SINGLE.items():
        name = f"M_{surface}_2K_Rebuild20260918"
        material, info = build_single(name, texture_name, tint)
        built[name] = material
        report["materials"].append({"name": name, "path": _path(material), **info})
        print("MAT_REBUILT", name)

    blueprint = EAL.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, components = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if _path(component) in seen:
            continue
        seen.add(_path(component))
        components.append(component)

    rebound = 0
    for component in components:
        for index in range(component.get_num_materials()):
            current = component.get_material(index)
            if not current:
                continue
            key = current.get_name()
            replacement_name = BINDINGS.get(key)
            if not replacement_name:
                continue
            replacement = built.get(replacement_name)
            assert replacement, (key, replacement_name)
            component.set_material(index, replacement)
            rebound += 1
            report["bindings"].append({"component": component.get_name(), "slot": index,
                                       "from": key, "to": replacement_name})
    print("MAT_REBOUND slots", rebound)
    report["rebound_slots"] = rebound
    report["component_count"] = len(components)
    report["instance_total"] = sum(c.get_instance_count() for c in components)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("MAT_REBUILD_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
