"""Import the Great South Gate high-fidelity GLB as an instanced UE5 scene.

The source package is intentionally kept outside the repository.  The GLB is
authored Z-up and contains many repeated mesh nodes, so this import uses the
Interchange scene pipeline with mesh baking disabled.  That keeps repeated
stone blocks, roof tiles, railings, columns and brackets as shared static mesh
assets while the hierarchy carries their individual transforms.

The import is isolated in a new preview level so the existing showcase map and
the previously imported Great South Gate remain unchanged.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
PACKAGE_ROOT = Path(
    "C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package"
)
SOURCE = PACKAGE_ROOT / "GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
LEVEL_PATH = DEST + "/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
REPORT_PATH = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_HighFidelity.json"

EXPECTED_SOURCE_TRIANGLES = 179312
EXPECTED_SOURCE_NODES = 12196
EXPECTED_SOURCE_MESHES = 112
EXPECTED_SOURCE_MATERIALS = 7
EXPECTED_SOURCE_TEXTURES = 17


def editor_world():
    return unreal.get_editor_subsystem(
        unreal.UnrealEditorSubsystem
    ).get_editor_world()


def all_level_actors():
    return unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()


def is_level_saved():
    return unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH)


def load_or_create_preview_level():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if is_level_saved():
        assert level_editor.load_level(LEVEL_PATH), LEVEL_PATH
        return level_editor.get_current_level(), False

    assert level_editor.new_level(LEVEL_PATH), LEVEL_PATH
    return level_editor.get_current_level(), True


def configure_pipelines():
    assets_pipeline = unreal.InterchangeGenericAssetsPipeline()
    assets_pipeline.asset_name = "SM_GreatSouthGate_Zhengnanmen_HighFidelity"
    # The source is Z-up.  Interchange applies this global offset to the root
    # scene node even when mesh baking is disabled, which preserves shared mesh
    # prototypes while restoring the source's intended UE orientation.
    assets_pipeline.import_offset_rotation = unreal.Rotator(roll=-90.0)
    assets_pipeline.import_offset_uniform_scale = 1.0
    assets_pipeline.common_meshes_properties.bake_meshes = False
    assets_pipeline.common_meshes_properties.bake_pivot_meshes = False
    assets_pipeline.mesh_pipeline.combine_static_meshes = False
    assets_pipeline.mesh_pipeline.build_nanite = True
    assets_pipeline.mesh_pipeline.set_editor_property("collision", False)
    assets_pipeline.mesh_pipeline.generate_lightmap_u_vs = False

    scene_pipeline = unreal.InterchangeGenericLevelPipeline()
    scene_pipeline.scene_hierarchy_type = (
        unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
    )
    scene_pipeline.delete_missing_assets = False
    scene_pipeline.force_reimport_deleted_actors = False
    scene_pipeline.force_reimport_deleted_assets = False

    return assets_pipeline, scene_pipeline


def static_mesh_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEST], True)
    assets = []
    for path in unreal.EditorAssetLibrary.list_assets(
        DEST, recursive=True, include_folder=False
    ):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            assets.append(asset)
    return sorted(assets, key=lambda mesh: mesh.get_path_name())


def static_mesh_components(actors):
    result = []
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component and component.static_mesh:
            result.append((actor, component))
    return result


def bounds_report(components):
    if not components:
        return None
    bounds = [
        actor.get_actor_bounds(False)
        for actor, _component in components
    ]
    mins = [
        min(origin.x - extent.x for origin, extent in bounds),
        min(origin.y - extent.y for origin, extent in bounds),
        min(origin.z - extent.z for origin, extent in bounds),
    ]
    maxs = [
        max(origin.x + extent.x for origin, extent in bounds),
        max(origin.y + extent.y for origin, extent in bounds),
        max(origin.z + extent.z for origin, extent in bounds),
    ]
    return {"min": mins, "max": maxs, "size": [maxs[i] - mins[i] for i in range(3)]}


assert PROJECT_ROOT.resolve() == Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve(), "Wrong Unreal project"
assert SOURCE.is_file(), SOURCE

level, level_created = load_or_create_preview_level()
actors_before = all_level_actors()
components_before = static_mesh_components(actors_before)
existing_assets = static_mesh_assets()

if existing_assets and components_before:
    imported_this_run = False
    imported_objects = []
else:
    assert not existing_assets and not components_before, (
        "Preview level or destination contains a partial import; refusing to mix "
        "assets or duplicate scene actors"
    )
    assets_pipeline, scene_pipeline = configure_pipelines()
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.import_level = level
    params.replace_existing = False
    params.override_pipelines = [
        unreal.SoftObjectPath(assets_pipeline.get_path_name()),
        unreal.SoftObjectPath(scene_pipeline.get_path_name()),
    ]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    imported_ok = manager.import_scene(
        DEST, manager.create_source_data(str(SOURCE)), params
    )
    assert imported_ok, "Interchange scene import returned false"
    assert unreal.EditorAssetLibrary.save_directory(DEST), (
        "Failed to save imported Great South Gate assets"
    )
    imported_objects = [
        asset.get_path_name()
        for asset in static_mesh_assets()
    ]
    imported_this_run = True

actors_after = all_level_actors()
components_after = static_mesh_components(actors_after)
meshes_after = static_mesh_assets()
assert meshes_after, "No static mesh assets were imported"
assert components_after, "No static mesh scene actors were imported"

world = editor_world()
assert world and unreal.EditorLoadingAndSavingUtils.save_map(
    world, LEVEL_PATH
), "Failed to save preview level"

report = {
    "source": str(SOURCE),
    "source_bytes": SOURCE.stat().st_size,
    "package_root": str(PACKAGE_ROOT),
    "destination": DEST,
    "preview_level": LEVEL_PATH,
    "level_created_this_run": level_created,
    "imported_this_run": imported_this_run,
    "imported_objects": imported_objects,
    "pipeline": {
        "translator": "Interchange glTF",
        "scene_hierarchy": "Create level actors",
        "mesh_baking": False,
        "combine_static_meshes": False,
        "nanite": True,
        "collision": "not_generated_for_decorative_nanite_mesh",
        "import_roll": -90.0,
    },
    "source_manifest_expectations": {
        "meshes": EXPECTED_SOURCE_MESHES,
        "nodes": EXPECTED_SOURCE_NODES,
        "materials": EXPECTED_SOURCE_MATERIALS,
        "textures": EXPECTED_SOURCE_TEXTURES,
        "stored_triangles": EXPECTED_SOURCE_TRIANGLES,
    },
    "asset_inventory": {
        "static_mesh_count": len(meshes_after),
        "asset_count": len(
            unreal.EditorAssetLibrary.list_assets(
                DEST, recursive=True, include_folder=False
            )
        ),
        "static_meshes": [mesh.get_path_name() for mesh in meshes_after],
    },
    "scene_inventory": {
        "actors_before": len(actors_before),
        "actors_after": len(actors_after),
        "static_mesh_actor_count": len(components_after),
        "bounds_cm": bounds_report(components_after),
        "actor_labels_sample": sorted(
            actor.get_actor_label() for actor, _component in components_after[:20]
        ),
    },
    "passed": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
print(
    "GREAT_SOUTH_GATE_ZHENGNANMEN_HIGH_FIDELITY_IMPORTED",
    json.dumps(
        {
            "imported_this_run": imported_this_run,
            "preview_level": LEVEL_PATH,
            "static_mesh_assets": len(meshes_after),
            "static_mesh_actors": len(components_after),
            "bounds_cm": report["scene_inventory"]["bounds_cm"],
        }
    ),
)
