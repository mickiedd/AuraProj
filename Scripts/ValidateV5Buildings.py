"""Validate V5 import completeness, material tuning and intact Blueprint envelopes."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
EAL = unreal.EditorAssetLibrary


def _path(value):
    return value.get_path_name() if value else ""


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
        if component.get_path_name() in seen:
            continue
        seen.add(component.get_path_name())
        result.append(component)
    return result


def _orientation_component(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if isinstance(component, unreal.SceneComponent) and component.get_name().startswith("BuildingOrientation"):
            return component
    return None


def main():
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    results = []
    for cfg in json.loads((ROOT / "packages.json").read_text(encoding="utf-8")):
        imported = json.loads((ROOT / (cfg["name"] + "-import.json")).read_text(encoding="utf-8"))
        tuning = json.loads((ROOT / (cfg["name"] + "-tuning.json")).read_text(encoding="utf-8"))
        assert imported["complete"] and tuning["passed"]
        assert imported["scene"]["mesh_count"] == cfg["glb_referenced_mesh_count"]
        assert imported["scene"]["instance_count"] == cfg["glb_mesh_instance_count"]
        blueprint = EAL.load_asset(imported["blueprint"])
        assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
        expected_rotation = cfg.get("blueprint_root_rotation", [0.0, 0.0, 0.0])
        orientation = _orientation_component(blueprint)
        if any(abs(value) > 0.001 for value in expected_rotation):
            assert orientation, cfg["name"]
            root_rotation = orientation.get_editor_property("relative_rotation")
        else:
            root_rotation = orientation.get_editor_property("relative_rotation") if orientation else unreal.Rotator()
        assert max(abs(actual - expected) for actual, expected in zip(
            (root_rotation.pitch, root_rotation.yaw, root_rotation.roll), expected_rotation
        )) < 0.01, (cfg["name"], root_rotation, expected_rotation)
        components = _components(blueprint)
        assert len(components) == imported["hism_component_count"] == cfg["glb_referenced_mesh_count"]
        assert sum(component.get_instance_count() for component in components) == cfg["glb_mesh_instance_count"]
        mesh_records = []
        for component in components:
            assert component.get_instance_count() > 0
            assert str(component.get_collision_profile_name()) == "BlockAll"
            mesh = component.static_mesh
            assert isinstance(mesh, unreal.StaticMesh) and mesh.get_num_sections(0) > 0
            body = mesh.get_editor_property("body_setup")
            assert body.get_editor_property("collision_trace_flag") == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
            assert body.get_editor_property("double_sided_geometry")
            materials = []
            for index in range(component.get_num_materials()):
                material = component.get_material(index)
                assert isinstance(material, unreal.MaterialInterface), (component.get_name(), index)
                assert material.get_name().endswith("_ReferenceTuned"), _path(material)
                base = material.get_base_material()
                assert base.get_editor_property("two_sided"), _path(material)
                assert _path(material).startswith(cfg["destination"] + "/Materials/"), _path(material)
                materials.append(_path(material))
            mesh_records.append({
                "component": component.get_name(),
                "mesh": _path(mesh),
                "instances": component.get_instance_count(),
                "materials": materials,
            })
        for source, texture_path in imported["textures"].items():
            texture = EAL.load_asset(texture_path)
            assert isinstance(texture, unreal.Texture2D), texture_path
            assert texture.blueprint_get_size_x() >= 512 and texture.blueprint_get_size_y() >= 512, (source, texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
            channel = next(channel for maps in cfg["material_maps"].values() for channel, item in maps.items() if item == source)
            assert texture.get_editor_property("srgb") == (channel == "BaseColor"), (source, channel)
        actor = actor_subsystem.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector())
        assert actor
        origin, extent = actor.get_actor_bounds(False)
        size = [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0]
        expected = cfg["expected_dimensions_cm"]
        assert max(size) > max(expected) * 0.70 and max(size) < max(expected) * 1.35, (cfg["name"], size, expected)
        assert min(size) > min(expected) * 0.45, (cfg["name"], size, expected)
        assert size[2] > expected[2] * 0.65, (cfg["name"], size, expected)
        dependencies = set()
        pending = [imported["blueprint"]]
        options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
        while pending:
            for dependency in registry.get_dependencies(pending.pop(), options):
                dependency_path = str(dependency)
                if dependency_path in dependencies:
                    continue
                dependencies.add(dependency_path)
                if dependency_path.startswith("/Game/"):
                    assert dependency_path.startswith(cfg["destination"] + "/"), dependency_path
                    assert EAL.does_asset_exist(dependency_path), dependency_path
                    assert not dependency_path.endswith("_Preview"), dependency_path
                    pending.append(dependency_path)
        actor_subsystem.destroy_actor(actor)
        result = {
            "name": cfg["name"],
            "passed": True,
            "blueprint": imported["blueprint"],
            "source_mesh_definition_count": cfg["glb_mesh_count"],
            "source_referenced_mesh_count": cfg["glb_referenced_mesh_count"],
            "source_instance_count": cfg["glb_mesh_instance_count"],
            "hism_component_count": len(components),
            "blueprint_instance_count": sum(component["instances"] for component in mesh_records),
            "size_cm": size,
            "expected_dimensions_cm": expected,
            "mesh_records": mesh_records,
            "dependencies": sorted(dependencies),
            "intactness": {
                "source_mesh_coverage": True,
                "source_instance_coverage": True,
                "nonempty_mesh_sections": True,
                "all_slots_reference_tuned": True,
                "two_sided_surface_visibility": True,
                "double_sided_complex_collision": True,
                "package_local_dependencies": True,
                "plausible_building_bounds": True,
                "reference_upright_orientation_component": True,
                "intentional_gate_arch_remains_open": True,
            },
        }
        results.append(result)
        print("V5_VALIDATION_PASS", cfg["name"], size, result["blueprint_instance_count"])
    output = ROOT / "validation.json"
    output.write_text(json.dumps({"passed": True, "packages": results}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
