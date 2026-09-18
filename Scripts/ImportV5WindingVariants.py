"""Import private outward-winding Wuxianmen meshes without touching Blueprints."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
VARIANT_ROOT = PROJECT / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
PACKAGES = [
    {
        "name": "Wuxianmen_V5_4K_Core",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core",
        "source": VARIANT_ROOT / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb",
        "tuning": ROOT / "Wuxianmen_V5_4K_Core-tuning.json",
    },
    {
        "name": "Wuxianmen_V5_FullPBR",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR",
        "source": VARIANT_ROOT / "Wuxianmen_V5_FullPBR/Wuxianmen_V5_50M_Instanced_WindingOutward.glb",
        "tuning": ROOT / "Wuxianmen_V5_FullPBR-tuning.json",
    },
]
EAL = unreal.EditorAssetLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _all_level_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _import_one(package):
    destination = package["destination"] + "/Meshes/ReferenceTuned20260917"
    existing = [
        actor for actor in _all_level_actors()
        if (actor.get_component_by_class(unreal.StaticMeshComponent)
            and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh
            and _path(actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh).startswith(destination + "/"))
    ]
    if existing:
        actors = existing
        print("V5_WINDING_IMPORT_RECOVER", package["name"], len(actors))
    else:
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.import_offset_rotation = unreal.Rotator(roll=-90.0)
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
        parameters.override_pipelines = [
            unreal.SoftObjectPath(pipeline.get_path_name()),
            unreal.SoftObjectPath(scene_pipeline.get_path_name()),
        ]
        manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        before = {_path(actor) for actor in _all_level_actors()}
        source = manager.create_source_data(str(package["source"]))
        assert manager.import_scene(destination, source, parameters), str(package["source"])
        actors = [actor for actor in _all_level_actors() if _path(actor) not in before]
    meshes = {}
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component is None or component.static_mesh is None:
            continue
        mesh = component.static_mesh
        meshes[_path(mesh)] = mesh
    assert len(meshes) == 7, (package["name"], sorted(meshes))
    tuning = json.loads(package["tuning"].read_text(encoding="utf-8"))["tuned_materials"]
    role_materials = {
        "stone": tuning["M_Stone"],
        "wood": tuning["M_Wood"],
        "plaster": tuning["M_Plaster"],
        "iron": tuning["M_Iron"],
        "tile": tuning["M_RoofTile"],
        "ridge": tuning["M_RoofTile"],
        "plaque": tuning["M_Plaque_Wuxianmen"],
    }
    records = []
    for mesh_path, mesh in sorted(meshes.items()):
        stem = mesh_path.rsplit("/", 1)[-1].split(".", 1)[0].lower()
        slots = mesh.get_editor_property("static_materials")
        slot_names = [str(slot.material_slot_name).lower() for slot in slots]
        print("V5_WINDING_MESH", package["name"], stem, slot_names)
        role = next((key for key in role_materials if key in stem or any(key in slot for slot in slot_names)), None)
        assert role, (package["name"], mesh_path)
        tuned = EAL.load_asset(role_materials[role])
        assert slots, (package["name"], mesh_path)
        for index in range(len(slots)):
            mesh.set_material(index, tuned)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("double_sided_geometry", True)
        nanite = None
        try:
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("fallback_percent_triangles", 100.0)
            nanite.set_editor_property("fallback_relative_error", 0.0)
            mesh.set_editor_property("nanite_settings", nanite)
        except Exception as exc:
            print("V5_NANITE_FALLBACK_NOTE", package["name"], mesh_path, str(exc))
        assert EAL.save_loaded_asset(mesh)
        records.append({
            "mesh": mesh_path,
            "role": role,
            "tuned_material": _path(tuned),
            "sections": int(mesh.get_num_sections(0)),
            "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
            "nanite_fallback_requested": nanite is not None,
        })
    EAL.save_directory(destination, only_if_is_dirty=False, recursive=True)
    for actor in actors:
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
    report = {
        "name": package["name"],
        "source": str(package["source"]),
        "destination": destination,
        "mesh_count": len(records),
        "meshes": records,
        "blueprint_touched": False,
        "temporary_actors_destroyed": True,
    }
    (ROOT / (package["name"] + "-winding-import.json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("V5_WINDING_IMPORTED", package["name"], len(records), destination)


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    print("V5_WINDING_DIRTY_MAP_RECORDED", [_path(item) for item in dirty])
    for package in PACKAGES:
        _import_one(package)


if __name__ == "__main__":
    main()
