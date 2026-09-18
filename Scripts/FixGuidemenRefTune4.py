"""Guidemen V5 4K detail pass — per-instance weathering on the stone and tile roles.

Guidemen is the V5 sibling of Wuxianmen and uses the same `_ReferenceTuned` material
family, but it never received the revision-2/3 treatment. Its two largest surfaces
are uniform:

- 2,170 stone-block instances (`SB_0` 1,806, `BA_0` 182, `BB_0` 182) sharing
  `M_WeatheredStone_4K_ReferenceTuned` with no per-instance variation;
- 16,248 roof-tile instances (`R1_T0`) sharing
  `M_GrayClayTile_2K_ReferenceTuned`, again uniform.

Unlike Wuxianmen, these materials also sample an AO map, so the rebuild replicates
that hookup: BaseColor x tint -> BaseColor, Normal -> Normal, MetallicRoughness
G -> Roughness and B -> Metallic, AO R -> Ambient Occlusion.

Added, all deterministic and placement-independent via per-instance custom data:

- stone: UV offset, a wall-height stain (darker toward the base), a wider
  per-instance tone, and a restrained moss tint confined to the lower band;
- roof tile: a per-instance tone so the roof is not one flat surface.

Only the components that actually use those materials are touched, and only their
material bindings and custom-data payloads — no instance transform is changed.

Never saves a map.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport"
AUDIT = ROOT / "GuangzhouLandmarks-audit-v2-20260918.json"
BASELINE = ROOT / "Guidemen_V5_4K-reftune4-baseline-20260918.json"
REPORT = ROOT / "Guidemen_V5_4K-reftune4-fix-20260918.json"

PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K"
BLUEPRINT = PACKAGE + "/BP_Guidemen_V5_4K"
MATERIALS = PACKAGE + "/Materials"
TEXTURES = PACKAGE + "/Textures"

STONE_SOURCE = MATERIALS + "/M_WeatheredStone_4K_ReferenceTuned"
TILE_SOURCE = MATERIALS + "/M_GrayClayTile_2K_ReferenceTuned"
STONE_MATERIAL = "M_WeatheredStone_4K_ReferenceTuned_RefTune4"
TILE_MATERIAL = "M_GrayClayTile_2K_ReferenceTuned_RefTune4"

STONE_TINT = (0.53, 0.50, 0.44, 1.0)
STONE_TONE_BASE = 0.72
STONE_TONE_SPREAD = 0.50
STONE_STAIN_COLOUR = (0.58, 0.58, 0.56, 1.0)
STONE_MOSS_COLOUR = (0.42, 0.50, 0.30, 1.0)
STONE_MOSS_STRENGTH = 0.38
STONE_HEIGHT_GAIN = 1.6
STONE_MOSS_FALLOFF = 2.2

TILE_TINT = (0.43, 0.46, 0.50, 1.0)
TILE_TONE_BASE = 0.86
TILE_TONE_SPREAD = 0.30

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def _path(value):
    return value.get_path_name() if value else ""


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


def _transform_hash(component):
    payload = []
    for index in range(component.get_instance_count()):
        transform = component.get_instance_transform(index, False)
        rotator = transform.rotation.rotator()
        payload.append([round(transform.translation.x, 4), round(transform.translation.y, 4),
                        round(transform.translation.z, 4), round(rotator.pitch, 5),
                        round(rotator.yaw, 5), round(rotator.roll, 5),
                        round(transform.scale3d.x, 6), round(transform.scale3d.y, 6),
                        round(transform.scale3d.z, 6)])
    return hashlib.sha256(json.dumps(payload, separators=(",", ":")).encode("utf-8")).hexdigest()


def _seed(index, salt=0):
    return ((index * 2654435761 + salt * 40503) % 4294967296) / 4294967296.0


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
    assert MEL.connect_material_expressions(a, a_out, node, "A")
    assert MEL.connect_material_expressions(b, b_out, node, "B")
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


def _connect_uv(uv_node, sampler):
    for pin in ("UVs", "Coordinates"):
        try:
            if MEL.connect_material_expressions(uv_node, "", sampler, pin):
                return pin
        except Exception:
            continue
    raise AssertionError("could not connect UV on " + sampler.get_name())


def _finish(material):
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    assert EAL.save_loaded_asset(material)


def _duplicate(source_path, new_name):
    target = MATERIALS + "/" + new_name
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    source = EAL.load_asset(source_path)
    assert source, source_path
    asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(new_name, MATERIALS, source)
    assert asset, target
    return asset


def build_stone_material():
    material = _duplicate(STONE_SOURCE, STONE_MATERIAL)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -2100, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    offset = _add(material, coordinate, _custom(material, 0, -1900, 440), -1700, 0)
    uv = _un(material, unreal.MaterialExpressionFrac, offset, -1500, 0)

    base = _sample(material, TEXTURES + "/M_WeatheredStone_BaseColor", -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, TEXTURES + "/M_WeatheredStone_Normal", -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    metal_rough = _sample(material, TEXTURES + "/M_WeatheredStone_MetallicRoughness", -700, 700,
                          unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    ao = _sample(material, TEXTURES + "/M_WeatheredStone_AO", -700, 1100,
                 unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, metal_rough, ao)]

    coloured = _mul(material, base, _vec3(material, STONE_TINT, -900, 220), -560, 80)
    tone = _add(material, _mul(material, _custom(material, 2, -1900, 900),
                               _scalar(material, STONE_TONE_SPREAD, -1700, 1080), -1500, 900),
                _scalar(material, STONE_TONE_BASE, -1500, 1240), -1300, 900)
    toned = _mul(material, coloured, tone, -420, 80)
    height = _custom(material, 1, -1900, 1400)
    stain_alpha = _un(material, unreal.MaterialExpressionSaturate,
                      _mul(material, height, _scalar(material, STONE_HEIGHT_GAIN, -1700, 1580), -1500, 1400),
                      -1300, 1400)
    stain = _lerp(material, _vec3(material, STONE_STAIN_COLOUR, -1500, 1700),
                  _vec3(material, (1.0, 1.0, 1.0, 1.0), -1500, 1900), stain_alpha, -1100, 1500)
    stained = _mul(material, toned, stain, -300, 80)
    moss_alpha = _un(material, unreal.MaterialExpressionSaturate,
                     _add(material, _mul(material, height, _scalar(material, -STONE_MOSS_FALLOFF, -1700, 2100),
                                         -1500, 2000),
                          _scalar(material, 1.0, -1500, 2180), -1300, 2000),
                     -1100, 2000)
    mossed = _lerp(material, stained,
                   _mul(material, stained, _vec3(material, STONE_MOSS_COLOUR, -1100, 2400), -900, 2400),
                   _mul(material, moss_alpha, _scalar(material, STONE_MOSS_STRENGTH, -1100, 2280), -900, 2000),
                   -700, 2000)
    assert MEL.connect_material_property(mossed, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(
        _mul(material, metal_rough, _scalar(material, 1.10, -700, 1400), -420, 700, "G"),
        "", unreal.MaterialProperty.MP_ROUGHNESS)
    assert MEL.connect_material_property(metal_rough, "B", unreal.MaterialProperty.MP_METALLIC)
    assert MEL.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _finish(material)
    return material, {"tiling": 1.0, "tint": list(STONE_TINT),
                      "tone_range": [STONE_TONE_BASE, round(STONE_TONE_BASE + STONE_TONE_SPREAD, 3)],
                      "stain_gain": STONE_HEIGHT_GAIN, "moss_strength": STONE_MOSS_STRENGTH,
                      "ao_hookup": "preserved", "uv_pins_connected": pins}


def build_tile_material():
    material = _duplicate(TILE_SOURCE, TILE_MATERIAL)
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    base = _sample(material, TEXTURES + "/M_GrayClayTile_BaseColor", -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, TEXTURES + "/M_GrayClayTile_Normal", -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    metal_rough = _sample(material, TEXTURES + "/M_GrayClayTile_MetallicRoughness", -700, 700,
                          unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    ao = _sample(material, TEXTURES + "/M_GrayClayTile_AO", -700, 1100,
                 unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(coordinate, sampler) for sampler in (base, normal, metal_rough, ao)]
    tone = _add(material, _mul(material, _custom(material, 0, -1200, 900),
                               _scalar(material, TILE_TONE_SPREAD, -1000, 1080), -800, 900),
                _scalar(material, TILE_TONE_BASE, -800, 1240), -600, 900)
    coloured = _mul(material, _mul(material, base, _vec3(material, TILE_TINT, -900, 220), -700, 80),
                    tone, -420, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(
        _mul(material, metal_rough, _scalar(material, 1.06, -700, 1400), -420, 700, "G"),
        "", unreal.MaterialProperty.MP_ROUGHNESS)
    assert MEL.connect_material_property(metal_rough, "B", unreal.MaterialProperty.MP_METALLIC)
    assert MEL.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _finish(material)
    return material, {"tint": list(TILE_TINT),
                      "tone_range": [TILE_TONE_BASE, round(TILE_TONE_BASE + TILE_TONE_SPREAD, 3)],
                      "ao_hookup": "preserved", "uv_pins_connected": pins}


def main():
    audit = json.loads(AUDIT.read_text(encoding="utf-8"))
    guidemen = next(b for b in audit["buildings"] if b["label"] == "Guidemen_V5_4K")
    stone_components = [g["name"] for g in guidemen["groups"]
                        if any("WeatheredStone" in m for m in g.get("materials", []))]
    tile_components = [g["name"] for g in guidemen["groups"]
                       if any("GrayClayTile" in m for m in g.get("materials", []))]
    print("REFTUNE4_TARGETS stone", stone_components, "tile", tile_components)

    blueprint = EAL.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = {component.get_name(): component for component in _components(blueprint)}

    baseline = {"created": "2026-09-18", "blueprint": BLUEPRINT, "components": []}
    for name, component in components.items():
        baseline["components"].append({
            "name": name,
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
        })
    BASELINE.write_text(json.dumps(baseline, indent=2), encoding="utf-8")

    report = {"created": "2026-09-18", "blueprint": BLUEPRINT, "steps": {}}

    # wall height for the stain, from the stone instances' own Z range
    zs = []
    for name in stone_components:
        component = components[name]
        for index in range(component.get_instance_count()):
            zs.append(component.get_instance_transform(index, False).translation.z)
    z_low, z_high = (min(zs), max(zs)) if zs else (0.0, 1.0)
    span = max(z_high - z_low, 1.0)

    stone_material, stone_info = build_stone_material()
    tile_material, tile_info = build_tile_material()

    written = {"stone": 0, "tile": 0}
    for name in stone_components:
        component = components[name]
        component.set_editor_property("num_custom_data_floats", 3)
        assert int(component.get_editor_property("num_custom_data_floats")) >= 3
        for index in range(component.get_instance_count()):
            z = component.get_instance_transform(index, False).translation.z
            component.set_custom_data_value(index, 0, float(_seed(index, 1)))
            component.set_custom_data_value(index, 1, float(min(max((z - z_low) / span, 0.0), 1.0)))
            component.set_custom_data_value(index, 2, float(_seed(index, 2)))
            written["stone"] += 1
        component.set_material(0, stone_material)

    for name in tile_components:
        component = components[name]
        component.set_editor_property("num_custom_data_floats", 1)
        assert int(component.get_editor_property("num_custom_data_floats")) >= 1
        for index in range(component.get_instance_count()):
            component.set_custom_data_value(index, 0, float(_seed(index, 3)))
            written["tile"] += 1
        component.set_material(0, tile_material)

    report["steps"]["custom_data"] = {
        "written": written,
        "stone_layout": {"0": "uv offset", "1": "wall height 0..1", "2": "tone seed"},
        "wall_height_source": {"z_low_cm": round(z_low, 2), "z_high_cm": round(z_high, 2)},
    }
    report["steps"]["materials"] = {"stone": stone_info, "tile": tile_info,
                                    "stone_material": MATERIALS + "/" + STONE_MATERIAL,
                                    "tile_material": MATERIALS + "/" + TILE_MATERIAL}

    post = []
    for item in baseline["components"]:
        component = components[item["name"]]
        digest = _transform_hash(component)
        assert component.get_instance_count() == item["instance_count"], item["name"]
        assert digest == item["transform_hash"], (item["name"], "instance transforms changed")
        assert bool(component.is_visible()) == item["visible"], item["name"]
        assert str(component.get_collision_profile_name()) == item["collision_profile"], item["name"]
        assert bool(component.get_editor_property("cast_shadow")) == item["cast_shadow"], item["name"]
        post.append({"name": item["name"],
                     "materials": [_path(component.get_material(i))
                                   for i in range(component.get_num_materials())],
                     "custom_data_floats": int(component.get_editor_property("num_custom_data_floats"))})
    report["post_state"] = post
    report["instance_total"] = sum(item["instance_count"] for item in baseline["components"])
    report["transform_payload_unchanged"] = True
    report["map_saved"] = False
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    print("REFTUNE4_FIX", REPORT)
    print("REFTUNE4_COUNTS instances", report["instance_total"], "custom", json.dumps(written))
    print("REFTUNE4_MATERIALS", json.dumps(report["steps"]["materials"]))
    print("REFTUNE4_WALL", json.dumps(report["steps"]["custom_data"]["wall_height_source"]))


if __name__ == "__main__":
    main()
