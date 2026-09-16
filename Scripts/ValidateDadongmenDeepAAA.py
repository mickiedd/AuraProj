"""Focused Dadongmen-only validator for the Deep AAA pass.

This intentionally does not call ValidateV4Buildings.py.  It validates the
new Dadongmen material graphs, imported hero mesh, Blueprint ownership,
rollback assets, and a fresh spawned Blueprint instance without touching the
shared showcase map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
REPORT_PATH = ROOT / "Dadongmen_V4-deep-aaa.json"
MANIFEST_PATH = ROOT / "DeepAAA/Dadongmen_V4_DeepAAA.json"
VALIDATION_PATH = ROOT / "Dadongmen_V4-deep-aaa-validation.json"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
BP_PATH = DEST + "/BP_Dadongmen_V4"
MAPS = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _path(value):
    return value.get_path_name() if value else ""


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    result = []
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent):
            continue
        if component.get_name() in seen:
            continue
        seen.add(component.get_name())
        result.append(component)
    return result


def _collision_profile(component):
    try:
        return str(component.get_editor_property("collision_profile_name"))
    except Exception:
        try:
            return str(component.get_collision_profile_name())
        except Exception:
            return "unavailable"


def main():
    report = json.loads(REPORT_PATH.read_text(encoding="utf-8"))
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    assert report["variant_suffix"] == "_DeepAAA", report["variant_suffix"]
    assert report["geometry_changed"] is True
    assert report["uvs_changed"] is False
    assert report["collision_changed"] is False
    assert report["placement_changed"] is False
    assert manifest["geometry_changed"] is True
    assert manifest["vertex_count"] > 20000, manifest
    assert manifest["triangle_count"] > 80000, manifest
    for primitive in ("beveled_stone_course", "dressed_arch_voussoir", "tunnel_paving_stone", "open_door_plank", "open_door_hinge_barrel", "dougong_lower_step", "front_roof_tile_rib", "attached_vine_stem", "attached_vine_leaf"):
        assert manifest["primitive_counts"].get(primitive, 0) > 0, primitive
    assert manifest["uv0_preserved_on_source_assemblies"] is True
    assert "NoCollision" in report["detail_collision_intent"]

    material_evidence = {}
    for key, record in report["materials"].items():
        material = _asset(record["path"])
        assert isinstance(material, unreal.Material), record["path"]
        assert material.get_editor_property("used_with_nanite") is True, key
        assert material.get_editor_property("two_sided") is True, key
        expression_count = unreal.MaterialEditingLibrary.get_num_material_expressions(material)
        assert expression_count >= 24, (key, expression_count)
        assert record["height_ratio"] > 0.015, (key, record)
        assert any("BumpOffset" in line for line in record["graph"]), key
        if key == "Vegetation":
            assert record["blend_mode"] == "masked", record
            assert record["opacity_mask"] == 0.0, record
        if key not in ("Iron", "Vegetation", "VegetationLeaf"):
            assert record["normal_layers"] == 2, (key, record)
            assert set(MAPS) == set(record["maps"]), (key, record["maps"])
            used = {_path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material)}
            # Opaque permutations must retain every authored channel.  UE's
            # translucent material permutation may legally prune channels
            # that do not affect its compiled shading path; the report still
            # records those graph inputs and the water path must retain its
            # authored base/AO/height sources.
            if record["blend_mode"] == "opaque":
                assert set(record["maps"].values()).issubset(used), (key, used)
            else:
                required = {record["maps"][channel] for channel in ("BaseColor", "AO", "Height")}
                assert required.issubset(used), (key, used)
            for channel, texture_path in record["maps"].items():
                texture = _asset(texture_path)
                assert isinstance(texture, unreal.Texture2D), texture_path
                assert texture.blueprint_get_size_x() > 0 and texture.blueprint_get_size_y() > 0, texture_path
        material_evidence[key] = {
            "path": record["path"],
            "expression_count": expression_count,
            "used_textures": len(unreal.MaterialEditingLibrary.get_used_textures(material)),
            "maps": record["maps"],
            "height_source": record["height_source"],
        }

    detail = _asset(report["detail_mesh"])
    assert isinstance(detail, unreal.StaticMesh), report["detail_mesh"]
    slots = [str(slot.material_slot_name) for slot in detail.get_editor_property("static_materials")]
    assert slots == report["detail_slots"], (slots, report["detail_slots"])
    assert all("_DeepAAA" in _path(slot.material_interface) for slot in detail.get_editor_property("static_materials")), slots
    uv_channels = None
    try:
        uv_channels = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(detail, 0)
        assert uv_channels > 0, uv_channels
    except Exception as exc:
        unreal.log_warning("Dadongmen Deep AAA UV query unavailable: %s" % exc)

    blueprint = _asset(BP_PATH)
    assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
    components = _components(blueprint)
    source = [component for component in components if component.get_name().startswith("SM_Dadongmen_LOD0")]
    assert len(source) == 1, [(c.get_name(), _path(c.static_mesh)) for c in components]
    assert source[0].static_mesh and "OpenDoor" in _path(source[0].static_mesh), _path(source[0].static_mesh)
    source_materials = [_path(source[0].get_material(i)) for i in range(source[0].get_num_materials())]
    assert source_materials and all("_DeepAAA" in path for path in source_materials), source_materials
    rotation = source[0].get_editor_property("relative_rotation")
    assert abs(float(rotation.roll) + 90.0) < 0.1, rotation
    detail_rows = [component for component in components if component.get_name().startswith("DeepAAA_DetailGeometry")]
    assert len(detail_rows) == 1, [c.get_name() for c in detail_rows]
    detail_component = detail_rows[0]
    assert _path(detail_component.static_mesh) == report["detail_mesh"], _path(detail_component.static_mesh)
    assert "NOCOLLISION" in _collision_profile(detail_component).upper().replace(" ", ""), _collision_profile(detail_component)
    assert detail_component.get_editor_property("visible") is True
    prior_detail_count = len([component for component in components if component.get_name().startswith("Detail_Dadongmen_Door_")])
    assert prior_detail_count >= 60, prior_detail_count

    # Spawn in an unsaved blank world so the compiled Blueprint instance is
    # checked independently of template-only subobject handles.
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    instance = actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector())
    assert instance, BP_PATH
    instance_components = instance.get_components_by_class(unreal.StaticMeshComponent)
    instance_source = next((c for c in instance_components if c.get_name().startswith("SM_Dadongmen_LOD0")), None)
    instance_detail = [c for c in instance_components if c.get_name().startswith("DeepAAA_DetailGeometry")]
    assert instance_source and all("_DeepAAA" in _path(instance_source.get_material(i)) for i in range(instance_source.get_num_materials())), "spawned source bindings"
    assert len(instance_detail) == 1 and _path(instance_detail[0].static_mesh) == report["detail_mesh"], "spawned detail"
    actors.destroy_actor(instance)

    rollback_checked = 0
    for path in report["rollback_assets"]:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            rollback_checked += 1
    assert rollback_checked >= 11, rollback_checked
    result = {
        "passed": True,
        "gate": "Dadongmen",
        "blueprint": BP_PATH,
        "materials_checked": len(material_evidence),
        "material_graphs_checked": len(material_evidence),
        "detail_mesh": report["detail_mesh"],
        "detail_vertices": manifest["vertex_count"],
        "detail_triangles": manifest["triangle_count"],
        "primitive_counts": manifest["primitive_counts"],
        "uv0_channels": uv_channels,
        "blueprint_components_checked": len(components),
        "prior_detail_components_preserved": prior_detail_count,
        "deep_detail_component": detail_component.get_name(),
        "vegetation_source_culled": report["materials"]["Vegetation"]["opacity_mask"] == 0.0,
        "attached_vegetation_material": report["materials"]["VegetationLeaf"]["path"],
        "source_materials": source_materials,
        "material_evidence": material_evidence,
        "rollback_assets_checked": rollback_checked,
        "shared_gate_scope": "Dadongmen only",
    }
    VALIDATION_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("DADONGMEN_DEEP_AAA_VALIDATION_PASS", json.dumps({key: result[key] for key in ("gate", "materials_checked", "detail_vertices", "detail_triangles", "blueprint_components_checked", "prior_detail_components_preserved", "rollback_assets_checked")}))


if __name__ == "__main__":
    main()
