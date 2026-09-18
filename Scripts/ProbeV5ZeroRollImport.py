"""Import corrected Wuxianmen geometry with the measured zero-roll basis as a probe.

This is a temporary scene probe. It does not touch any Blueprint and leaves the
probe actors for the companion capture/cleanup step.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
VARIANT_ROOT = PROJECT / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
PACKAGES = [
    {"name": "Wuxianmen_V5_4K_Core", "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core", "source": VARIANT_ROOT / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb", "tuning": ROOT / "Wuxianmen_V5_4K_Core-tuning.json"},
    {"name": "Wuxianmen_V5_FullPBR", "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR", "source": VARIANT_ROOT / "Wuxianmen_V5_FullPBR/Wuxianmen_V5_50M_Instanced_WindingOutward.glb", "tuning": ROOT / "Wuxianmen_V5_FullPBR-tuning.json"},
]


def _path(value):
    return value.get_path_name() if value else ""


def _all():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _import(package):
    destination = package["destination"] + "/Meshes/ReferenceTuned20260917ZeroRollProbe"
    actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = [actor for actor in _all() if actor.get_component_by_class(unreal.StaticMeshComponent) and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(destination + "/")]
    if not actors:
        before = {_path(actor) for actor in _all()}
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.import_offset_rotation = unreal.Rotator(roll=0.0)
        pipeline.common_meshes_properties.bake_meshes = False
        pipeline.common_meshes_properties.bake_pivot_meshes = False
        pipeline.mesh_pipeline.combine_static_meshes = False
        pipeline.mesh_pipeline.build_nanite = True
        pipeline.mesh_pipeline.set_editor_property("collision", False)
        pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
        pipeline.mesh_pipeline.generate_lightmap_u_vs = False
        pipeline.material_pipeline.import_materials = False
        pipeline.material_pipeline.texture_pipeline.import_textures = False
        scene_pipeline = unreal.InterchangeGenericLevelPipeline()
        scene_pipeline.scene_hierarchy_type = unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
        parameters = unreal.ImportAssetParameters()
        parameters.is_automated = True
        parameters.replace_existing = False
        parameters.import_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
        parameters.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name()), unreal.SoftObjectPath(scene_pipeline.get_path_name())]
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        assert manager.import_scene(destination, manager.create_source_data(str(package["source"])), parameters)
        actors = [actor for actor in _all() if _path(actor) not in before]
    meshes = {}
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component and component.static_mesh:
            meshes[_path(component.static_mesh)] = component.static_mesh
    assert len(meshes) == 7, (package["name"], len(meshes))
    tuning = json.loads(package["tuning"].read_text(encoding="utf-8"))["tuned_materials"]
    role_materials = {"stone": tuning["M_Stone"], "wood": tuning["M_Wood"], "plaster": tuning["M_Plaster"], "iron": tuning["M_Iron"], "tile": tuning["M_RoofTile"], "ridge": tuning["M_RoofTile"], "plaque": tuning["M_Plaque_Wuxianmen"]}
    mesh_records = []
    for mesh_path, mesh in sorted(meshes.items()):
        slots = mesh.get_editor_property("static_materials")
        slot_names = [str(slot.material_slot_name).lower() for slot in slots]
        role = next((role for role, token in (("stone", "stone"), ("wood", "wood"), ("plaster", "plaster"), ("iron", "iron"), ("tile", "rooftile"), ("plaque", "plaque_wuxianmen")) if any(token in slot for slot in slot_names)), None)
        assert role, (package["name"], mesh_path, slot_names)
        tuned = unreal.EditorAssetLibrary.load_asset(role_materials[role])
        for index in range(len(slots)):
            mesh.set_material(index, tuned)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("double_sided_geometry", True)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        mesh_records.append({"mesh": mesh_path, "role": role, "material": _path(tuned)})
    bounds = []
    transforms = []
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component or not component.static_mesh:
            continue
        origin, extent = actor.get_actor_bounds(False)
        bounds.append((origin, extent))
        tr = actor.get_actor_transform()
        transforms.append({"mesh": _path(component.static_mesh), "location": [tr.translation.x, tr.translation.y, tr.translation.z], "rotation": [tr.rotation.rotator().pitch, tr.rotation.rotator().yaw, tr.rotation.rotator().roll], "scale": [tr.scale3d.x, tr.scale3d.y, tr.scale3d.z]})
    coords = lambda value: (value.x, value.y, value.z)
    minv = [min(coords(origin)[i] - coords(extent)[i] for origin, extent in bounds) for i in range(3)]
    maxv = [max(coords(origin)[i] + coords(extent)[i] for origin, extent in bounds) for i in range(3)]
    report = {"name": package["name"], "destination": destination, "actor_count": len(actors), "mesh_count": len(mesh_records), "mesh_records": mesh_records, "bounds_min": minv, "bounds_max": maxv, "bounds_size": [maxv[i] - minv[i] for i in range(3)], "import_offset_roll": 0.0, "blueprint_touched": False, "actors_left_for_capture": True, "transforms": transforms}
    (ROOT / (package["name"] + "-zero-roll-probe.json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("V5_ZERO_ROLL_PROBE", package["name"], len(actors), report["bounds_size"])


def main():
    for package in PACKAGES:
        _import(package)


if __name__ == "__main__":
    main()
