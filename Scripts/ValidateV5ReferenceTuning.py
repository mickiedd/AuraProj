"""Post-bind live validation for the V5 reference-tuning revision."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
PACKAGES = [
    {"name": "Guidemen_V5_4K", "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K", "mesh_root": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/Meshes/UprightSource", "expected": [5600.0, 1800.0, 1930.0]},
    {"name": "Wuxianmen_V5_4K_Core", "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core", "mesh_root": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/Meshes/ReferenceTuned20260917", "expected": [3600.0, 2400.0, 1700.0]},
    {"name": "Wuxianmen_V5_FullPBR", "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR", "mesh_root": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/Meshes/ReferenceTuned20260917", "expected": [3600.0, 2400.0, 1700.0]},
]
EAL = unreal.EditorAssetLibrary


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


def _nanite(mesh):
    try:
        settings = mesh.get_editor_property("nanite_settings")
        return {"fallback_percent_triangles": float(settings.get_editor_property("fallback_percent_triangles")), "fallback_relative_error": float(settings.get_editor_property("fallback_relative_error"))}
    except Exception as exc:
        return {"unavailable": str(exc)}


def main():
    dirty_maps = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    baseline = json.loads((ROOT / "reference-tuning-baseline-20260917.json").read_text(encoding="utf-8"))
    results = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for cfg in PACKAGES:
        blueprint = EAL.load_asset(cfg["blueprint"])
        assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class(), cfg["blueprint"]
        baseline_pkg = next(item for item in baseline["packages"] if item["name"] == cfg["name"])
        components = _components(blueprint)
        assert len(components) == baseline_pkg["component_count"]
        expected_by_name = {item["name"]: item for item in baseline_pkg["components"]}
        mesh_records = []
        for component in components:
            expected = expected_by_name[component.get_name()]
            assert component.get_instance_count() == expected["instance_count"]
            assert _transform_hash(component) == hashlib.sha256(json.dumps(expected["instance_transforms_local"], separators=(",", ":"), sort_keys=True).encode("utf-8")).hexdigest()
            assert _path(component.static_mesh).startswith(cfg["mesh_root"] + "/"), _path(component.static_mesh)
            assert str(component.get_collision_profile_name()) == expected["collision_profile"]
            assert bool(component.is_visible()) == expected["visible"]
            assert bool(component.get_editor_property("cast_shadow")) == expected["cast_shadow"]
            for index in range(component.get_num_materials()):
                material = component.get_material(index)
                assert material and (material.get_name().endswith("_ReferenceTuned") or material.get_name().endswith("_RoofTilingFix"))
                assert _path(material).startswith(cfg["blueprint"].rsplit("/", 1)[0] + "/Materials/")
                assert material.get_base_material().get_editor_property("two_sided")
            mesh = component.static_mesh
            body = mesh.get_editor_property("body_setup")
            assert body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
            assert bool(body.get_editor_property("double_sided_geometry"))
            mesh_records.append({"component": component.get_name(), "mesh": _path(mesh), "instances": component.get_instance_count(), "nanite": _nanite(mesh)})
        actor = actor_subsystem.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(100000.0, 100000.0, 0.0))
        assert actor
        _, extent = actor.get_actor_bounds(False)
        size = [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0]
        assert max(size) > max(cfg["expected"]) * 0.70 and max(size) < max(cfg["expected"]) * 1.35
        actor_subsystem.destroy_actor(actor)
        options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
        pending = [cfg["blueprint"]]
        dependencies = set()
        while pending:
            for dependency in registry.get_dependencies(pending.pop(), options):
                dep = str(dependency)
                if dep in dependencies:
                    continue
                dependencies.add(dep)
                if dep.startswith("/Game/"):
                    assert dep.startswith(cfg["blueprint"].rsplit("/", 1)[0] + "/"), (cfg["name"], dep)
                    pending.append(dep)
        results.append({"name": cfg["name"], "blueprint": cfg["blueprint"], "component_count": len(components), "instance_count": sum(item["instances"] for item in mesh_records), "size_cm": size, "mesh_records": mesh_records, "transform_payload_preserved": True, "package_local_dependencies": True, "intentional_arch_opening": True})
        print("V5_REFERENCE_TUNING_PASS", cfg["name"], size, sum(item["instances"] for item in mesh_records))
    output = ROOT / "reference-tuning-validation-20260917.json"
    output.write_text(json.dumps({"passed": True, "dirty_maps_recorded": dirty_maps, "packages": results}, indent=2), encoding="utf-8")
    print("V5_REFERENCE_TUNING_VALIDATION_WRITTEN", output)


if __name__ == "__main__":
    main()
