"""Validate the isolated Great South Gate high-fidelity preview import."""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
SOURCE = Path(
    "C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package"
) / "GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
LEVEL_PATH = DEST + "/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_HighFidelity.json"
SOURCE_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_HighFidelity-source-analysis.json"
VALIDATION_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_HighFidelity-validation.json"

EXPECTED_SOURCE_MESHES = 112
EXPECTED_SOURCE_NODES = 12196
EXPECTED_SOURCE_MATERIALS = 7
EXPECTED_SOURCE_TEXTURES = 17
EXPECTED_SOURCE_TRIANGLES = 179312
EXPECTED_BOUNDS_CM = [2700.0, 1705.3526, 2303.4817]
BOUNDS_TOLERANCE_CM = 2.0
INTACT_FILL_PREFIX = "Zhengnanmen_IntactFill_"


def level_actors():
    return unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    ).get_all_level_actors()


def mesh_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEST], True)
    result = []
    for path in unreal.EditorAssetLibrary.list_assets(
        DEST, recursive=True, include_folder=False
    ):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            result.append(asset)
    return sorted(result, key=lambda mesh: mesh.get_path_name())


def mesh_components(actors):
    result = []
    for actor in actors:
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component and component.static_mesh:
            result.append((actor, component))
    return result


def aggregate_bounds(components):
    values = [actor.get_actor_bounds(False) for actor, _component in components]
    mins = [
        min(origin.x - extent.x for origin, extent in values),
        min(origin.y - extent.y for origin, extent in values),
        min(origin.z - extent.z for origin, extent in values),
    ]
    maxs = [
        max(origin.x + extent.x for origin, extent in values),
        max(origin.y + extent.y for origin, extent in values),
        max(origin.z + extent.z for origin, extent in values),
    ]
    return {"min": mins, "max": maxs, "size": [maxs[i] - mins[i] for i in range(3)]}


assert PROJECT_ROOT.resolve() == Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve(), "Wrong Unreal project"
assert SOURCE.is_file(), SOURCE
assert IMPORT_REPORT.is_file(), IMPORT_REPORT
assert SOURCE_REPORT.is_file(), SOURCE_REPORT
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH), LEVEL_PATH

actors = level_actors()
infill_actors = [actor for actor in actors if actor.get_actor_label().startswith(INTACT_FILL_PREFIX)]
# Keep this import validator focused on the original GLB scene. The later
# intact-façade pass adds engine-cube panels, which are validated separately.
components = mesh_components(
    [actor for actor in actors if not actor.get_actor_label().startswith(INTACT_FILL_PREFIX)]
)
meshes = mesh_assets()
all_assets = unreal.EditorAssetLibrary.list_assets(
    DEST, recursive=True, include_folder=False
)
materials = [
    unreal.EditorAssetLibrary.load_asset(path)
    for path in all_assets
    if "/M_" in path
]
textures = []
for path in all_assets:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if isinstance(asset, unreal.Texture2D):
        textures.append(asset)

errors = []


def check(condition, message):
    if not condition:
        errors.append(message)


source_report = json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
check(source_report["counts"]["meshes"] == EXPECTED_SOURCE_MESHES, "offline mesh count changed")
check(source_report["counts"]["nodes"] == EXPECTED_SOURCE_NODES, "offline node count changed")
check(source_report["counts"]["materials"] == EXPECTED_SOURCE_MATERIALS, "offline material count changed")
check(source_report["counts"]["textures"] == EXPECTED_SOURCE_TEXTURES, "offline texture count changed")
check(source_report["totals"]["source_triangles"] == EXPECTED_SOURCE_TRIANGLES, "offline triangle count changed")

check(len(meshes) == EXPECTED_SOURCE_MESHES, "expected 112 shared static mesh assets, found {}".format(len(meshes)))
check(len(components) == EXPECTED_SOURCE_NODES - 1, "expected 12195 scene mesh actors, found {}".format(len(components)))
check(len(components) > len(meshes), "scene import did not preserve repeated geometry as shared assets")
check(len(materials) >= EXPECTED_SOURCE_MATERIALS, "expected at least 7 imported materials")
check(len(textures) >= EXPECTED_SOURCE_TEXTURES, "expected at least 17 imported textures")

nanite_disabled = []
unassigned_materials = []
for mesh in meshes:
    nanite = mesh.get_editor_property("nanite_settings").get_editor_property("enabled")
    if not nanite:
        nanite_disabled.append(mesh.get_name())
    if any(slot.material_interface is None for slot in mesh.static_materials):
        unassigned_materials.append(mesh.get_name())
check(not nanite_disabled, "Nanite disabled on {}".format(nanite_disabled[:10]))
check(not unassigned_materials, "unassigned material slots on {}".format(unassigned_materials[:10]))

bounds = aggregate_bounds(components)
for axis, expected in enumerate(EXPECTED_BOUNDS_CM):
    check(
        abs(bounds["size"][axis] - expected) <= BOUNDS_TOLERANCE_CM,
        "axis {} is {} cm, expected {} cm".format(axis, round(bounds["size"][axis], 3), expected),
    )
check(abs(bounds["min"][2]) <= BOUNDS_TOLERANCE_CM, "scene is not grounded at Z=0: {}".format(bounds["min"][2]))
check(bounds["size"][2] > bounds["size"][1], "scene is not upright: {}".format(bounds["size"]))

result = {
    "passed": not errors,
    "source": str(SOURCE),
    "level": LEVEL_PATH,
    "destination": DEST,
    "source_expectations": {
        "meshes": EXPECTED_SOURCE_MESHES,
        "nodes": EXPECTED_SOURCE_NODES,
        "materials": EXPECTED_SOURCE_MATERIALS,
        "textures": EXPECTED_SOURCE_TEXTURES,
        "stored_triangles": EXPECTED_SOURCE_TRIANGLES,
    },
    "asset_inventory": {
        "all_assets": len(all_assets),
        "static_meshes": len(meshes),
        "materials": len(materials),
        "textures": len(textures),
        "nanite_disabled": nanite_disabled,
        "unassigned_materials": unassigned_materials,
    },
    "scene_inventory": {
        "actors": len(actors),
        "static_mesh_actors": len(components),
        "intact_fill_actors_excluded": len(infill_actors),
        "bounds_cm": bounds,
        "shared_geometry_ratio": round(len(components) / len(meshes), 3) if meshes else None,
    },
    "pipeline": import_report.get("pipeline", {}),
    "errors": errors,
}
VALIDATION_REPORT.parent.mkdir(parents=True, exist_ok=True)
VALIDATION_REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_SOUTH_GATE_ZHENGNANMEN_HIGH_FIDELITY_VALIDATION", json.dumps({
    "passed": result["passed"],
    "static_meshes": len(meshes),
    "static_mesh_actors": len(components),
    "bounds_cm": [round(value, 3) for value in bounds["size"]],
    "errors": errors,
}))
assert not errors, "; ".join(errors)
