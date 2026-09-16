"""Apply Dadongmen-only Deep AAA materials and additive hero geometry.

This script is intentionally independent of the parallel gate pass.  It
creates new ``_DeepAAA`` materials from Dadongmen's authored 4K channels,
imports the local Dadongmen hero FBX, rebinds only Dadongmen source-facing
mesh/Blueprint components, and preserves all prior assets for rollback.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
DEEP_ROOT = ROOT / "DeepAAA"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
BP_PATH = DEST + "/BP_Dadongmen_V4"
PREVIEW_PATH = DEST + "/L_Dadongmen_V4_Preview"
SUFFIX = "_DeepAAA"
MAPS = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")
DETAIL_SOURCE = DEEP_ROOT / "Dadongmen_V4_DeepAAA.fbx"
DETAIL_MANIFEST = DEEP_ROOT / "Dadongmen_V4_DeepAAA.json"
DETAIL_FOLDER = DEST + "/Meshes/DeepAAA/Dadongmen_V4_DeepAAA"
DETAIL_NAME = "Dadongmen_V4_DeepAAA"
FORCE_DETAIL_REIMPORT = True


MATERIALS = {
    # These are luminance-preserving multipliers, not absolute albedo tints;
    # the authored BC texture remains the brightness/contrast authority.
    "Stone": {"source": "M_Dadongmen_Stone_AAA", "folder": "Stone", "prefix": "T_Stone", "tint": (1.08, 0.98, 0.82), "dirt": (0.19, 0.14, 0.085), "edge": (0.96, 0.89, 0.73), "variation": 0.22, "noise": 2.25, "macro": (0.62, 0.62), "micro": (4.2, 4.0), "streak": (0.62, 2.6), "grime": 0.34, "streak_strength": 0.16, "edge_strength": 0.18, "roughness": 1.07, "metallic": 0.0, "ao": 0.88, "height": 0.058, "specular": 0.34, "clearcoat": 0.0},
    "Plaster": {"source": "M_Dadongmen_Plaster_AAA", "folder": "Plaster", "prefix": "T_Plaster", "tint": (1.04, 0.98, 0.84), "dirt": (0.24, 0.18, 0.105), "edge": (1.06, 0.98, 0.79), "variation": 0.19, "noise": 1.95, "macro": (0.70, 0.70), "micro": (4.6, 4.3), "streak": (0.60, 3.0), "grime": 0.29, "streak_strength": 0.18, "edge_strength": 0.14, "roughness": 1.10, "metallic": 0.0, "ao": 0.90, "height": 0.044, "specular": 0.31, "clearcoat": 0.0},
    "Wood": {"source": "M_Dadongmen_Wood_AAA", "folder": "Wood", "prefix": "T_Wood", "tint": (1.12, 0.82, 0.58), "dirt": (0.095, 0.028, 0.010), "edge": (1.17, 0.62, 0.22), "variation": 0.20, "noise": 2.10, "macro": (0.72, 2.4), "micro": (5.0, 2.0), "streak": (0.72, 2.2), "grime": 0.24, "streak_strength": 0.15, "edge_strength": 0.16, "roughness": 0.92, "metallic": 0.0, "ao": 0.90, "height": 0.034, "specular": 0.30, "clearcoat": 0.0},
    "RoofClay": {"source": "M_Dadongmen_RoofClay_AAA", "folder": "RoofClay", "prefix": "T_RoofClay", "tint": (1.08, 1.13, 1.18), "dirt": (0.052, 0.045, 0.038), "edge": (0.77, 0.83, 0.86), "variation": 0.22, "noise": 2.35, "macro": (0.56, 0.56), "micro": (4.1, 4.1), "streak": (0.56, 3.2), "grime": 0.28, "streak_strength": 0.19, "edge_strength": 0.13, "roughness": 0.97, "metallic": 0.0, "ao": 0.87, "height": 0.054, "specular": 0.36, "clearcoat": 0.08},
    "Dirt": {"source": "M_Dadongmen_Dirt_AAA", "folder": "Dirt", "prefix": "T_Dirt", "tint": (1.10, 0.88, 0.68), "dirt": (0.15, 0.075, 0.025), "edge": (0.72, 0.47, 0.22), "variation": 0.25, "noise": 2.00, "macro": (0.48, 0.48), "micro": (3.5, 3.5), "streak": (0.50, 2.7), "grime": 0.25, "streak_strength": 0.12, "edge_strength": 0.10, "roughness": 1.15, "metallic": 0.0, "ao": 0.92, "height": 0.062, "specular": 0.24, "clearcoat": 0.0},
    "Water": {"source": "M_Dadongmen_Water_AAA", "folder": "Water", "prefix": "T_Water", "tint": (0.70, 1.05, 1.08), "dirt": (0.018, 0.085, 0.080), "edge": (0.22, 0.66, 0.64), "variation": 0.18, "noise": 1.75, "macro": (0.42, 0.42), "micro": (6.0, 5.0), "streak": (0.42, 2.0), "grime": 0.10, "streak_strength": 0.08, "edge_strength": 0.13, "roughness": 0.29, "metallic": 0.0, "ao": 0.96, "height": 0.026, "specular": 0.48, "clearcoat": 0.0, "opacity": 0.60},
    # The source slot is a constant-green shard material.  It is deliberately
    # masked out on the combined source mesh; attached replacement leaves use
    # VegetationLeaf below.
    "Vegetation": {"source": "M_Dadongmen_Vegetation_AAA", "folder": None, "prefix": None, "tint": (0.035, 0.14, 0.018), "dirt": (0.012, 0.032, 0.006), "edge": (0.10, 0.30, 0.035), "variation": 0.28, "noise": 2.6, "macro": (0.85, 0.85), "micro": (5.0, 5.0), "streak": (0.80, 2.0), "grime": 0.16, "streak_strength": 0.08, "edge_strength": 0.10, "roughness": 0.94, "metallic": 0.0, "ao": 0.93, "height": 0.032, "specular": 0.05, "clearcoat": 0.0, "masked_cull": True},
    "VegetationLeaf": {"source": "M_Dadongmen_Vegetation_AAA", "folder": "Dirt", "prefix": "T_Dirt", "tint": (0.50, 1.10, 0.34), "dirt": (0.035, 0.12, 0.012), "edge": (0.12, 0.38, 0.05), "variation": 0.25, "noise": 2.6, "macro": (0.85, 0.85), "micro": (5.0, 5.0), "streak": (0.80, 2.0), "grime": 0.16, "streak_strength": 0.08, "edge_strength": 0.10, "roughness": 0.94, "metallic": 0.0, "ao": 0.93, "height": 0.032, "specular": 0.05, "clearcoat": 0.0},
    "DoorWood": {"source": "M_Dadongmen_DoorWood_AAA", "folder": "Wood", "prefix": "T_Wood", "tint": (1.10, 0.72, 0.46), "dirt": (0.045, 0.012, 0.004), "edge": (0.70, 0.27, 0.055), "variation": 0.22, "noise": 2.5, "macro": (0.78, 2.3), "micro": (5.2, 2.1), "streak": (0.75, 2.4), "grime": 0.26, "streak_strength": 0.15, "edge_strength": 0.18, "roughness": 0.88, "metallic": 0.0, "ao": 0.91, "height": 0.040, "specular": 0.29, "clearcoat": 0.0},
    "Iron": {"source": "M_Dadongmen_Iron_AAA", "folder": None, "prefix": None, "tint": (0.82, 0.90, 0.96), "dirt": (0.026, 0.024, 0.021), "edge": (0.55, 0.58, 0.54), "variation": 0.16, "noise": 3.0, "macro": (1.2, 1.2), "micro": (6.0, 6.0), "streak": (1.2, 2.8), "grime": 0.15, "streak_strength": 0.08, "edge_strength": 0.23, "roughness": 0.78, "metallic": 0.86, "ao": 0.93, "height": 0.022, "specular": 0.50, "clearcoat": 0.0},
}


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(source, source_output, target, target_input), (source, source_output, target, target_input)


def _connect_prop(source, source_output, material, prop):
    assert unreal.MaterialEditingLibrary.connect_material_property(source, source_output, prop), (source, source_output, prop)


def _scalar(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", float(value))
    return node


def _color(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(float(value[0]), float(value[1]), float(value[2]), 1.0))
    return node


def _coord(material, tiling, x, y):
    node = _expr(material, unreal.MaterialExpressionTextureCoordinate, x, y)
    node.set_editor_property("coordinate_index", 0)
    node.set_editor_property("u_tiling", float(tiling[0]))
    node.set_editor_property("v_tiling", float(tiling[1]))
    return node


def _sample(material, texture, uv, sampler, x, y):
    node = _expr(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    if uv is not None:
        _connect(uv, "", node, "UVs")
    return node


def _binary(material, cls, left, right, x, y, left_output="", right_output=""):
    node = _expr(material, cls, x, y)
    _connect(left, left_output, node, "A")
    _connect(right, right_output, node, "B")
    return node


def _multiply(material, left, right, x, y, left_output="", right_output=""):
    return _binary(material, unreal.MaterialExpressionMultiply, left, right, x, y, left_output, right_output)


def _add(material, left, right, x, y, left_output="", right_output=""):
    return _binary(material, unreal.MaterialExpressionAdd, left, right, x, y, left_output, right_output)


def _subtract(material, left, right, x, y, left_output="", right_output=""):
    return _binary(material, unreal.MaterialExpressionSubtract, left, right, x, y, left_output, right_output)


def _lerp(material, left, right, alpha, x, y, left_output="", right_output="", alpha_output=""):
    node = _expr(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    _connect(left, left_output, node, "A")
    _connect(right, right_output, node, "B")
    _connect(alpha, alpha_output, node, "Alpha")
    return node


def _noise(material, scale, x, y):
    node = _expr(material, unreal.MaterialExpressionNoise, x, y)
    for prop, value in (("scale", scale), ("quality", 2), ("levels", 5), ("output_min", 0.0), ("output_max", 1.0)):
        try:
            node.set_editor_property(prop, value)
        except Exception:
            pass
    return node


def _one_minus(material, source, x, y, source_output=""):
    return _subtract(material, _scalar(material, 1.0, x - 150, y - 70), source, x, y, "", source_output)


def _fresnel(material, x, y):
    node = _expr(material, unreal.MaterialExpressionFresnel, x, y)
    for prop, value in (("exponent", 4.2), ("base_reflect_fraction", 0.04)):
        try:
            node.set_editor_property(prop, value)
        except Exception:
            pass
    return node


def _map_asset(key, channel):
    profile = MATERIALS[key]
    if not profile["folder"]:
        return None
    path = "%s/Textures/%s/%s_%s_4K.%s_%s_4K" % (DEST, profile["folder"], profile["prefix"], channel, profile["prefix"], channel)
    return _asset(path)


def _build_material(key):
    profile = MATERIALS[key]
    source_path = DEST + "/Materials/" + profile["source"]
    source = _asset(source_path)
    target_name = "M_Dadongmen_" + key + SUFFIX
    target_path = DEST + "/Materials/" + target_name
    target = _asset(target_path) if unreal.EditorAssetLibrary.does_asset_exist(target_path) else unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(target_name, DEST + "/Materials", source)
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(target)
    target.set_editor_property("used_with_nanite", True)
    target.set_editor_property("two_sided", True)
    if profile.get("masked_cull"):
        target.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    else:
        target.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if key == "Water" else unreal.BlendMode.BLEND_OPAQUE)

    macro_uv = _coord(target, profile["macro"], -1700, -40)
    micro_uv = _coord(target, profile["micro"], -1700, 690)
    streak_uv = _coord(target, profile["streak"], -1700, 1400)
    height = _map_asset(key, "H")
    ao = _map_asset(key, "AO")
    base = _map_asset(key, "BC")
    normal = _map_asset(key, "N")
    roughness = _map_asset(key, "R")
    metallic = _map_asset(key, "M")

    bump = _expr(target, unreal.MaterialExpressionBumpOffset, -1450, 220)
    _connect(macro_uv, "", bump, "Coordinate")
    if height:
        height_sample = _sample(target, height, macro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1650, 240)
        _connect(height_sample, "", bump, "Height")
    else:
        _connect(_noise(target, profile["noise"], -1650, 240), "", bump, "Height")
    try:
        bump.set_editor_property("height_ratio", float(profile["height"]))
        bump.set_editor_property("reference_plane", 0.5)
    except Exception:
        pass

    if base:
        base_sample = _sample(target, base, bump, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1480, -300)
        base_tinted = _multiply(target, base_sample, _color(target, profile["tint"], -1460, -100), -1200, -300, "RGB")
        macro_noise = _noise(target, profile["noise"], -1440, 1640)
        macro_range = _lerp(target, _scalar(target, 1.0 - profile["variation"], -1200, 1610), _scalar(target, 1.0 + profile["variation"], -1200, 1720), macro_noise, -980, 1640)
        macro_color = _multiply(target, base_tinted, macro_range, -760, -300)
        streak_sample = _sample(target, base, streak_uv, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1460, 1390)
        streak_tint = _multiply(target, streak_sample, _color(target, profile["dirt"], -1190, 1460), -930, 1390, "RGB")
        streak_mask = _multiply(target, _noise(target, profile["noise"] * 0.72, -1220, 1900), _scalar(target, profile["streak_strength"], -980, 1880), -700, 1390)
        streak_layer = _lerp(target, macro_color, streak_tint, streak_mask, -460, -300)
    else:
        macro_noise = _noise(target, profile["noise"], -1440, 1640)
        macro_color = _lerp(target, _color(target, profile["tint"], -1460, -120), _color(target, profile["edge"], -1180, -120), macro_noise, -760, -300)
        streak_layer = macro_color

    if ao:
        ao_sample = _sample(target, ao, bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 1050)
        cavity = _one_minus(target, ao_sample, -1200, 1050)
    else:
        ao_sample = _scalar(target, 0.92, -1450, 1050)
        cavity = _one_minus(target, ao_sample, -1200, 1050)
    grime_noise = _noise(target, profile["noise"] * 0.58, -1200, 2110)
    grime_mask = _multiply(target, grime_noise, _add(target, _scalar(target, 0.18, -980, 1160), _multiply(target, cavity, _scalar(target, 1.08, -980, 1270), -760, 1160), -500, 1160), -260, 1160)
    grime_layer = _lerp(target, streak_layer, _color(target, profile["dirt"], -480, 90), _multiply(target, grime_mask, _scalar(target, profile["grime"], -250, 1270), -20, 1160), 210, -300)
    edge_mask = _fresnel(target, -200, 2210)
    final_base = _lerp(target, grime_layer, _color(target, profile["edge"], 0, 1830), _multiply(target, edge_mask, _scalar(target, profile["edge_strength"], 0, 2280), 250, 1830), 520, -300)
    _connect_prop(final_base, "", target, unreal.MaterialProperty.MP_BASE_COLOR)

    if normal:
        macro_normal = _sample(target, normal, bump, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1450, 500)
        micro_normal = _sample(target, normal, micro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1450, 760)
        normal_output = macro_normal
        normal_layers = 1
        blend_class = getattr(unreal, "MaterialExpressionBlendAngleCorrectedNormals", None)
        if blend_class is not None:
            try:
                blend = _expr(target, blend_class, -900, 580)
                _connect(macro_normal, "RGB", blend, "BaseNormal")
                _connect(micro_normal, "RGB", blend, "AdditionalNormal")
                normal_output = blend
                normal_layers = 2
            except Exception as exc:
                unreal.log_warning("Dadongmen Deep AAA angle-normal fallback for %s: %s" % (key, exc))
        if normal_layers == 1:
            normal_output = _add(target, macro_normal, _multiply(target, micro_normal, _scalar(target, 0.58, -1080, 760), -900, 760, "RGB"), -700, 580, "RGB")
            normal_layers = 2
        _connect_prop(normal_output, "RGB" if normal_output is macro_normal else "", target, unreal.MaterialProperty.MP_NORMAL)
    else:
        normal_layers = 0

    if roughness:
        rough_sample = _sample(target, roughness, bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 930)
        rough_base = _multiply(target, rough_sample, _scalar(target, profile["roughness"], -1200, 980), -940, 930, "R")
    else:
        rough_base = _scalar(target, profile["roughness"], -940, 930)
    rough_final = _subtract(target, _add(target, rough_base, _multiply(target, grime_mask, _scalar(target, 0.18 if key != "Water" else 0.04, -300, 1010), -40, 930), 220, 930), _multiply(target, edge_mask, _scalar(target, 0.08, 0, 2380), 250, 2380), 500, 930)
    _connect_prop(rough_final, "", target, unreal.MaterialProperty.MP_ROUGHNESS)

    if metallic:
        metallic_output = _sample(target, metallic, bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 1190)
        _connect_prop(metallic_output, "R", target, unreal.MaterialProperty.MP_METALLIC)
    else:
        _connect_prop(_scalar(target, profile["metallic"], -650, 1190), "", target, unreal.MaterialProperty.MP_METALLIC)
    ao_final = _lerp(target, _scalar(target, 1.0, -900, 1350), ao_sample, _scalar(target, profile["ao"], -900, 1460), -650, 1190)
    _connect_prop(ao_final, "", target, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _connect_prop(_scalar(target, profile["specular"], 690, 1110), "", target, unreal.MaterialProperty.MP_SPECULAR)
    if profile["clearcoat"] > 0.0:
        for prop_name, value in (("MP_CLEAR_COAT", profile["clearcoat"]), ("MP_CLEAR_COAT_ROUGHNESS", 0.24)):
            prop = getattr(unreal.MaterialProperty, prop_name, None)
            if prop is not None:
                try:
                    _connect_prop(_scalar(target, value, 690, 1230), "", target, prop)
                except Exception:
                    pass
    if key == "Water":
        _connect_prop(_scalar(target, profile["opacity"], 690, 1410), "", target, unreal.MaterialProperty.MP_OPACITY)
    if profile.get("masked_cull"):
        opacity_mask = getattr(unreal.MaterialProperty, "MP_OPACITY_MASK", None)
        assert opacity_mask is not None, "UE 5.5 opacity-mask property unavailable"
        _connect_prop(_scalar(target, 0.0, 690, 1520), "", target, opacity_mask)

    lib.layout_material_expressions(target)
    lib.recompile_material(target)
    assert unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False), target_path
    map_paths = {}
    for channel in MAPS:
        tex = _map_asset(key, "H" if channel == "Height" else {"BaseColor": "BC", "Normal": "N", "Roughness": "R", "Metallic": "M", "AO": "AO"}[channel])
        if tex:
            map_paths[channel] = tex.get_path_name()
    graph_lines = ["macro source channel plus procedural large-scale breakup", "AO/cavity grime overlay", "stretched source-channel streak overlay", "Fresnel edge-wear overlay", "macro+micro strengthened normal layer", "height BumpOffset", "calibrated roughness/metallic/AO", "two-sided Nanite-compatible source-facing output"]
    if profile.get("masked_cull"):
        graph_lines.append("zero opacity-mask culls detached source vegetation shards")
    return target, {
        "path": target.get_path_name(),
        "maps": map_paths,
        "height_source": "authored H_4K" if height else "procedural Noise",
        "graph": graph_lines,
        "normal_layers": normal_layers,
        "height_ratio": profile["height"],
        "two_sided": True,
        "used_with_nanite": True,
        "blend_mode": "masked" if profile.get("masked_cull") else ("translucent" if key == "Water" else "opaque"),
        "opacity_mask": 0.0 if profile.get("masked_cull") else None,
        "clearcoat": profile["clearcoat"],
        "opacity": profile.get("opacity"),
    }


def _rebind_mesh(path, materials):
    mesh = _asset(path)
    rebound = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name)
        short = slot_name.replace("M_Dadongmen_", "")
        key = {"Stone": "Stone", "Plaster": "Plaster", "Wood": "Wood", "RoofClay": "RoofClay", "Dirt": "Dirt", "Water": "Water", "Vegetation": "Vegetation"}.get(short)
        if key and key in materials:
            mesh.set_material(index, materials[key])
            rebound.append({"index": index, "slot": slot_name, "material": materials[key].get_path_name()})
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), path
    return rebound


def _blueprint_components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if isinstance(component, unreal.StaticMeshComponent):
            result.append(component)
    return subsystem, library, result


def _rebind_blueprint(materials, detail_mesh):
    blueprint = _asset(BP_PATH)
    subsystem, library, components = _blueprint_components(blueprint)
    rebound = []
    for component in components:
        if component.get_name().startswith("DeepAAA_DetailGeometry"):
            continue
        if not component.static_mesh:
            continue
        changed = []
        detail_name = component.get_name()
        # Keep the pre-existing detail component ownership and transforms, but
        # bring its visible overrides onto the same calibrated Deep AAA family.
        # This removes the old brown/gray material split from the door and
        # ground layers without deleting or replacing those rollback-safe
        # components.
        detail_material_key = None
        if detail_name.startswith("Detail_Dadongmen_Door_"):
            if "Approach_Water" in detail_name:
                detail_material_key = "Water"
            elif "Approach_Path" in detail_name:
                detail_material_key = "Dirt"
            elif "IronStrap" in detail_name or "Stud" in detail_name:
                detail_material_key = "Iron"
            elif "Plank" in detail_name or "Leaf" in detail_name or "Brace" in detail_name:
                detail_material_key = "DoorWood"
            else:
                detail_material_key = "Stone"
        if detail_material_key in materials:
            component.set_material(0, materials[detail_material_key])
            changed.append(detail_material_key)
        for index, slot in enumerate(component.static_mesh.get_editor_property("static_materials")):
            slot_name = str(slot.material_slot_name)
            short = slot_name.replace("M_Dadongmen_", "")
            if short in materials:
                component.set_material(index, materials[short])
                changed.append(slot_name)
        if changed:
            rebound.append({"component": component.get_name(), "mesh": component.static_mesh.get_path_name(), "slots": changed})

    detail_component = next((c for c in components if c.get_name().startswith("DeepAAA_DetailGeometry")), None)
    if detail_component is None:
        handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
        root = next((h for h in handles if library.is_root_component(subsystem.k2_find_subobject_data_from_handle(h))), None)
        assert root, BP_PATH
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint, conform_transform_to_parent=False)
        handle, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, unreal.Text("DeepAAA_DetailGeometry"))
        detail_component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
    detail_component.set_static_mesh(detail_mesh)
    detail_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    detail_component.set_editor_property("relative_rotation", unreal.Rotator(roll=-90.0))
    detail_component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    detail_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    detail_component.set_collision_profile_name("NoCollision")
    detail_component.set_editor_property("visible", True)
    detail_component.set_editor_property("hidden_in_game", False)
    detail_component.set_editor_property("cast_shadow", True)
    for index, slot in enumerate(detail_mesh.get_editor_property("static_materials")):
        short = str(slot.material_slot_name).replace("M_Dadongmen_", "")
        if short in materials:
            detail_component.set_material(index, materials[short])
    try:
        detail_component.set_editor_property("component_tags", ["DadongmenDeepAAA", "RollbackSafeAdditive"])
    except Exception:
        pass
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), BP_PATH
    rebound.append({"component": detail_component.get_name(), "mesh": detail_mesh.get_path_name(), "slots": [str(s.material_slot_name) for s in detail_mesh.get_editor_property("static_materials")]})
    return blueprint, rebound, detail_component.get_name()


def _import_detail(materials):
    assert DETAIL_SOURCE.exists(), DETAIL_SOURCE
    detail_path = DETAIL_FOLDER + "/" + DETAIL_NAME + "." + DETAIL_NAME
    mesh = _asset(detail_path) if unreal.EditorAssetLibrary.does_asset_exist(detail_path) else None
    # Reimport the local sibling on every run so deterministic source edits
    # (such as attached vegetation being added after the first import) cannot
    # leave the saved DeepAAA mesh stale.
    if mesh is None or FORCE_DETAIL_REIMPORT:
        task = unreal.AssetImportTask()
        task.filename = str(DETAIL_SOURCE)
        task.destination_path = DETAIL_FOLDER
        task.destination_name = DETAIL_NAME
        task.automated, task.save, task.replace_existing = True, True, True
        ui = unreal.FbxImportUI()
        ui.import_mesh, ui.import_materials, ui.import_textures = True, False, False
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        ui.automated_import_should_detect_type = False
        ui.static_mesh_import_data.combine_meshes = True
        ui.static_mesh_import_data.generate_lightmap_u_vs = False
        ui.static_mesh_import_data.auto_generate_collision = False
        ui.static_mesh_import_data.build_nanite = True
        task.options = ui
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        old_flag = unreal.SystemLibrary.get_console_variable_bool_value("Interchange.FeatureFlags.Import.FBX")
        unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
        try:
            task.factory = unreal.FbxFactory()
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        finally:
            unreal.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX " + str(int(old_flag)))
        imported = [obj for obj in task.get_objects() if isinstance(obj, unreal.StaticMesh)]
        assert len(imported) == 1, task.imported_object_paths
        mesh = imported[0]
    assert isinstance(mesh, unreal.StaticMesh), detail_path
    slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        short = str(slot.material_slot_name).replace("M_Dadongmen_", "")
        assert short in materials, (slot.material_slot_name, list(materials))
        mesh.set_material(index, materials[short])
        slots.append(str(slot.material_slot_name))
    body = mesh.get_editor_property("body_setup")
    try:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    except Exception:
        pass
    try:
        body.set_editor_property("double_sided_geometry", True)
    except Exception:
        pass
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), detail_path
    return mesh, slots, detail_path


def main():
    assert DETAIL_MANIFEST.exists(), DETAIL_MANIFEST
    manifest = json.loads(DETAIL_MANIFEST.read_text(encoding="utf-8"))
    materials, records = {}, {}
    for key in MATERIALS:
        material, record = _build_material(key)
        materials[key] = material
        records[key] = record

    mesh_paths = [
        DEST + "/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0.SM_Dadongmen_LOD0",
        DEST + "/Meshes/SM_Dadongmen_OpenDoor_LOD0/SM_Dadongmen_OpenDoor_LOD0.SM_Dadongmen_OpenDoor_LOD0",
    ]
    source_rebounds = {path: _rebind_mesh(path, materials) for path in mesh_paths if unreal.EditorAssetLibrary.does_asset_exist(path)}
    detail_mesh, detail_slots, detail_path = _import_detail(materials)
    blueprint, rebound, detail_component = _rebind_blueprint(materials, detail_mesh)
    report = {
        "intent": "Dadongmen-only Deep AAA materials plus real additive hero geometry",
        "variant_suffix": SUFFIX,
        "blueprint": BP_PATH,
        "preview_level": PREVIEW_PATH,
        "materials": records,
        "source_mesh_rebounds": source_rebounds,
        "detail_mesh": detail_path,
        "detail_source": manifest,
        "detail_slots": detail_slots,
        "blueprint_component_count_rebound": len(rebound),
        "blueprint_material_components_rebound": len([row for row in rebound if row["component"] != detail_component]),
        "detail_component": detail_component,
        "detail_component_relative_rotation_roll": -90.0,
        "geometry_changed": True,
        "uvs_changed": False,
        "collision_changed": False,
        "placement_changed": False,
        "detail_collision_intent": "NoCollision additive component; source Blueprint collision and collision intent preserved",
        "rollback_assets": [
            DEST + "/Materials/M_Dadongmen_" + key + suffix
            for key in MATERIALS
            for suffix in ("_AAA", "_ReferenceTuned")
        ] + [
            DEST + "/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0.SM_Dadongmen_LOD0",
            DEST + "/Meshes/SM_Dadongmen_OpenDoor_LOD0/SM_Dadongmen_OpenDoor_LOD0.SM_Dadongmen_OpenDoor_LOD0",
        ],
        "scope": "Dadongmen only; no shared map or parallel gate asset touched",
    }
    report_path = ROOT / "Dadongmen_V4-deep-aaa.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("DADONGMEN_DEEP_AAA_APPLIED", json.dumps({"materials": len(records), "detail_vertices": manifest["vertex_count"], "detail_triangles": manifest["triangle_count"], "rebound": len(rebound), "report": str(report_path)}))


if __name__ == "__main__":
    main()
