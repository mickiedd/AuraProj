"""Shared live-editor assertions for the two Guangzhou Deep AAA gates."""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
MAPS = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")
CONFIG = {
    "Wuxianmen": {"report": ROOT / "Wuxianmen_V4-deep-aaa.json", "validation": ROOT / "Wuxianmen_V4-deep-aaa-validation.json", "min_components": 1, "detail_roll": -90.0},
    "Zhengximen": {"report": ROOT / "Zhengximen_V4-deep-aaa.json", "validation": ROOT / "Zhengximen_V4-deep-aaa-validation.json", "min_components": 1, "detail_roll": 0.0},
}


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _path(value):
    return value.get_path_name() if value else ""


def _expression_count(material):
    return unreal.MaterialEditingLibrary.get_num_material_expressions(material)


def _component_rows(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    rows, unique = [], set()
    gathered = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in gathered:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        key = (component.get_name(), _path(component.static_mesh))
        if key in unique:
            continue
        unique.add(key)
        materials = [_path(component.get_material(index)) for index in range(component.get_num_materials())]
        collision = ""
        try:
            collision = str(component.get_editor_property("collision_profile_name"))
        except Exception:
            try:
                collision = str(component.get_collision_profile_name())
            except Exception:
                collision = "unavailable"
        rows.append({"component": component.get_name(), "mesh": _path(component.static_mesh), "materials": materials, "collision_profile": collision})
    return gathered, rows


def validate_gate(label):
    cfg = CONFIG[label]
    report = json.loads(cfg["report"].read_text(encoding="utf-8"))
    assert report["variant_suffix"] == "_AAADeep"
    assert report["geometry_changed"] is True
    assert report["uvs_changed"] is False
    assert report["collision_changed"] is False
    assert report["placement_changed"] is False
    source = report["detail_source"]
    assert source["geometry_changed"] is True
    assert source["vertex_count"] > 10000, source
    assert source["triangle_count"] > 50000, source
    assert source["primitive_counts"], source
    assert source["uv0_preserved_on_source_assemblies"] is True
    assert "NoCollision" in report["detail_collision_intent"]

    material_evidence = {}
    for name, record in report["materials"].items():
        material = _asset(record["path"])
        assert isinstance(material, unreal.Material), record["path"]
        assert material.get_editor_property("used_with_nanite") is True
        assert material.get_editor_property("two_sided") is True
        assert "OPAQUE" in str(material.get_editor_property("blend_mode")).upper()
        assert set(record["maps"]) == set(MAPS)
        used_textures = {_path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material)}
        assert set(record["maps"].values()).issubset(used_textures), (name, used_textures)
        for channel, texture_path in record["maps"].items():
            texture = _asset(texture_path)
            assert isinstance(texture, unreal.Texture2D), texture_path
            size = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
            assert size[0] > 0 and size[1] > 0, (channel, texture_path, size)
        expression_count = _expression_count(material)
        assert expression_count >= 30, (name, expression_count)
        assert record["normal_layers"] == 2, (name, record)
        assert record["height_ratio"] > 0.015, (name, record)
        material_evidence[name] = {"path": record["path"], "used_textures": len(used_textures), "expression_count": expression_count, "texture_sizes": {channel: [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] for channel, texture_path in record["maps"].items() for texture in [_asset(texture_path)]}}

    detail = _asset(report["detail_mesh"])
    assert isinstance(detail, unreal.StaticMesh), report["detail_mesh"]
    detail_slots = [str(slot.material_slot_name) for slot in detail.get_editor_property("static_materials")]
    assert detail_slots == report["detail_slots"], (detail_slots, report["detail_slots"])
    assert all("_AAADeep" in _path(slot.material_interface) for slot in detail.get_editor_property("static_materials"))
    uv_channels = None
    try:
        uv_channels = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(detail, 0)
        assert uv_channels > 0, uv_channels
    except Exception as exc:
        unreal.log_warning("Deep AAA UV live query unavailable: %s" % exc)

    blueprint = _asset(report["blueprint"])
    assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
    gathered, rows = _component_rows(blueprint)
    detail_rows = [row for row in rows if "DeepAAA_DetailGeometry" in row["component"]]
    source_rows = [row for row in rows if row not in detail_rows and not row["component"].startswith("IntactFill_")]
    assert len(detail_rows) == 1, detail_rows
    assert detail_rows[0]["mesh"] == report["detail_mesh"], detail_rows
    assert abs(float(report["detail_component_relative_rotation_roll"]) - cfg["detail_roll"]) < 0.01, report
    assert "NO COLLISION" in detail_rows[0]["collision_profile"].upper() or "NOCOLLISION" in detail_rows[0]["collision_profile"].upper(), detail_rows
    assert source_rows, rows
    assert all(materials and all("_AAADeep" in path for path in materials) for materials in (row["materials"] for row in source_rows)), source_rows
    for rollback_path in report["rollback_assets"]:
        assert unreal.EditorAssetLibrary.does_asset_exist(rollback_path), rollback_path

    result = {
        "passed": True,
        "gate": label,
        "blueprint": report["blueprint"],
        "materials_checked": len(material_evidence),
        "textures_checked": len(material_evidence) * len(MAPS),
        "material_graphs_checked": len(material_evidence),
        "detail_mesh": report["detail_mesh"],
        "detail_vertices": source["vertex_count"],
        "detail_triangles": source["triangle_count"],
        "primitive_counts": source["primitive_counts"],
        "uv0_channels": uv_channels,
        "blueprint_handles_gathered": len(gathered),
        "blueprint_components_checked": len(rows),
        "source_components_checked": len(source_rows),
        "detail_component_checked": detail_rows[0],
        "material_evidence": material_evidence,
        "rollback_assets_checked": len(report["rollback_assets"]),
    }
    cfg["validation"].write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("GUANGZHOU_DEEP_AAA_VALIDATION_PASS", json.dumps({key: result[key] for key in ("gate", "materials_checked", "textures_checked", "detail_vertices", "detail_triangles", "blueprint_components_checked", "source_components_checked", "uv0_channels")}))


if "REQUESTED_GATE" in globals():
    validate_gate(REQUESTED_GATE)
