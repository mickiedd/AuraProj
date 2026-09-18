"""Wuxianmen V5 reference tuning revision 3 — detail and weathering pass.

Applies to both `BP_Wuxianmen_V5_4K_Core` and `BP_Wuxianmen_V5_FullPBR`, which
carry identical geometry after the revision-2 correction.

What this pass adds, and why:

1. Stone weathering. The standing project rule is that these gates read too clean
   against the reference sheet, which shows heavily weathered masonry with dark
   staining and moss. The revision-2 stone material varied only per-instance tone.
   This pass adds, all deterministic and placement-independent:
     - a wall-height stain, darker toward the base of the wall;
     - a wider per-instance tone spread;
     - a restrained moss tint confined to the lower band of the wall.
   Wall height comes from a per-instance custom data value rather than world
   position, so the asset does not weather differently depending on where it is
   placed.

2. Wood. The wood role was untouched. Its 451 instances share one material slot
   with UV0 of 0..1 per unit cube, and the instances run from 0.80 m to 22.59 m
   (397 over 1 m, 61 over 3 m, 17 over 10 m), so the large pieces stretch the sheet
   up to 17x. A per-instance custom data value encodes a tiling derived from each
   instance's own size, so a 22.5 m roof deck tiles instead of smearing while a
   1.3 m column is unchanged. The tint is also warmed: the previous tint made the
   timber read as flat dark maroon.

3. Roof tile tone. The roof was a single uniform surface. A per-instance tone
   variation breaks it into weathered patches, matching the reference.

4. Ridge cap. The ridge cylinders were reusing the roof-strip UV contract, which
   maps arbitrarily on a cylinder. They get a dedicated material that runs the tile
   sheet along the cap axis at roughly 0.36 m per tile.

Deliberately not changed, with reasons recorded in the report:

- Nanite settings. Every mesh in both variants already carries
  `fallback_relative_error = 0.0` and `fallback_percent_triangles = 100.0`, which
  is the full-triangle fallback the tuning workflow asks for. Verified, not
  modified.
- The eight small ridge ornaments. An audit suspected they were buried after the
  revision-2 un-inversion; measuring the corrected roof surface under each one
  shows four sit 3 cm above the lower roof and four sit 26 cm low inside the main
  roof, which is within the ornaments' own 0.35 m size. Left in place.

Never saves a map. Writes a JSON report and asserts every invariant before save.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"

VARIANTS = {
    "Core": {
        "package": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core",
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
        "stone": {"BaseColor": "Stone_BaseColor_4K_Repaired", "Normal": "Stone_Normal_4K_Repaired",
                  "Roughness": "Stone_Roughness_4K_Repaired"},
        "roughness_channel": "R",
        "wood": {"BaseColor": "Wood_BaseColor_4K_Repaired", "Normal": "Wood_Normal_4K_Repaired",
                 "Roughness": "Wood_Roughness_4K_Repaired"},
        "wood_tiling_mode": "per_instance",
        "tile_sheet": {"BaseColor": "Roof_BaseColor_4K_Repaired", "Normal": "Roof_Normal_4K_Repaired",
                       "Roughness": "Roof_Roughness_4K_Repaired"},
        "stone_tiling": 1.0 / 6.36,
    },
    "FullPBR": {
        "package": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR",
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
        "stone": {"BaseColor": "Stone_BaseColor_4K_Repaired", "Normal": "Stone_Normal_4K_Repaired",
                  "MetallicRoughness": "Stone_MetallicRoughness_4K_Repaired"},
        "roughness_channel": "G",
        "wood": {"BaseColor": "Wood_BaseColor_4K_Repaired", "Normal": "Wood_Normal_4K_Repaired",
                 "MetallicRoughness": "Wood_MetallicRoughness_4K_Repaired"},
        "wood_tiling_mode": "fixed",
        "tile_sheet": {"BaseColor": "RoofTile_BaseColor_4K_TileSheet",
                       "Normal": "RoofTile_Normal_4K_TileSheet",
                       "Roughness": "RoofTile_Roughness_4K_TileSheet"},
        "stone_tiling": 0.06,
    },
}

TILE_COMPONENT = "HISM_002_lower_tile_1_077_GEN_VARIABLE"
RIDGE_COMPONENT = "HISM_004_ridge_000010_GEN_VARIABLE"
STONE_COMPONENT = "HISM_005_stone_004213_GEN_VARIABLE"
WOOD_COMPONENT = "HISM_006_wood_000451_GEN_VARIABLE"

# Existing revision-2 roof-strip contract, reused for the tile material.
ROOF_COLUMNS_PER_U = 14.629
ROOF_COURSES_PER_AUTHORED_V = 5.242
ROOF_AUTHORED_V = 0.4646
ROOF_COURSES_PER_REPEAT = 5.0
ROOF_REPEATS = 2.8
ROOF_TINT = (0.40, 0.43, 0.47, 1.0)

STONE_TINT = (0.53, 0.50, 0.44, 1.0)
STONE_TONE_BASE = 0.72
STONE_TONE_SPREAD = 0.50
STONE_STAIN_COLOUR = (0.58, 0.58, 0.56, 1.0)
STONE_MOSS_COLOUR = (0.42, 0.50, 0.30, 1.0)
STONE_MOSS_STRENGTH = 0.38
WALL_HEIGHT_CM = 955.0
WALL_HEIGHT_GAIN = 1.6
WALL_MOSS_FALLOFF = 2.2

WOOD_TINT = (0.78, 0.56, 0.40, 1.0)
WOOD_REFERENCE_SIZE_M = 1.3
WOOD_TILING_SPAN = 16.0
WOOD_FIXED_TILING = 0.3
WOOD_ROUGHNESS = 0.94

RIDGE_AXIAL_TILES = 75.0
RIDGE_U_TILING = RIDGE_AXIAL_TILES / ROOF_COLUMNS_PER_U
RIDGE_V_TILING = ROOF_AUTHORED_V

TILE_TONE_BASE = 0.86
TILE_TONE_SPREAD = 0.30

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _transform_record(value):
    return {"location": [float(value.translation.x), float(value.translation.y), float(value.translation.z)],
            "rotation": [float(value.rotation.rotator().pitch), float(value.rotation.rotator().yaw),
                         float(value.rotation.rotator().roll)],
            "scale": [float(value.scale3d.x), float(value.scale3d.y), float(value.scale3d.z)]}


def _transform_hash(component):
    payload = [_transform_record(component.get_instance_transform(i, False))
               for i in range(component.get_instance_count())]
    return hashlib.sha256(json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")).hexdigest()


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if _path(component) in seen:
            continue
        seen.add(_path(component))
        result.append(component)
    return result


def _rotator_from_record(record):
    return unreal.Rotator(pitch=record[0], yaw=record[1], roll=record[2])


def _quat_dot(a, b):
    return abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)


def _matches_baseline(component, expected, tolerance=1e-3):
    if component.get_instance_count() != len(expected):
        return False
    for index, item in enumerate(expected):
        transform = component.get_instance_transform(index, False)
        if any(abs(a - b) > tolerance for a, b in zip(
                [transform.translation.x, transform.translation.y, transform.translation.z], item["location"])):
            return False
        if any(abs(a - b) > tolerance for a, b in zip(
                [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z], item["scale"])):
            return False
        expected_quat = unreal.MathLibrary.conv_rotator_to_quaternion(_rotator_from_record(item["rotation"]))
        if _quat_dot(transform.rotation, expected_quat) < 1.0 - 1e-6:
            return False
    return True


def _rough_key(mapping):
    """The Core package ships a separate Roughness map; FullPBR packs it into G of MetallicRoughness."""
    for key in ("Roughness", "MetallicRoughness"):
        if key in mapping:
            return key
    raise KeyError(sorted(mapping))


def _local_box(mesh):
    bounds = mesh.get_bounds()
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def _aabb(transform, local_min, local_max):
    corners = []
    for x in (local_min.x, local_max.x):
        for y in (local_min.y, local_max.y):
            for z in (local_min.z, local_max.z):
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    transform.rotation,
                    unreal.Vector(x * transform.scale3d.x, y * transform.scale3d.y, z * transform.scale3d.z))
                corners.append(rotated + transform.translation)
    return ([min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)],
            [max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)])


def _seed(index, salt=0):
    return ((index * 2654435761 + salt * 40503) % 4294967296) / 4294967296.0


# --------------------------------------------------------------------------- #
# material helpers
# --------------------------------------------------------------------------- #

def _duplicate(source_path, new_name, folder):
    target = folder + "/" + new_name
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    source = EAL.load_asset(source_path)
    assert source, source_path
    asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(new_name, folder, source)
    assert asset, target
    return asset


def _node(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def _sample(material, texture_path, x, y, sampler):
    node = _node(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", EAL.load_asset(texture_path))
    node.set_editor_property("sampler_type", sampler)
    return node


def _scalar(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def _vec2(material, u, v, x, y):
    node = _node(material, unreal.MaterialExpressionConstant2Vector, x, y)
    node.set_editor_property("r", u)
    node.set_editor_property("g", v)
    return node


def _vec3(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _bin(material, cls, a, b, x, y, a_out="", b_out=""):
    node = _node(material, cls, x, y)
    assert MEL.connect_material_expressions(a, a_out, node, "A"), (cls, "A")
    assert MEL.connect_material_expressions(b, b_out, node, "B"), (cls, "B")
    return node


def _mul(material, a, b, x, y, a_out="", b_out=""):
    return _bin(material, unreal.MaterialExpressionMultiply, a, b, x, y, a_out, b_out)


def _add(material, a, b, x, y, a_out="", b_out=""):
    return _bin(material, unreal.MaterialExpressionAdd, a, b, x, y, a_out, b_out)


def _un(material, cls, source, x, y, out=""):
    node = _node(material, cls, x, y)
    assert MEL.connect_material_expressions(source, out, node, "")
    return node


def _lerp(material, a, b, alpha, x, y):
    node = _node(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    assert MEL.connect_material_expressions(a, "", node, "A")
    assert MEL.connect_material_expressions(b, "", node, "B")
    assert MEL.connect_material_expressions(alpha, "", node, "Alpha")
    return node


def _custom(material, index, x, y):
    node = _node(material, unreal.MaterialExpressionPerInstanceCustomData, x, y)
    node.set_editor_property("data_index", index)
    return node


def _uv_scaled_offset(material, tiling, x=-2100):
    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, x, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    scaled = _mul(material, coordinate, _vec2(material, tiling, tiling, x, 220), x + 200, 0)
    offset = _add(material, scaled, _custom(material, 0, x + 200, 440), x + 400, 0)
    return _un(material, unreal.MaterialExpressionFrac, offset, x + 600, 0)


def _connect_uv(uv_node, sampler):
    for pin in ("UVs", "Coordinates"):
        try:
            if MEL.connect_material_expressions(uv_node, "", sampler, pin):
                return pin
        except Exception:
            continue
    raise AssertionError("could not connect UV on " + sampler.get_name())


def _roughness(material, sample, channel, factor, x=-420, y=700):
    return _mul(material, sample, _scalar(material, factor, -700, 1000), x, y, channel)


def _finish(material):
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    assert EAL.save_loaded_asset(material)


# --------------------------------------------------------------------------- #
# materials
# --------------------------------------------------------------------------- #

def build_stone_material(config, folder, textures):
    material = _duplicate(folder + "/M_Stone_ReferenceTuned_RefTune2", "M_Stone_ReferenceTuned_RefTune3", folder)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    uv = _uv_scaled_offset(material, config["stone_tiling"])
    base = _sample(material, textures[config["stone"]["BaseColor"]], -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, textures[config["stone"]["Normal"]], -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough_sample = _sample(material, textures[config["stone"][_rough_key(config["stone"])]], -700, 700,
                           unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, rough_sample)]

    coloured = _mul(material, base, _vec3(material, STONE_TINT, -900, 220), -560, 80)

    # per-instance tone spread
    tone = _add(material, _mul(material, _custom(material, 2, -1900, 900),
                               _scalar(material, STONE_TONE_SPREAD, -1700, 1080), -1500, 900),
                _scalar(material, STONE_TONE_BASE, -1500, 1240), -1300, 900)
    toned = _mul(material, coloured, tone, -420, 80)

    # wall-height stain, darker toward the base
    height = _custom(material, 1, -1900, 1400)
    stain_alpha = _un(material, unreal.MaterialExpressionSaturate,
                      _mul(material, height, _scalar(material, WALL_HEIGHT_GAIN, -1700, 1580), -1500, 1400),
                      -1300, 1400)
    stain = _lerp(material, _vec3(material, STONE_STAIN_COLOUR, -1500, 1700),
                  _vec3(material, (1.0, 1.0, 1.0, 1.0), -1500, 1900), stain_alpha, -1100, 1500)
    stained = _mul(material, toned, stain, -300, 80)

    # restrained moss on the lower band
    moss_alpha = _un(material, unreal.MaterialExpressionSaturate,
                     _add(material, _mul(material, height, _scalar(material, -WALL_MOSS_FALLOFF, -1700, 2100),
                                         -1500, 2000),
                          _scalar(material, 1.0, -1500, 2180), -1300, 2000),
                     -1100, 2000)
    moss_amount = _mul(material, moss_alpha, _scalar(material, STONE_MOSS_STRENGTH, -1100, 2280), -900, 2000)
    mossed = _lerp(material, stained,
                   _mul(material, stained, _vec3(material, STONE_MOSS_COLOUR, -1100, 2400), -900, 2400),
                   moss_amount, -700, 2000)

    assert MEL.connect_material_property(mossed, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(_roughness(material, rough_sample, config["roughness_channel"], 1.10),
                                         "", unreal.MaterialProperty.MP_ROUGHNESS)
    if "MetallicRoughness" in config["stone"]:
        assert MEL.connect_material_property(rough_sample, "B", unreal.MaterialProperty.MP_METALLIC)
    _finish(material)
    return material, {
        "tiling": round(config["stone_tiling"], 6),
        "tint": list(STONE_TINT),
        "tone_range": [STONE_TONE_BASE, round(STONE_TONE_BASE + STONE_TONE_SPREAD, 3)],
        "wall_height_stain": {"gain": WALL_HEIGHT_GAIN, "base_colour": list(STONE_STAIN_COLOUR)},
        "moss": {"colour": list(STONE_MOSS_COLOUR), "strength": STONE_MOSS_STRENGTH,
                 "falloff": WALL_MOSS_FALLOFF},
        "custom_data": {"0": "uv offset", "1": "wall height 0..1", "2": "tone seed 0..1"},
        "uv_pins_connected": pins,
    }


def build_wood_material(config, folder, textures):
    material = _duplicate(folder + "/M_Wood_ReferenceTuned", "M_Wood_ReferenceTuned_RefTune3", folder)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    if config["wood_tiling_mode"] == "per_instance":
        tiling = _add(material, _mul(material, _custom(material, 0, -1600, 440),
                                     _scalar(material, WOOD_TILING_SPAN, -1400, 620), -1200, 440),
                      _scalar(material, 1.0, -1200, 800), -1000, 440)
        uv = _mul(material, coordinate, tiling, -800, 0)
    else:
        uv = _mul(material, coordinate, _vec2(material, WOOD_FIXED_TILING, WOOD_FIXED_TILING, -1400, 220),
                  -1200, 0)

    base = _sample(material, textures[config["wood"]["BaseColor"]], -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, textures[config["wood"]["Normal"]], -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough_sample = _sample(material, textures[config["wood"][_rough_key(config["wood"])]], -700, 700,
                           unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, rough_sample)]

    coloured = _mul(material, base, _vec3(material, WOOD_TINT, -900, 220), -560, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(_roughness(material, rough_sample, config["roughness_channel"], WOOD_ROUGHNESS),
                                         "", unreal.MaterialProperty.MP_ROUGHNESS)
    if "MetallicRoughness" in config["wood"]:
        assert MEL.connect_material_property(rough_sample, "B", unreal.MaterialProperty.MP_METALLIC)
    _finish(material)
    return material, {
        "tiling_mode": config["wood_tiling_mode"],
        "per_instance_tiling_range": [1.0, 1.0 + WOOD_TILING_SPAN] if config["wood_tiling_mode"] == "per_instance"
        else None,
        "fixed_tiling": WOOD_FIXED_TILING if config["wood_tiling_mode"] == "fixed" else None,
        "tint": list(WOOD_TINT),
        "uv_pins_connected": pins,
    }


def build_tile_material(config, folder, textures):
    material = _duplicate(folder + "/M_RoofTile_ReferenceTuned_RefTune2", "M_RoofTile_ReferenceTuned_RefTune3",
                          folder)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    u_tiling = 1.0 / ROOF_COLUMNS_PER_U
    v_scale = ROOF_AUTHORED_V * (ROOF_COURSES_PER_REPEAT / ROOF_COURSES_PER_AUTHORED_V)
    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -2100, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    scaled = _mul(material, coordinate, _vec2(material, u_tiling, ROOF_REPEATS, -2100, 220), -1900, 0)
    wrapped = _un(material, unreal.MaterialExpressionFrac, scaled, -1700, 0)
    uv = _mul(material, wrapped, _vec2(material, 1.0, v_scale, -1700, 220), -1500, 0)

    base = _sample(material, textures[config["tile_sheet"]["BaseColor"]], -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, textures[config["tile_sheet"]["Normal"]], -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough_sample = _sample(material, textures[config["tile_sheet"]["Roughness"]], -700, 700,
                           unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, rough_sample)]

    tone = _add(material, _mul(material, _custom(material, 0, -1600, 900),
                               _scalar(material, TILE_TONE_SPREAD, -1400, 1080), -1200, 900),
                _scalar(material, TILE_TONE_BASE, -1200, 1240), -1000, 900)
    coloured = _mul(material, _mul(material, base, _vec3(material, ROOF_TINT, -900, 220), -700, 80),
                    tone, -420, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(_roughness(material, rough_sample, "R", 1.04), "",
                                         unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {
        "u_tiling": round(u_tiling, 6), "v_repeat": ROOF_REPEATS, "v_scale": round(v_scale, 6),
        "tone_range": [TILE_TONE_BASE, round(TILE_TONE_BASE + TILE_TONE_SPREAD, 3)],
        "uv_pins_connected": pins,
    }


def build_ridge_material(config, folder, textures):
    material = _duplicate(folder + "/M_RoofTile_ReferenceTuned_RefTune2", "M_RidgeCap_ReferenceTuned_RefTune3",
                          folder)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    uv = _mul(material, coordinate, _vec2(material, RIDGE_U_TILING, RIDGE_V_TILING, -1600, 220), -1300, 0)

    base = _sample(material, textures[config["tile_sheet"]["BaseColor"]], -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, textures[config["tile_sheet"]["Normal"]], -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough_sample = _sample(material, textures[config["tile_sheet"]["Roughness"]], -700, 700,
                           unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, rough_sample)]

    coloured = _mul(material, base, _vec3(material, ROOF_TINT, -900, 220), -560, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(_roughness(material, rough_sample, "R", 1.04), "",
                                         unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {
        "axial_tiles": RIDGE_AXIAL_TILES,
        "u_tiling": round(RIDGE_U_TILING, 6), "v_tiling": round(RIDGE_V_TILING, 6),
        "tile_length_m": round(22.5 / RIDGE_AXIAL_TILES, 4),
        "uv_pins_connected": pins,
    }


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #

def _run_variant(variant, config):
    folder = config["package"] + "/Materials"
    texture_folder = config["package"] + "/Textures"
    textures = {name: texture_folder + "/" + name for name in set(
        list(config["stone"].values()) + list(config["wood"].values()) + list(config["tile_sheet"].values()))}

    baseline_path = ROOT / f"Wuxianmen_V5_{variant}-reftune3-baseline-20260917.json"
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    baseline_by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = EAL.load_asset(config["blueprint"])
    assert isinstance(blueprint, unreal.Blueprint), config["blueprint"]
    components = {component.get_name(): component for component in _components(blueprint)}
    assert set(components) == set(baseline_by_name), (sorted(components), sorted(baseline_by_name))

    report = {"variant": variant, "blueprint": config["blueprint"], "steps": {}}

    # ---- custom data payloads ------------------------------------------- #
    stone = components[STONE_COMPONENT]
    wood = components[WOOD_COMPONENT]
    tile = components[TILE_COMPONENT]
    ridge = components[RIDGE_COMPONENT]

    stone.set_editor_property("num_custom_data_floats", 3)
    assert int(stone.get_editor_property("num_custom_data_floats")) >= 3
    height_written = 0
    for index in range(stone.get_instance_count()):
        transform = stone.get_instance_transform(index, False)
        height = min(max(transform.translation.z / WALL_HEIGHT_CM, 0.0), 1.0)
        stone.set_custom_data_value(index, 0, float(_seed(index, 1)))
        stone.set_custom_data_value(index, 1, float(height))
        stone.set_custom_data_value(index, 2, float(_seed(index, 2)))
        height_written += 1

    wood_min, wood_max = _local_box(wood.static_mesh)
    wood_tiling = None
    if config["wood_tiling_mode"] == "per_instance":
        wood.set_editor_property("num_custom_data_floats", 1)
        assert int(wood.get_editor_property("num_custom_data_floats")) >= 1
        sizes = []
        for index in range(wood.get_instance_count()):
            transform = wood.get_instance_transform(index, False)
            low, high = _aabb(transform, wood_min, wood_max)
            size_m = max(high[0] - low[0], high[1] - low[1], high[2] - low[2]) / 100.0
            sizes.append(size_m)
            normalised = min(max((size_m / WOOD_REFERENCE_SIZE_M - 1.0) / WOOD_TILING_SPAN, 0.0), 1.0)
            wood.set_custom_data_value(index, 0, float(normalised))
        wood_tiling = {
            "reference_size_m": WOOD_REFERENCE_SIZE_M,
            "instances": len(sizes),
            "size_min_m": round(min(sizes), 4),
            "size_median_m": round(sorted(sizes)[len(sizes) // 2], 4),
            "size_max_m": round(max(sizes), 4),
            "tiling_min": 1.0,
            "tiling_max": 1.0 + WOOD_TILING_SPAN,
            "instances_above_tiling_1": int(sum(1 for size in sizes if size > WOOD_REFERENCE_SIZE_M)),
        }

    tile.set_editor_property("num_custom_data_floats", 1)
    assert int(tile.get_editor_property("num_custom_data_floats")) >= 1
    for index in range(tile.get_instance_count()):
        tile.set_custom_data_value(index, 0, float(_seed(index, 3)))

    report["steps"]["custom_data"] = {
        "stone": {"floats": 3, "written": height_written,
                  "layout": {"0": "uv offset", "1": "wall height", "2": "tone seed"}},
        "wood": {"floats": 1 if config["wood_tiling_mode"] == "per_instance" else 0,
                 "tiling": wood_tiling},
        "tile": {"floats": 1, "written": tile.get_instance_count()},
    }

    # ---- materials ------------------------------------------------------- #
    stone_material, stone_info = build_stone_material(config, folder, textures)
    wood_material, wood_info = build_wood_material(config, folder, textures)
    tile_material, tile_info = build_tile_material(config, folder, textures)
    ridge_material, ridge_info = build_ridge_material(config, folder, textures)

    bindings = {
        STONE_COMPONENT: stone_material,
        WOOD_COMPONENT: wood_material,
        TILE_COMPONENT: tile_material,
        RIDGE_COMPONENT: ridge_material,
    }
    for name, material in bindings.items():
        components[name].set_material(0, material)
    report["steps"]["materials"] = {
        "stone": stone_info, "wood": wood_info, "tile": tile_info, "ridge_cap": ridge_info,
        "bindings": {name: _path(material) for name, material in bindings.items()},
    }

    # ---- invariants ------------------------------------------------------ #
    post = {}
    for name, component in components.items():
        post[name] = {
            "instance_count": component.get_instance_count(),
            "mesh": _path(component.static_mesh),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "transform_hash": _transform_hash(component),
        }
        expected = baseline_by_name[name]
        assert post[name]["instance_count"] == expected["instance_count"], name
        assert post[name]["visible"] == expected["visible"], name
        assert post[name]["collision_profile"] == expected["collision_profile"], name
        assert post[name]["cast_shadow"] == expected["cast_shadow"], name
        assert post[name]["transform_hash"] == expected["transform_hash"], (name, "transforms changed")
    report["post_state"] = post
    report["instance_total"] = sum(item["instance_count"] for item in post.values())
    report["transform_payload_unchanged"] = True
    report["map_saved"] = False

    # ---- nanite review (verified, not modified) -------------------------- #
    report["steps"]["nanite_review"] = {
        name: {
            "enabled": baseline_by_name[name]["nanite_enabled"],
            "fallback_relative_error": baseline_by_name[name]["nanite_fallback_relative_error"],
            "fallback_percent_triangles": baseline_by_name[name]["nanite_fallback_percent_triangles"],
        }
        for name in baseline_by_name
    }
    report["steps"]["ridge_ornaments"] = {
        "action": "left in place",
        "reason": "the corrected roof surface under each ornament is within the ornament's own size: "
                  "four sit 3 cm above the lower roof, four sit 26 cm low inside the main roof",
    }

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    out = ROOT / f"Wuxianmen_V5_{variant}-reftune3-fix-20260917.json"
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE3_FIX", variant, out)
    print("REFTUNE3_FIX_COUNTS", variant, report["instance_total"], "transforms unchanged")
    print("REFTUNE3_FIX_MATERIALS", variant, json.dumps({k: v for k, v in report["steps"]["materials"].items()
                                                         if k != "bindings"}))


def main():
    dirty = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    print("REFTUNE3_DIRTY_MAPS", json.dumps(dirty))
    for variant, config in VARIANTS.items():
        _run_variant(variant, config)


if __name__ == "__main__":
    main()
