"""Fix Wuxianmen V5 Core roof-map tiling without changing authored geometry."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
MATERIAL_FOLDER = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/Materials"
SOURCE_MATERIAL = MATERIAL_FOLDER + "/M_RoofTile_ReferenceTuned"
FIXED_MATERIAL = MATERIAL_FOLDER + "/M_RoofTile_ReferenceTuned_RoofTilingFix"
BASELINE = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-baseline-20260917.json"
REPORT = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-fix-20260917.json"

TINT = (0.40, 0.43, 0.47, 1.0)
ROUGHNESS_SCALE = 1.04


def _path(value):
    return value.get_path_name() if value else ""


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def _transform(value):
    return {
        "location": _vec(value.translation),
        "rotation": _rot(value.rotation.rotator()),
        "scale": _vec(value.scale3d),
    }


def _transform_hash(component):
    payload = [_transform(component.get_instance_transform(index, False)) for index in range(component.get_instance_count())]
    return hashlib.sha256(json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")).hexdigest()


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


def _expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(source, source_output, target, target_input)


def _connect_property(source, source_output, property_name):
    assert unreal.MaterialEditingLibrary.connect_material_property(source, source_output, property_name)


def _coord(material):
    # Roof_BaseColor_4K is a 4096x4096 container whose authored tile sheet is
    # the upper 2048 rows.  Keep the original UVs and crop only at the graph
    # boundary so base/normal/roughness stay on one physical tile scale.
    node = _expr(material, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    node.set_editor_property("coordinate_index", 0)
    node.set_editor_property("u_tiling", 1.0)
    node.set_editor_property("v_tiling", 0.5)
    return node


def _sample(material, texture, uv, sampler, x, y):
    node = _expr(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    _connect(uv, "", node, "UVs")
    return node


def _constant3(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(*value))
    return node


def _scalar(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", float(value))
    return node


def _multiply(material, left, right, x, y, left_output=""):
    node = _expr(material, unreal.MaterialExpressionMultiply, x, y)
    _connect(left, left_output, node, "A")
    _connect(right, "", node, "B")
    return node


def _find_texture(textures, token):
    matches = [texture for texture in textures if token.lower() in texture.get_name().lower()]
    assert len(matches) == 1, (token, [_path(item) for item in textures])
    return matches[0]


def _component_state(component):
    mesh = component.static_mesh
    body = mesh.get_editor_property("body_setup")
    return {
        "name": component.get_name(),
        "mesh": _path(mesh),
        "instance_count": component.get_instance_count(),
        "transform_hash": _transform_hash(component),
        "relative_transform": {
            "location": _vec(component.get_editor_property("relative_location")),
            "rotation": _rot(component.get_editor_property("relative_rotation")),
            "scale": _vec(component.get_editor_property("relative_scale3d")),
        },
        "materials": [_path(component.get_material(index)) for index in range(component.get_num_materials())],
        "visible": bool(component.is_visible()),
        "collision_profile": str(component.get_collision_profile_name()),
        "cast_shadow": bool(component.get_editor_property("cast_shadow")),
        "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
        "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
    }


def _build_material():
    source = unreal.EditorAssetLibrary.load_asset(SOURCE_MATERIAL)
    assert isinstance(source, unreal.Material), SOURCE_MATERIAL
    material = unreal.EditorAssetLibrary.load_asset(FIXED_MATERIAL) if unreal.EditorAssetLibrary.does_asset_exist(FIXED_MATERIAL) else None
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            "M_RoofTile_ReferenceTuned_RoofTilingFix", MATERIAL_FOLDER, source
        )
    assert isinstance(material, unreal.Material), FIXED_MATERIAL
    textures = list(unreal.MaterialEditingLibrary.get_used_textures(source))
    base = _find_texture(textures, "Roof_BaseColor")
    normal = _find_texture(textures, "Roof_Normal")
    roughness = _find_texture(textures, "Roof_Roughness")
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)
    uv = _coord(material)
    base_sample = _sample(material, base, uv, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -850, 0)
    base_tinted = _multiply(material, base_sample, _constant3(material, TINT, -850, 180), -500, 80, "RGB")
    _connect_property(base_tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal_sample = _sample(material, normal, uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -850, 390)
    _connect_property(normal_sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness_sample = _sample(material, roughness, uv, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -850, 650)
    roughness = _multiply(material, roughness_sample, _scalar(material, ROUGHNESS_SCALE, -850, 820), -500, 650, "R")
    _connect_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material, {"base_color": _path(base), "normal": _path(normal), "roughness": _path(roughness_sample.get_editor_property("texture"))}


def main():
    assert BASELINE.is_file(), BASELINE
    assert not REPORT.exists(), "Refusing a second roof-tiling apply: " + str(REPORT)
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = _components(blueprint)
    assert len(components) == baseline["component_count"] == 7
    before = {component.get_name(): _component_state(component) for component in components}
    fixed_material, texture_paths = _build_material()
    changed = []
    for component in components:
        role = "roof_tile" if "lower_tile" in component.get_name().lower() else "ridge" if "ridge" in component.get_name().lower() else None
        if role is None:
            continue
        mesh = component.static_mesh
        for index in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(index, fixed_material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        for index in range(component.get_num_materials()):
            component.set_material(index, fixed_material)
        changed.append(component.get_name())
    assert changed == ["HISM_002_lower_tile_1_077_GEN_VARIABLE", "HISM_004_ridge_000010_GEN_VARIABLE"], changed
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    after = {component.get_name(): _component_state(component) for component in components}
    for name, state in before.items():
        result = after[name]
        for key in ("mesh", "instance_count", "transform_hash", "relative_transform", "visible", "collision_profile", "cast_shadow", "collision_trace_flag", "double_sided_geometry"):
            assert result[key] == state[key], (name, key, state[key], result[key])
        if name in changed:
            assert all(path == FIXED_MATERIAL + ".M_RoofTile_ReferenceTuned_RoofTilingFix" for path in result["materials"])
        else:
            assert result["materials"] == state["materials"], (name, state["materials"], result["materials"])
    dirty_maps = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    report = {
        "passed": True,
        "created": "2026-09-17",
        "blueprint": BLUEPRINT,
        "baseline": str(BASELINE),
        "dirty_maps_recorded_after_apply": dirty_maps,
        "map_saved": False,
        "changed_components": changed,
        "changed_material": FIXED_MATERIAL,
        "source_material_preserved": SOURCE_MATERIAL,
        "uv_contract": {"coordinate_index": 0, "u_tiling": 1.0, "v_tiling": 0.5, "u_offset": 0.0, "v_offset": 0.0, "sampled_v_range": [0.0, 0.5], "reason": "Roof_BaseColor_4K upper 2048 rows contain the authored tile sheet; lower 2048 rows are black padding."},
        "textures": texture_paths,
        "before": before,
        "after": after,
        "geometry_changed": False,
        "instance_transform_payload_preserved": True,
        "intent": "Restore repeated gray roof-tile appearance by removing black padded rows from the roof material sample while preserving authored Wuxianmen Core geometry and instance transforms.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("V5_WUXIANMEN_ROOF_TILING_FIX_PASS", json.dumps({"blueprint": BLUEPRINT, "components": changed, "material": FIXED_MATERIAL, "report": str(REPORT)}))


if __name__ == "__main__":
    main()
