"""Wuxianmen V5 FullPBR reference-tuning revision 2.

The FullPBR assembly is geometrically identical to the Core assembly that the
previous pass corrected: same seven components, same 5,103 instances, and every
component's recorded transform payload matches the Core baseline byte for byte.
So the same geometry corrections and the same pivots apply, and this script
reuses them.

Geometry
  1. Both roof tiers are inverted (tile and wood roof decks rise from z 12.75 m
     at y = 0 to z 15.72 m at |y| = 4.5 m). Corrected with a 180 degree rotation
     about the world X axis through each tier's mid-plane, then the wood decks are
     re-seated below the tiles (a Z mirror reverses vertical stacking).
  2. The ridge role is bound to a roof-tile mesh while main_ridge sits unused, and
     both ridge bars run along Y although the roof pitches along Y. Re-bound,
     turned 90 degrees about Z, and re-seated on the corrected apex.

Materials
  3. The FullPBR source archive ships smooth mottle textures with no
     architectural joints - measured at 18x less edge energy on stone and 4x less
     on the roof than the Core package's authored sheets - so they cannot produce
     ashlar blocks or tile courses at any UV scale. Per the user's decision the
     Core package's authored 4K roof tile sheet is copied into the FullPBR package
     as a task-owned texture and drives a new roof material with the same measured
     UV contract.
  4. Stone keeps the FullPBR base colour and normal but gains a per-instance
     custom data value driving a UV offset and a +-20 percent tone variation, so
     4,213 otherwise identical blocks stop reading as one flat surface.
  5. Plaster tint is brought in line with the Core pass; the plaque tint is
     neutralised so the board reads light like the reference rather than gold.

Never saves a map. Writes a JSON report and asserts every invariant before save.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR"
BLUEPRINT = PACKAGE + "/BP_Wuxianmen_V5_FullPBR"
MATERIALS = PACKAGE + "/Materials"
TEXTURES = PACKAGE + "/Textures"
MESHES = PACKAGE + "/Meshes/ReferenceTuned20260917"

BASELINE = ROOT / "Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json"
REPORT = ROOT / "Wuxianmen_V5_FullPBR-reftune2-fix-20260917.json"
TILE_SHEET = ROOT / "RefTune2/Wuxianmen_V5_FullPBR"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

TILE_COMPONENT = "HISM_002_lower_tile_1_077_GEN_VARIABLE"
RIDGE_COMPONENT = "HISM_004_ridge_000010_GEN_VARIABLE"
PLASTER_COMPONENT = "HISM_003_plaster_000016_GEN_VARIABLE"
STONE_COMPONENT = "HISM_005_stone_004213_GEN_VARIABLE"
WOOD_COMPONENT = "HISM_006_wood_000451_GEN_VARIABLE"
PLAQUE_COMPONENT = "HISM_000_Wuxianmen_Plaque_GEN_VARIABLE"

# Identical to the Core pass; justified by the recorded transform comparison.
TIER_SPLIT_Z = 1350.0
TIER_PIVOT_Z = {"main": 1423.5, "lower": 1251.5}
TIER_APEX_Z = {"main": 1572.0, "lower": 1338.0}
DECK_PIVOT_Z = {"main": 1403.5, "lower": 1236.5}

# Measured from the Core authored sheet now driving the FullPBR roof.
ROOF_COLUMNS_PER_U = 14.629
ROOF_COURSES_PER_AUTHORED_V = 5.242
ROOF_AUTHORED_V = 0.4646
ROOF_COURSES_PER_REPEAT = 5.0
ROOF_REPEATS = 2.8
ROOF_TINT = (0.40, 0.43, 0.47, 1.0)

# The FullPBR stone sheet is smooth mottle; sampling it 1:1 minifies the detail
# below visibility. A small tiling magnifies it to roughly 0.1 m features and the
# per-instance offset then gives every block a different region.
STONE_TILING = 0.06
STONE_TINT = (0.53, 0.50, 0.44, 1.0)
STONE_TONE_BASE = 0.80
STONE_TONE_SPREAD = 0.40

PLASTER_TINT = (0.62, 0.57, 0.49, 1.0)
PLAQUE_TINT = (0.92, 0.90, 0.86, 1.0)

TOLERANCE_CM = 0.25


def _path(value):
    return value.get_path_name() if value else ""


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def _transform_record(value):
    return {
        "location": _vec(value.translation),
        "rotation": _rot(value.rotation.rotator()),
        "scale": _vec(value.scale3d),
    }


def _transform_hash(component):
    payload = [
        _transform_record(component.get_instance_transform(index, False))
        for index in range(component.get_instance_count())
    ]
    return hashlib.sha256(
        json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    ).hexdigest()


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set()
    result = []
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
    """Keyword constructor: probed to round-trip exactly. Do not compose
    axis-angle rotations - Unreal's Roll and Pitch are negated relative to a
    right-handed rotation about +X / +Y and the composition diverges."""
    return unreal.Rotator(pitch=record[0], yaw=record[1], roll=record[2])


def _quat_dot(a, b):
    return abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)


def _matches_baseline(component, expected, tolerance=1e-3):
    if component.get_instance_count() != len(expected):
        return False
    for index, item in enumerate(expected):
        transform = component.get_instance_transform(index, False)
        if any(
            abs(a - b) > tolerance
            for a, b in zip(
                [transform.translation.x, transform.translation.y, transform.translation.z],
                item["location"],
            )
        ):
            return False
        if any(
            abs(a - b) > tolerance
            for a, b in zip(
                [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z], item["scale"]
            )
        ):
            return False
        expected_quat = unreal.MathLibrary.conv_rotator_to_quaternion(
            _rotator_from_record(item["rotation"])
        )
        if _quat_dot(transform.rotation, expected_quat) < 1.0 - 1e-6:
            return False
    return True


def _restore_baseline(component, expected):
    if _matches_baseline(component, expected):
        return 0
    for index, item in enumerate(expected):
        component.update_instance_transform(
            index,
            unreal.Transform(
                unreal.Vector(*item["location"]),
                _rotator_from_record(item["rotation"]),
                unreal.Vector(*item["scale"]),
            ),
            False,
        )
    return len(expected)


def _local_box(mesh):
    bounds = mesh.get_bounds()
    if hasattr(bounds, "min") and hasattr(bounds, "max"):
        return bounds.min, bounds.max
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def _axis_angle(axis, degrees):
    return unreal.MathLibrary.rotator_from_axis_and_angle(unreal.Vector(*axis), degrees)


def _compose(outer, inner):
    """Return the rotation equivalent to applying `inner` first, then `outer`."""
    return unreal.MathLibrary.quat_rotator(
        unreal.MathLibrary.multiply_quat_quat(
            unreal.MathLibrary.conv_rotator_to_quaternion(outer),
            unreal.MathLibrary.conv_rotator_to_quaternion(inner),
        )
    )


def _aabb(transform, local_min, local_max):
    quat = transform.rotation
    scale = transform.scale3d
    corners = []
    for x in (local_min.x, local_max.x):
        for y in (local_min.y, local_max.y):
            for z in (local_min.z, local_max.z):
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    quat, unreal.Vector(x * scale.x, y * scale.y, z * scale.z)
                )
                corners.append(rotated + transform.translation)
    low = [min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)]
    high = [max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)]
    return low, high


def _mirror_z(box, pivot_z):
    low, high = box
    return [low[0], low[1], 2 * pivot_z - high[2]], [high[0], high[1], 2 * pivot_z - low[2]]


def _match(expected, actual, tolerance=TOLERANCE_CM):
    pool = list(expected)
    unmatched = []
    for entry in actual:
        found = None
        for index, candidate in enumerate(pool):
            if all(abs(a - b) <= tolerance for a, b in zip(candidate[0], entry[0])) and all(
                abs(a - b) <= tolerance for a, b in zip(candidate[1], entry[1])
            ):
                found = index
                break
        if found is None:
            unmatched.append(entry)
        else:
            pool.pop(found)
    return unmatched


# --------------------------------------------------------------------------- #
# material helpers
# --------------------------------------------------------------------------- #

def _duplicate(source_path, new_name):
    target = MATERIALS + "/" + new_name
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    source = EAL.load_asset(source_path)
    assert source, source_path
    asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(new_name, MATERIALS, source)
    assert asset, target
    return asset


def _node(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def _sample(material, texture_path, x, y, sampler):
    node = _node(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", EAL.load_asset(texture_path))
    node.set_editor_property("sampler_type", sampler)
    return node


def _constant(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def _constant2(material, u, v, x, y):
    node = _node(material, unreal.MaterialExpressionConstant2Vector, x, y)
    node.set_editor_property("r", u)
    node.set_editor_property("g", v)
    return node


def _constant3(material, value, x, y):
    node = _node(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _binary(material, cls, a, b, x, y, a_output="", b_output=""):
    node = _node(material, cls, x, y)
    assert MEL.connect_material_expressions(a, a_output, node, "A"), (cls, "A")
    assert MEL.connect_material_expressions(b, b_output, node, "B"), (cls, "B")
    return node


def _multiply(material, a, b, x, y, a_output="", b_output=""):
    return _binary(material, unreal.MaterialExpressionMultiply, a, b, x, y, a_output, b_output)


def _add(material, a, b, x, y, a_output="", b_output=""):
    return _binary(material, unreal.MaterialExpressionAdd, a, b, x, y, a_output, b_output)


def _unary(material, cls, source, x, y):
    node = _node(material, cls, x, y)
    assert MEL.connect_material_expressions(source, "", node, "")
    return node


def _connect_uv(uv_node, sampler):
    for pin in ("UVs", "Coordinates"):
        try:
            if MEL.connect_material_expressions(uv_node, "", sampler, pin):
                return pin
        except Exception:
            continue
    raise AssertionError("could not connect UV input on " + sampler.get_name())


def _uv_chain(material, tiling, frac_scale=None, custom_offset=False, x=-1900):
    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, x, 0)
    coordinate.set_editor_property("coordinate_index", 0)
    current = _multiply(material, coordinate, _constant2(material, tiling[0], tiling[1], x, 200), x + 200, 0)
    if custom_offset:
        data = _node(material, unreal.MaterialExpressionPerInstanceCustomData, x + 200, 420)
        data.set_editor_property("data_index", 0)
        current = _add(material, current, data, x + 400, 0)
    current = _unary(material, unreal.MaterialExpressionFrac, current, x + 600, 0)
    if frac_scale is not None:
        current = _multiply(
            material, current, _constant2(material, frac_scale[0], frac_scale[1], x + 600, 200), x + 800, 0
        )
    return current


def _finish(material):
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    assert EAL.save_loaded_asset(material)


def import_tile_sheet():
    tasks = []
    for source, name in (
        ("RoofTile_BaseColor_4K_TileSheet.png", "RoofTile_BaseColor_4K_TileSheet"),
        ("RoofTile_Normal_4K_TileSheet.png", "RoofTile_Normal_4K_TileSheet"),
        ("RoofTile_Roughness_4K_TileSheet.png", "RoofTile_Roughness_4K_TileSheet"),
    ):
        task = unreal.AssetImportTask()
        task.filename = str(TILE_SHEET / source)
        task.destination_path = TEXTURES
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in ("RoofTile_BaseColor_4K_TileSheet", "RoofTile_Normal_4K_TileSheet",
                 "RoofTile_Roughness_4K_TileSheet"):
        asset = EAL.load_asset(TEXTURES + "/" + name)
        assert isinstance(asset, unreal.Texture2D), name
    return {name: TEXTURES + "/" + name for name in (
        "RoofTile_BaseColor_4K_TileSheet", "RoofTile_Normal_4K_TileSheet",
        "RoofTile_Roughness_4K_TileSheet")}


def build_roof_material(tile_sheet):
    material = _duplicate(MATERIALS + "/M_RoofTile_ReferenceTuned", "M_RoofTile_ReferenceTuned_RefTune2")
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    u_tiling = 1.0 / ROOF_COLUMNS_PER_U
    v_scale = ROOF_AUTHORED_V * (ROOF_COURSES_PER_REPEAT / ROOF_COURSES_PER_AUTHORED_V)
    uv = _uv_chain(material, (u_tiling, ROOF_REPEATS), frac_scale=(1.0, v_scale))

    base = _sample(material, tile_sheet["RoofTile_BaseColor_4K_TileSheet"], -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, tile_sheet["RoofTile_Normal_4K_TileSheet"], -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = _sample(material, tile_sheet["RoofTile_Roughness_4K_TileSheet"], -700, 700,
                        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, roughness)]
    rough = _multiply(material, roughness, _constant(material, 1.04, -700, 1000), -420, 700, "R")
    coloured = _multiply(material, base, _constant3(material, ROOF_TINT, -900, 220), -560, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {
        "source": "authored tile sheet copied from the Core package (user decision)",
        "u_tiling": round(u_tiling, 6),
        "v_repeat": ROOF_REPEATS,
        "v_scale": round(v_scale, 6),
        "uv_pins_connected": pins,
        "tint": list(ROOF_TINT),
        "courses_per_strip": ROOF_REPEATS * ROOF_COURSES_PER_REPEAT,
        "course_pitch_m": round(5.0678 / (ROOF_REPEATS * ROOF_COURSES_PER_REPEAT), 4),
    }


def build_stone_material(custom_data_available):
    material = _duplicate(MATERIALS + "/M_Stone_ReferenceTuned", "M_Stone_ReferenceTuned_RefTune2")
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)

    uv = _uv_chain(material, (STONE_TILING, STONE_TILING), custom_offset=custom_data_available)
    base = _sample(material, TEXTURES + "/Stone_BaseColor_4K_Repaired", -700, -180,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, TEXTURES + "/Stone_Normal_4K_Repaired", -700, 260,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    metal_rough = _sample(material, TEXTURES + "/Stone_MetallicRoughness_4K_Repaired", -700, 700,
                          unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    pins = [_connect_uv(uv, sampler) for sampler in (base, normal, metal_rough)]

    # glTF packing: G = roughness, B = metallic.
    rough = _multiply(material, metal_rough, _constant(material, 1.10, -700, 1000), -420, 700, "G")
    assert MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    assert MEL.connect_material_property(metal_rough, "B", unreal.MaterialProperty.MP_METALLIC)

    tone = None
    if custom_data_available:
        data = _node(material, unreal.MaterialExpressionPerInstanceCustomData, -1900, 900)
        data.set_editor_property("data_index", 0)
        varied = _multiply(material, data, _constant(material, STONE_TONE_SPREAD, -1700, 1080), -1500, 900)
        tone = _add(material, varied, _constant(material, STONE_TONE_BASE, -1500, 1240), -1300, 900)

    coloured = _multiply(
        material,
        _multiply(material, base, _constant3(material, STONE_TINT, -900, 220), -700, 80),
        tone if tone is not None else _constant(material, 1.0, -900, 420),
        -540,
        80,
    )
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    _finish(material)
    return material, {
        "tiling": [STONE_TILING, STONE_TILING],
        "tint": list(STONE_TINT),
        "tone_range": [STONE_TONE_BASE, round(STONE_TONE_BASE + STONE_TONE_SPREAD, 3)],
        "per_instance_variation": bool(custom_data_available),
        "uv_pins_connected": pins,
        "metallic_roughness_packing": "G = roughness, B = metallic",
    }


def build_plaster_material():
    material = _duplicate(MATERIALS + "/M_Plaster_ReferenceTuned", "M_Plaster_ReferenceTuned_RefTune2")
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)
    base = _sample(material, TEXTURES + "/Plaster_BaseColor_4K_Repaired", -900, 0,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = _sample(material, TEXTURES + "/Plaster_Normal_4K_Repaired", -900, 380,
                     unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    metal_rough = _sample(material, TEXTURES + "/Plaster_MetallicRoughness_4K_Repaired", -900, 700,
                          unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    coloured = _multiply(material, base, _constant3(material, PLASTER_TINT, -900, 220), -600, 80)
    rough = _multiply(material, metal_rough, _constant(material, 1.10, -900, 1000), -600, 700, "G")
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    assert MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    assert MEL.connect_material_property(metal_rough, "B", unreal.MaterialProperty.MP_METALLIC)
    _finish(material)
    return material, {"tint": list(PLASTER_TINT), "packing": "G = roughness, B = metallic"}


def build_plaque_material():
    material = _duplicate(MATERIALS + "/M_Plaque_Wuxianmen_ReferenceTuned", "M_Plaque_Wuxianmen_RefTune2")
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)
    base = _sample(material, TEXTURES + "/Plaque_Wuxianmen_BaseColor_2K_Repaired", -900, 0,
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    coloured = _multiply(material, base, _constant3(material, PLAQUE_TINT, -900, 220), -600, 80)
    assert MEL.connect_material_property(coloured, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(_constant(material, 0.80, -600, 660), "", unreal.MaterialProperty.MP_ROUGHNESS)
    _finish(material)
    return material, {"tint": list(PLAQUE_TINT), "texture": TEXTURES + "/Plaque_Wuxianmen_BaseColor_2K_Repaired"}


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #

def main():
    dirty_before = [
        _path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    ]
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    baseline_by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = EAL.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = {component.get_name(): component for component in _components(blueprint)}
    assert set(components) == set(baseline_by_name), (sorted(components), sorted(baseline_by_name))

    report = {"created": "2026-09-17", "revision": "reftune2", "blueprint": BLUEPRINT, "steps": {}}
    report["dirty_maps_before"] = dirty_before
    tile = components[TILE_COMPONENT]
    ridge = components[RIDGE_COMPONENT]
    plaster = components[PLASTER_COMPONENT]
    stone = components[STONE_COMPONENT]
    wood = components[WOOD_COMPONENT]
    plaque = components[PLAQUE_COMPONENT]

    restored = {}
    for name in (TILE_COMPONENT, RIDGE_COMPONENT, WOOD_COMPONENT):
        restored[name] = _restore_baseline(components[name], baseline_by_name[name]["instances"])
        assert _matches_baseline(components[name], baseline_by_name[name]["instances"]), name
    report["steps"]["baseline_restore"] = {
        "restored_instances": restored,
        "note": "0 means the component already matched the recorded baseline",
    }

    r180 = _axis_angle((1.0, 0.0, 0.0), 180.0)
    changed = {"main": 0, "lower": 0}
    verification = {}

    def flip(component, indices, tier, label, pivot_z=None):
        local_min, local_max = _local_box(component.static_mesh)
        pivot_z = TIER_PIVOT_Z[tier] if pivot_z is None else pivot_z
        before, after, planned = [], [], []
        for index in indices:
            old = component.get_instance_transform(index, False)
            before.append(_aabb(old, local_min, local_max))
            new = unreal.Transform(
                unreal.Vector(old.translation.x, -old.translation.y, 2.0 * pivot_z - old.translation.z),
                _compose(r180, old.rotation.rotator()),
                old.scale3d,
            )
            planned.append((index, new))
            after.append(_aabb(new, local_min, local_max))
        unmatched = _match([_mirror_z(box, pivot_z) for box in before], after)
        verification[label] = {
            "instances": len(indices),
            "unmatched": len(unmatched),
            "passed": not unmatched,
            "pivot_z": pivot_z,
            "sample_unmatched": [[round(v, 3) for v in item[0]] for item in unmatched[:3]],
        }
        if unmatched:
            return
        for index, new in planned:
            component.update_instance_transform(index, new, False)
            changed[tier] += 1

    flip(tile, [i for i in range(tile.get_instance_count())
                if tile.get_instance_transform(i, False).translation.z >= TIER_SPLIT_Z], "main", "tile:main")
    flip(tile, [i for i in range(tile.get_instance_count())
                if tile.get_instance_transform(i, False).translation.z < TIER_SPLIT_Z], "lower", "tile:lower")

    wood_min, wood_max = _local_box(wood.static_mesh)
    deck_main, deck_lower = [], []
    for index in range(wood.get_instance_count()):
        transform = wood.get_instance_transform(index, False)
        low, high = _aabb(transform, wood_min, wood_max)
        if high[0] - low[0] < 1500.0 or high[2] - low[2] < 100.0:
            continue
        (deck_main if transform.translation.z >= TIER_SPLIT_Z else deck_lower).append(index)
    assert len(deck_main) == 2 and len(deck_lower) == 2, (deck_main, deck_lower)
    flip(wood, deck_main, "main", "wooddeck:main", DECK_PIVOT_Z["main"])
    flip(wood, deck_lower, "lower", "wooddeck:lower", DECK_PIVOT_Z["lower"])

    assert all(item["passed"] for item in verification.values()), verification
    report["steps"]["roof_flip"] = {
        "passed": True,
        "changed_instances": changed,
        "tier_pivot_z": TIER_PIVOT_Z,
        "deck_pivot_z": DECK_PIVOT_Z,
        "method": "180 degree rotation about the world X axis through each tier mid-plane; roof decks use a lower pivot so the mirror does not lift them above the tiles",
        "verification": verification,
        "verification_rule": "every corrected instance box must equal the Z mirror of an original box about the pivot used for that part",
    }

    replacement = EAL.load_asset(MESHES + "/main_ridge.main_ridge")
    assert isinstance(replacement, unreal.StaticMesh), MESHES
    ridge.set_static_mesh(replacement)
    ridge_min, ridge_max = _local_box(replacement)
    big = []
    for index in range(ridge.get_instance_count()):
        low, high = _aabb(ridge.get_instance_transform(index, False), ridge_min, ridge_max)
        if high[1] - low[1] > 1000.0:
            big.append(index)
    assert len(big) == 2, big
    ridge_records = []
    for index in big:
        old = ridge.get_instance_transform(index, False)
        tier = "main" if old.translation.z >= TIER_SPLIT_Z else "lower"
        new = unreal.Transform(
            unreal.Vector(0.0, 0.0, TIER_APEX_Z[tier]),
            _compose(_axis_angle((0.0, 0.0, 1.0), -90.0), old.rotation.rotator()),
            old.scale3d,
        )
        ridge.update_instance_transform(index, new, False)
        low, high = _aabb(new, ridge_min, ridge_max)
        record = {
            "index": index,
            "tier": tier,
            "after": _transform_record(new),
            "x_span_cm": round(high[0] - low[0], 2),
            "y_span_cm": round(high[1] - low[1], 2),
            "z_range_cm": [round(low[2], 2), round(high[2], 2)],
        }
        assert record["x_span_cm"] > 2000.0 and record["y_span_cm"] < 500.0, record
        ridge_records.append(record)
    report["steps"]["ridge"] = {
        "passed": True,
        "replacement_mesh": MESHES + "/main_ridge.main_ridge",
        "reoriented": ridge_records,
    }

    custom_data_available = False
    custom_data_error = None
    prop_used = None
    for prop in ("num_custom_data_floats", "num_custom_data"):
        try:
            stone.set_editor_property(prop, 1)
            if int(stone.get_editor_property(prop)) >= 1:
                custom_data_available = True
                prop_used = prop
                break
        except Exception as error:
            custom_data_error = repr(error)
    report["steps"]["stone_custom_data"] = {
        "available": custom_data_available,
        "property": prop_used,
        "error": None if custom_data_available else custom_data_error,
    }
    if custom_data_available:
        assigned = 0
        for index in range(stone.get_instance_count()):
            value = ((index * 2654435761) % 4294967296) / 4294967296.0
            if stone.set_custom_data_value(index, 0, float(value)):
                assigned += 1
        report["steps"]["stone_custom_data"].update({"assigned": assigned, "data_index": 0})
        custom_data_available = assigned == stone.get_instance_count()

    tile_sheet = import_tile_sheet()
    roof_material, roof_info = build_roof_material(tile_sheet)
    stone_material, stone_info = build_stone_material(custom_data_available)
    plaster_material, plaster_info = build_plaster_material()
    plaque_material, plaque_info = build_plaque_material()

    bindings = {
        PLAQUE_COMPONENT: plaque_material,
        TILE_COMPONENT: roof_material,
        RIDGE_COMPONENT: roof_material,
        PLASTER_COMPONENT: plaster_material,
        STONE_COMPONENT: stone_material,
    }
    for name, material in bindings.items():
        components[name].set_material(0, material)
    report["steps"]["materials"] = {
        "passed": True,
        "roof_tile": roof_info,
        "stone": stone_info,
        "plaster": plaster_info,
        "plaque": plaque_info,
        "bindings": {name: _path(material) for name, material in bindings.items()},
    }

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

    allowed = {TILE_COMPONENT, RIDGE_COMPONENT, WOOD_COMPONENT}
    for name in post:
        if name not in allowed:
            assert post[name]["transform_hash"] == baseline_by_name[name]["transform_hash"], name
    assert post[RIDGE_COMPONENT]["mesh"].endswith("main_ridge.main_ridge")

    report["post_state"] = post
    report["instance_total"] = sum(item["instance_count"] for item in post.values())
    report["map_saved"] = False
    report["dirty_maps_after_apply"] = [
        _path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    ]
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    print("FULLPBR_REFTUNE2_FIX", REPORT)
    print("FULLPBR_REFTUNE2_FIX_COUNTS", report["instance_total"], json.dumps(changed))
    print("FULLPBR_REFTUNE2_FIX_VERIFY", json.dumps(verification))
    print("FULLPBR_REFTUNE2_FIX_RIDGE", json.dumps([{k: r[k] for k in ("index", "tier", "x_span_cm", "y_span_cm", "z_range_cm")} for r in ridge_records]))


if __name__ == "__main__":
    main()
