"""Validate the Wuxianmen V5 Core roof-tiling fix in a live editor."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
FIXED_MATERIAL = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/Materials/M_RoofTile_ReferenceTuned_RoofTilingFix"
BASELINE = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-baseline-20260917.json"
REPORT = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-fix-20260917.json"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-validation-20260917.json"


def _path(value):
    return value.get_path_name() if value else ""


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def _transform(value):
    return {"location": _vec(value.translation), "rotation": _rot(value.rotation.rotator()), "scale": _vec(value.scale3d)}


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
        if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent) and _path(component) not in seen:
            seen.add(_path(component))
            result.append(component)
    return result


def main():
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    fix = json.loads(REPORT.read_text(encoding="utf-8"))
    assert fix["passed"] and fix["geometry_changed"] is False and fix["instance_transform_payload_preserved"] is True
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class(), BLUEPRINT
    components = _components(blueprint)
    assert len(components) == 7
    expected = {item["name"]: item for item in baseline["components"]}
    active = {}
    for component in components:
        state = {
            "mesh": _path(component.static_mesh),
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "relative_transform": {"location": _vec(component.get_editor_property("relative_location")), "rotation": _rot(component.get_editor_property("relative_rotation")), "scale": _vec(component.get_editor_property("relative_scale3d"))},
            "materials": [_path(component.get_material(index)) for index in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
        }
        old = expected[component.get_name()]
        assert state["instance_count"] == old["instance_count"]
        assert state["transform_hash"] == old["transform_hash"]
        assert state["relative_transform"] == old["relative_transform"]
        assert state["visible"] == old["visible"] and state["collision_profile"] == old["collision_profile"] and state["cast_shadow"] == old["cast_shadow"]
        role = "roof_tile" if "lower_tile" in component.get_name().lower() else "ridge" if "ridge" in component.get_name().lower() else "other"
        if role in ("roof_tile", "ridge"):
            assert all(path == FIXED_MATERIAL + ".M_RoofTile_ReferenceTuned_RoofTilingFix" for path in state["materials"]), (component.get_name(), state["materials"])
        else:
            assert state["materials"] == old["materials"], (component.get_name(), old["materials"], state["materials"])
        active[component.get_name()] = dict(state, role=role)
    material = unreal.EditorAssetLibrary.load_asset(FIXED_MATERIAL)
    assert isinstance(material, unreal.Material), FIXED_MATERIAL
    textures = sorted(_path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material))
    assert any("Roof_BaseColor" in path for path in textures)
    assert any("Roof_Normal" in path for path in textures)
    assert any("Roof_Roughness" in path for path in textures)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(100000.0, 100000.0, 0.0))
    assert actor
    origin, extent = actor.get_actor_bounds(False)
    size = [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0]
    actors.destroy_actor(actor)
    dirty_maps = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    output = {"passed": True, "blueprint": BLUEPRINT, "material": FIXED_MATERIAL, "component_count": len(components), "instance_count": sum(item["instance_count"] for item in active.values()), "size_cm": size, "textures": textures, "uv_contract": fix["uv_contract"], "components": active, "temporary_actor_destroyed": True, "dirty_maps_recorded": dirty_maps, "map_saved": False}
    OUTPUT.write_text(json.dumps(output, indent=2), encoding="utf-8")
    print("V5_WUXIANMEN_ROOF_TILING_VALIDATION_PASS", size, output["instance_count"])


if __name__ == "__main__":
    main()
