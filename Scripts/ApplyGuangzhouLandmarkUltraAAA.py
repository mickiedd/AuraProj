"""Apply the rollback-safe _UltraAAA material/geometry pass to five gates.

This script creates new materials and one new additive hero component per gate.
Existing meshes, materials, components and preview maps remain available. The
new graph samples authored maps directly and layers macro/meso/micro, cavity,
directional stain and Fresnel stages without multiplying the authored albedo by
a constant or rebinding an old material in place.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path("C:/Git/AuraProj")
ROOT = PROJECT / "Saved/RawModelImport/V4/UltraAAA"
SUFFIX = "_UltraAAA"
MAPS = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")


GATES = {
    "Wuxianmen": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/BP_Wuxianmen_V4",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen",
        "mesh_source": ROOT / "UltraAAA_Wuxianmen_V4_source.fbx",
        "mesh_folder": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/UltraAAA",
        "mesh_name": "Wuxianmen_V4_UltraAAA",
        "roll": -90.0,
        "roles": {
            "WeatheredStone": "M_WeatheredStone",
            "AgedWood": "M_AgedWood",
            "GateWood": "M_GateWood",
            "DarkMetal": "M_DarkMetal",
            "ClayRoof": "M_ClayRoof",
            "LimePlaster": "M_LimePlaster",
            "Plaque": "M_Plaque",
        },
        "role_map": {"WeatheredStone": "WeatheredStone", "AgedWood": "AgedWood", "GateWood": "GateWood", "DarkMetal": "DarkMetal", "ClayRoof": "ClayRoof", "LimePlaster": "LimePlaster", "Plaque": "Plaque"},
    },
    "Zhengximen": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/BP_Zhengximen_V4",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen",
        "mesh_source": ROOT / "UltraAAA_Zhengximen_V4_source.fbx",
        "mesh_folder": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Meshes/UltraAAA",
        "mesh_name": "Zhengximen_V4_UltraAAA",
        "roll": 0.0,
        "roles": {
            "GrayBrick": "M_GrayBrick",
            "StoneFoundation": "M_StoneFoundation",
            "AgedWood": "M_AgedWood",
            "DarkTimber": "M_DarkTimber",
            "ClayRoofTile": "M_ClayRoofTile",
            "BlackIron": "M_BlackIron",
            "GatePlaque": "M_GatePlaque",
        },
        "role_map": {"GrayBrick": "GrayBrick", "StoneFoundation": "StoneFoundation", "AgedWood": "AgedWood", "DarkTimber": "DarkTimber", "ClayRoofTile": "ClayRoofTile", "BlackIron": "BlackIron", "GatePlaque": "GatePlaque"},
    },
    "Dadongmen": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen",
        "mesh_source": ROOT / "UltraAAA_Dadongmen_V4_source.fbx",
        "mesh_folder": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Meshes/UltraAAA",
        "mesh_name": "Dadongmen_V4_UltraAAA",
        "roll": -90.0,
        "roles": {
            "Dadongmen_Stone": "M_Dadongmen_Stone",
            "Dadongmen_Plaster": "M_Dadongmen_Plaster",
            "Dadongmen_Wood": "M_Dadongmen_Wood",
            "Dadongmen_RoofClay": "M_Dadongmen_RoofClay",
            "Dadongmen_DoorWood": "M_Dadongmen_Wood",
            "Dadongmen_Iron": "M_Dadongmen_Iron_ReferenceTuned",
            "Dadongmen_Vegetation": "M_Dadongmen_Vegetation",
        },
        "role_map": {"Dadongmen_Stone": "Dadongmen_Stone", "Dadongmen_Plaster": "Dadongmen_Plaster", "Dadongmen_Wood": "Dadongmen_Wood", "Dadongmen_RoofClay": "Dadongmen_RoofClay", "Dadongmen_DoorWood": "Dadongmen_DoorWood", "Dadongmen_Iron": "Dadongmen_Iron", "Dadongmen_Vegetation": "Dadongmen_Vegetation"},
    },
    "Guidemen": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/BP_Guidemen_V4",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen",
        "mesh_source": ROOT / "UltraAAA_Guidemen_V4_source.fbx",
        "mesh_folder": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/Meshes/UltraAAA",
        "mesh_name": "Guidemen_V4_UltraAAA",
        "roll": -90.0,
        "roles": {
            "Stone_BlueGrey": "M_Stone_BlueGrey",
            "Plaster_OffWhite": "M_Plaster_OffWhite",
            "Wood_Aged": "M_Wood_Aged",
            "Tile_ClayGrey": "M_Tile_ClayGrey",
            "Metal_Bronze": "M_Metal_Bronze",
            "Plaque_Guide": "M_Plaque_Guide",
            "Inscription_Guide": "M_Inscription_Guide",
            "Sign_Guidemen": "M_Sign_Guidemen",
        },
        "role_map": {"Stone_BlueGrey": "Stone_BlueGrey", "Plaster_OffWhite": "Plaster_OffWhite", "Wood_Aged": "Wood_Aged", "Tile_ClayGrey": "Tile_ClayGrey", "Metal_Bronze": "Metal_Bronze", "Plaque_Guide": "Plaque_Guide", "Inscription_Guide": "Inscription_Guide", "Sign_Guidemen": "Sign_Guidemen"},
    },
    "Zhengnanmen": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity",
        "mesh_source": ROOT / "UltraAAA_Zhengnanmen_V4_source.fbx",
        "mesh_folder": "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Meshes/UltraAAA",
        "mesh_name": "Zhengnanmen_HighFidelity_UltraAAA",
        "roll": 0.0,
        "roles": {
            "Stone_Aged": "HF:Stone_Aged",
            "Wood_DarkAged": "HF:Wood_DarkAged",
            "Wood_RedLacquer": "HF:Wood_RedLacquer",
            "GlazedTile_Green": "HF:GlazedTile_Green",
            "Gold_RidgeOrnament": "HF:Gold_RidgeOrnament",
            "Signboard_Zhengnanmen": "HF:Signboard_Zhengnanmen",
            "DarkInterior": "HF:DarkInterior",
        },
        "role_map": {"Stone_Aged": "Stone_Aged", "Wood_DarkAged": "Wood_DarkAged", "Wood_RedLacquer": "Wood_RedLacquer", "GlazedTile_Green": "GlazedTile_Green", "Gold_RidgeOrnament": "Gold_RidgeOrnament", "Signboard_Zhengnanmen": "Signboard_Zhengnanmen", "DarkInterior": "DarkInterior"},
    },
}


def asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(source, source_output, target, target_input), (source, target)


def connect_property(source, output, material, prop):
    assert unreal.MaterialEditingLibrary.connect_material_property(source, output, prop)


def scalar(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", float(value))
    return node


def color(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value, 1.0))
    return node


def coord(material, tiling, x, y):
    node = expr(material, unreal.MaterialExpressionTextureCoordinate, x, y)
    node.set_editor_property("coordinate_index", 0)
    node.set_editor_property("u_tiling", float(tiling[0]))
    node.set_editor_property("v_tiling", float(tiling[1]))
    return node


def sample(material, texture, uv, sampler, x, y):
    node = expr(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    connect(uv, "", node, "UVs")
    return node


def binary(material, cls, a, b, x, y, ao="", bo=""):
    node = expr(material, cls, x, y)
    connect(a, ao, node, "A")
    connect(b, bo, node, "B")
    return node


def mul(material, a, b, x, y, ao="", bo=""):
    return binary(material, unreal.MaterialExpressionMultiply, a, b, x, y, ao, bo)


def add(material, a, b, x, y, ao="", bo=""):
    return binary(material, unreal.MaterialExpressionAdd, a, b, x, y, ao, bo)


def sub(material, a, b, x, y, ao="", bo=""):
    return binary(material, unreal.MaterialExpressionSubtract, a, b, x, y, ao, bo)


def lerp(material, a, b, alpha, x, y, ao="", bo="", alphao=""):
    node = expr(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    connect(a, ao, node, "A")
    connect(b, bo, node, "B")
    connect(alpha, alphao, node, "Alpha")
    return node


def noise(material, scale, x, y):
    node = expr(material, unreal.MaterialExpressionNoise, x, y)
    for prop, value in (("scale", scale), ("quality", 2), ("levels", 5), ("output_min", 0.0), ("output_max", 1.0)):
        try:
            node.set_editor_property(prop, value)
        except Exception:
            pass
    return node


def fresnel(material, x, y):
    node = expr(material, unreal.MaterialExpressionFresnel, x, y)
    try:
        node.set_editor_property("exponent", 4.0)
        node.set_editor_property("base_reflect_fraction", 0.04)
    except Exception:
        pass
    return node


def one_minus(material, node, x, y, output=""):
    return sub(material, scalar(material, 1.0, x - 160, y - 80), node, x, y, "", output)


def texture_name_matches(name, channel):
    n = name.lower()
    patterns = {
        "BaseColor": ("basecolor", "base_color", "_bc", "_albedo"),
        "Normal": ("normal", "_n", "_normalgl"),
        "Roughness": ("roughness", "_r", "_rough"),
        "Metallic": ("metallic", "_m", "_metal"),
        "AO": ("ambientocclusion", "_ao", "_occlusion"),
        "Height": ("height", "_h", "_displacement"),
    }
    return any(p in n for p in patterns[channel])


def source_maps(path):
    material = asset(path)
    textures = list(unreal.MaterialEditingLibrary.get_used_textures(material))
    result = {}
    for channel in MAPS:
        hit = next((tex for tex in textures if texture_name_matches(tex.get_name(), channel)), None)
        if hit:
            result[channel] = hit
    return result


def import_texture(source, destination, name, srgb):
    path = destination + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        tex = asset(path)
    else:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = destination
        task.destination_name = name
        task.automated, task.save, task.replace_existing = True, True, False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, (source, destination, name)
        tex = asset(task.imported_object_paths[0])
    tex.set_editor_property("srgb", bool(srgb))
    if not srgb:
        try:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        except Exception:
            pass
    unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def high_fidelity_maps():
    source_root = Path("C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package/Textures_4K_ImportOptimized")
    destination = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Textures/UltraAAA"
    files = {
        "GlazedTile_Green": "M_GlazedTile_Green",
        "Stone_Aged": "M_Stone_Aged",
        "Wood_DarkAged": "M_Wood_DarkAged",
        "Wood_RedLacquer": "M_Wood_RedLacquer",
    }
    result = {}
    for role, source_role in files.items():
        result[role] = {}
        for channel, suffix, srgb in (
            ("BaseColor", "BaseColor_4K.jpg", True),
            ("Normal", "Normal_4K.jpg", False),
            ("Roughness", "MetallicRoughness_4K.jpg", False),
            ("Metallic", "MetallicRoughness_4K.jpg", False),
            ("AO", "AO_4K.jpg", False),
            ("Height", "Height_2K_16bit.png", False),
        ):
            source = source_root / (source_role + "_" + suffix)
            assert source.is_file(), source
            asset_name = "T_" + role + "_" + channel + ("_2K" if channel == "Height" else "_4K")
            result[role][channel] = import_texture(source, destination + "/" + role, asset_name, srgb)
    # The HighFidelity package has no native six-map sets for gold ornament,
    # signboard or dark interior. Keep those roles distinct and explicit while
    # documenting the closest authored lineage fallback in the report.
    result["Gold_RidgeOrnament"] = result["GlazedTile_Green"]
    result["Signboard_Zhengnanmen"] = result["Wood_RedLacquer"]
    result["DarkInterior"] = result["Wood_DarkAged"]
    return result


def direct_v4_map(gate_name, role, channel):
    """Resolve a package-authored map even when the legacy material graph did not use it."""
    cfg = GATES[gate_name]
    if gate_name == "Wuxianmen":
        role_name = role
        path = cfg["destination"] + "/Textures/" + role_name + "/T_" + role_name + "_" + channel + "_4K"
    elif gate_name == "Zhengximen":
        role_name = role
        path = cfg["destination"] + "/Textures/T_" + role_name + "_" + channel
    elif gate_name == "Guidemen":
        aliases = {
            "Stone_BlueGrey": ("Stone", "T_Stone_BlueGrey"),
            "Plaster_OffWhite": ("Plaster", "T_Plaster_OffWhite"),
            "Wood_Aged": ("Wood", "T_Wood_Aged"),
            "Tile_ClayGrey": ("Tile", "T_Tile_ClayGrey"),
            "Metal_Bronze": ("Metal", "T_Metal_Bronze"),
            "Plaque_Guide": ("Plaques", "M_Plaque_Guide"),
            "Inscription_Guide": ("Plaques", "M_Inscription_Guide"),
            "Sign_Guidemen": ("Plaques", "M_Sign_Guidemen"),
        }
        folder, prefix = aliases[role]
        suffix = "NormalGL" if channel == "Normal" else channel
        path = cfg["destination"] + "/Textures/" + folder + "/" + prefix + "_" + suffix
    elif gate_name == "Dadongmen":
        short = role.removeprefix("Dadongmen_")
        short = {"DoorWood": "Wood", "Iron": "Stone", "Vegetation": "Stone"}.get(short, short)
        suffix = {"BaseColor": "BC", "Normal": "N", "Roughness": "R", "Metallic": "M", "AO": "AO", "Height": "H"}[channel]
        path = cfg["destination"] + "/Textures/" + short + "/T_" + short + "_" + suffix + "_4K"
    else:
        return None
    return unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def build_material(path, maps, role, gate_name, fallback=None):
    destination = GATES[gate_name]["destination"] + "/Materials"
    material_path = destination + "/M_" + role + SUFFIX
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = asset(material_path)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_" + role + SUFFIX, destination, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    # Deliberately non-uniform UV stages: each is a separate authored layer.
    uv_macro = coord(material, (0.83, 1.17), -1700, -80)
    uv_meso = coord(material, (4.65, 3.25), -1700, 520)
    uv_micro = coord(material, (19.0, 14.0), -1700, 1120)
    uv_streak = coord(material, (0.61, 8.7), -1700, 1700)
    height_source = sample(material, maps["Height"], uv_macro, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 190)
    bump = expr(material, unreal.MaterialExpressionBumpOffset, -1180, 190)
    connect(uv_macro, "", bump, "Coordinate")
    connect(height_source, "", bump, "Height")
    try:
        bump.set_editor_property("height_ratio", 0.075 if "Tile" not in role and "Roof" not in role else 0.105)
        bump.set_editor_property("reference_plane", 0.5)
    except Exception:
        pass

    # Base albedo is the authored source directly; all age layers are Lerp
    # masks, never a scalar multiply/tint-and-rebind shortcut.
    base = sample(material, maps["BaseColor"], bump, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1450, -360)
    meso_source = sample(material, maps["BaseColor"], uv_meso, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1450, -20)
    macro_noise = noise(material, 0.72, -1450, 420)
    macro_alpha = mul(material, macro_noise, scalar(material, 0.17, -1250, 450), -1000, 420)
    macro_breakup = lerp(material, base, meso_source, macro_alpha, -740, -360, "RGB", "RGB")
    cavity_source = sample(material, maps["AO"], bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 820)
    cavity = one_minus(material, cavity_source, -1180, 820)
    grime_noise = noise(material, 2.15, -1180, 1010)
    grime_mask = mul(material, cavity, grime_noise, -940, 820)
    grime_alpha = mul(material, grime_mask, scalar(material, 0.24, -720, 900), -500, 820)
    grime_layer = lerp(material, macro_breakup, color(material, (0.16, 0.12, 0.09), -720, -40), grime_alpha, -230, -360, "", "")
    streak_noise = noise(material, 1.8, -1180, 1570)
    streak_mask = mul(material, streak_noise, scalar(material, 0.14, -960, 1600), -720, 1460)
    directional_stain = lerp(material, grime_layer, color(material, (0.20, 0.16, 0.12), -720, 140), streak_mask, 40, -360, "", "")
    edge = fresnel(material, -250, 1960)
    edge_alpha = mul(material, edge, scalar(material, 0.08, -40, 2040), 190, 1900)
    final_base = lerp(material, directional_stain, color(material, (0.52, 0.44, 0.34), 0, 150), edge_alpha, 470, -360, "", "")
    connect_property(final_base, "", material, unreal.MaterialProperty.MP_BASE_COLOR)

    # Macro + micro normal detail is a separate stage, with visible relief also
    # carried by the height-driven BumpOffset used by the base/AO samples.
    macro_normal = sample(material, maps["Normal"], bump, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1450, 1080)
    micro_normal = sample(material, maps["Normal"], uv_micro, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1450, 1340)
    blend_class = getattr(unreal, "MaterialExpressionBlendAngleCorrectedNormals", None)
    if blend_class:
        blended = expr(material, blend_class, -900, 1120)
        connect(macro_normal, "RGB", blended, "BaseNormal")
        connect(micro_normal, "RGB", blended, "AdditionalNormal")
        connect_property(blended, "", material, unreal.MaterialProperty.MP_NORMAL)
    else:
        normal_mix = add(material, macro_normal, mul(material, micro_normal, scalar(material, 0.36, -1180, 1260), -980, 1240, "RGB"), -700, 1120, "RGB")
        connect_property(normal_mix, "", material, unreal.MaterialProperty.MP_NORMAL)

    rough_source = sample(material, maps["Roughness"], bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 740)
    rough_noise = noise(material, 1.25, -1180, 1850)
    rough_breakup = lerp(material, rough_source, rough_noise, scalar(material, 0.26, -860, 1820), -600, 740, "R", "")
    rough_final = lerp(material, rough_breakup, grime_noise, scalar(material, 0.12, -390, 1820), -120, 740, "", "")
    connect_property(rough_final, "", material, unreal.MaterialProperty.MP_ROUGHNESS)

    metallic = sample(material, maps["Metallic"], bump, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1450, 1240)
    connect_property(metallic, "R", material, unreal.MaterialProperty.MP_METALLIC)
    ao_final = lerp(material, scalar(material, 1.0, -900, 1510), cavity_source, scalar(material, 0.86, -900, 1620), -600, 1440, "", "R")
    connect_property(ao_final, "", material, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    connect_property(scalar(material, 0.34, 400, 1060), "", material, unreal.MaterialProperty.MP_SPECULAR)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False), material_path
    return material, {
        "path": material_path,
        "role": role,
        "maps": {channel: tex.get_path_name() for channel, tex in maps.items()},
        "texture_resolutions": {channel: [tex.blueprint_get_size_x(), tex.blueprint_get_size_y()] for channel, tex in maps.items()},
        "expression_count": lib.get_num_material_expressions(material),
        "stages": ["large-scale macro colour and roughness breakup", "meso detail", "micro normal detail", "cavity/AO grime", "directional streak and staining", "Fresnel/curvature edge wear", "height BumpOffset/parallax"],
        "base_colour_shortcut": False,
        "uniform_tiling": False,
        "source_fallback": fallback,
    }


def role_materials(gate_name):
    cfg = GATES[gate_name]
    maps_by_role = {}
    records = {}
    hf = high_fidelity_maps() if gate_name == "Zhengnanmen" else None
    for role, source in cfg["roles"].items():
        fallback = None
        if gate_name == "Zhengnanmen":
            source_role = source[3:]
            if source_role not in hf:
                source_role = {"Gold_RidgeOrnament": "GlazedTile_Green", "Signboard_Zhengnanmen": "Wood_RedLacquer", "DarkInterior": "Wood_DarkAged"}[source_role]
                fallback = source_role
            maps = hf[source_role]
        else:
            source_path = cfg["destination"] + "/Materials/" + source
            maps = source_maps(source_path)
            # Height is present in the packages but was not connected by some
            # older preview materials. Resolve every missing channel against
            # the native imported texture asset before using a fallback set.
            for channel in MAPS:
                if channel not in maps:
                    direct = direct_v4_map(gate_name, role, channel)
                    if direct:
                        maps[channel] = direct
            if len(maps) != len(MAPS):
                # Vegetation and legacy iron are the only known V4 exceptions;
                # use an authored six-map lineage fallback and record it.
                fallback = "M_Dadongmen_Stone" if gate_name == "Dadongmen" and role in ("Dadongmen_Iron", "Dadongmen_Vegetation") else "source material missing channel"
                fallback_source = cfg["destination"] + "/Materials/" + ("M_Dadongmen_Stone" if gate_name == "Dadongmen" and role in ("Dadongmen_Iron", "Dadongmen_Vegetation") else source)
                maps = dict(maps)
                for channel in MAPS:
                    if channel not in maps:
                        maps[channel] = source_maps(fallback_source).get(channel)
                assert all(maps.get(channel) for channel in MAPS), (gate_name, role, maps)
        material, record = build_material(cfg["destination"] + "/Materials/M_" + role + SUFFIX, maps, role, gate_name, fallback)
        maps_by_role[role] = material
        records[role] = record
    return maps_by_role, records


def component_rows(blueprint):
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    result = []
    seen = set()
    for handle in ss.k2_gather_subobject_data_for_blueprint(blueprint):
        data = ss.k2_find_subobject_data_from_handle(handle)
        comp = lib.get_object(data)
        if isinstance(comp, unreal.StaticMeshComponent) and comp.get_name() not in seen:
            result.append(comp)
            seen.add(comp.get_name())
    return ss, lib, result


def infer_role(gate_name, material_name, component_name, roles):
    text = (material_name + " " + component_name).lower()
    for role in roles:
        key = role.lower().replace("_ultraaaa", "")
        if key in text:
            return role
    if gate_name == "Dadongmen":
        if any(x in text for x in ("iron", "stud", "strap", "hardware", "brace")): return "Dadongmen_Iron"
        if any(x in text for x in ("door", "plank", "leaf")): return "Dadongmen_DoorWood"
        if any(x in text for x in ("vine", "vegetation", "leaf")): return "Dadongmen_Vegetation"
        if "plaster" in text: return "Dadongmen_Plaster"
        if "roof" in text or "tile" in text: return "Dadongmen_RoofClay"
        if "wood" in text or "timber" in text: return "Dadongmen_Wood"
        if "water" in text: return None
        return "Dadongmen_Stone"
    return None


def add_component_and_rebind(gate_name, materials):
    cfg = GATES[gate_name]
    blueprint = asset(cfg["blueprint"])
    ss, lib, components = component_rows(blueprint)
    rebound = []
    for component in components:
        changed = []
        for index in range(component.get_num_materials()):
            current = component.get_material(index)
            current_name = current.get_name() if current else ""
            role = infer_role(gate_name, current_name, component.get_name(), materials)
            if role and role in materials:
                component.set_material(index, materials[role])
                changed.append(role)
        if changed:
            rebound.append({"component": component.get_name(), "roles": changed})
    # This generated mesh is a complete gate with incompatible floor heights,
    # not detail that can be layered over the reference-shaped packed V2 model.
    # Keep material rebinding, but never remount a second Zhengnanmen assembly.
    if gate_name == "Zhengnanmen":
        overlays = [(h, lib.get_object(ss.k2_find_subobject_data_from_handle(h)))
                    for h in ss.k2_gather_subobject_data_for_blueprint(blueprint)]
        root = next(h for h, c in overlays
                    if lib.is_root_component(ss.k2_find_subobject_data_from_handle(h)))
        seen = set()
        for handle, component in overlays:
            if component and component.get_name().startswith("UltraAAA_HeroGeometry") and component.get_path_name() not in seen:
                seen.add(component.get_path_name())
                assert ss.delete_subobject(root, handle, blueprint) == 1
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
        return {"blueprint": cfg["blueprint"], "rebound_components": len(rebound), "rebound": rebound,
                "detail_component": None, "detail_mesh": None, "detail_slots": [],
                "removed_overlay_components": len(seen),
                "geometry_policy": "packed_reference_model_only"}
    detail = next((c for c in components if c.get_name().startswith("UltraAAA_HeroGeometry")), None)
    if detail is None:
        handles = ss.k2_gather_subobject_data_for_blueprint(blueprint)
        root = next((h for h in handles if lib.is_root_component(ss.k2_find_subobject_data_from_handle(h))), None)
        assert root, cfg["blueprint"]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint, conform_transform_to_parent=False)
        handle, reason = ss.add_new_subobject(params)
        assert lib.is_handle_valid(handle), str(reason)
        ss.rename_subobject(handle, unreal.Text("UltraAAA_HeroGeometry"))
        detail = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
    mesh_path = cfg["mesh_folder"] + "/" + cfg["mesh_name"] + "." + cfg["mesh_name"]
    mesh = asset(mesh_path)
    detail.set_static_mesh(mesh)
    detail.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    detail.set_editor_property("relative_rotation", unreal.Rotator(roll=cfg["roll"]))
    detail.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    detail.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    detail.set_collision_profile_name("NoCollision")
    detail.set_editor_property("visible", True)
    detail.set_editor_property("hidden_in_game", False)
    detail.set_editor_property("cast_shadow", True)
    detail.set_editor_property("component_tags", ["UltraAAA", "RollbackSafeAdditive", "VisibleSurfaceRelief"])
    slot_roles = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name).removeprefix("M_").removesuffix("_UltraAAA")
        if slot_name in materials:
            detail.set_material(index, materials[slot_name])
            slot_roles.append(slot_name)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), cfg["blueprint"]
    return {"blueprint": cfg["blueprint"], "rebound_components": len(rebound), "rebound": rebound, "detail_component": detail.get_name(), "detail_mesh": mesh_path, "detail_slots": slot_roles}


def import_mesh(cfg):
    mesh_path = cfg["mesh_folder"] + "/" + cfg["mesh_name"] + "." + cfg["mesh_name"]
    if unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
        return asset(mesh_path)
    task = unreal.AssetImportTask()
    task.filename = str(cfg["mesh_source"])
    task.destination_path = cfg["mesh_folder"]
    task.destination_name = cfg["mesh_name"]
    task.automated, task.save, task.replace_existing = True, True, False
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
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return mesh


def run_gate(name):
    cfg = GATES[name]
    assert cfg["mesh_source"].is_file(), cfg["mesh_source"]
    mesh = import_mesh(cfg)
    materials, material_records = role_materials(name)
    applied = add_component_and_rebind(name, materials)
    source_manifest = json.loads((ROOT / (name + "_V4_UltraAAA.json")).read_text(encoding="utf-8"))
    result = {
        "intent": "packed reference geometry with authored materials" if name == "Zhengnanmen" else "rollback-safe UltraAAA additive hero geometry and authored six-map layered materials",
        "gate": name,
        "variant_suffix": SUFFIX,
        "blueprint": cfg["blueprint"],
        "detail_mesh": applied["detail_mesh"],
        "detail_source_manifest": source_manifest,
        "materials": material_records,
        "applied": applied,
        "geometry_changed": name != "Zhengnanmen" or bool(applied.get("removed_overlay_components", 0)),
        "uvs_changed": False,
        "collision_changed": False,
        "placement_changed": False,
        "rollback_assets_preserved": True,
        "capture_requirements": ["front", "door close", "roof-corner close", "rear", "raking-light close", "3x3 tiling", "wireframe", "normals"],
        "known_source_blocker": "Zhengnanmen HighFidelity source has no native six-map texture sets for gold/signboard/interior; closest authored HighFidelity map sets are duplicated as explicit fallbacks for those roles." if name == "Zhengnanmen" else None,
    }
    out = ROOT / (name + "_V4-UltraAAA.json")
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("GUANGZHOU_ULTRA_AAA_APPLIED", json.dumps({"gate": name, "triangles": source_manifest["triangle_count"], "materials": len(material_records), "expressions": {k: v["expression_count"] for k, v in material_records.items()}, "report": str(out)}))


def main():
    for name in ("Wuxianmen", "Zhengximen", "Dadongmen", "Guidemen", "Zhengnanmen"):
        run_gate(name)


if __name__ == "__main__":
    main()
