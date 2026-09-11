import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = Path(r"C:\Works\Raw3DModels\GreatSouthGate_Zhengnanmen_UE5_HighPoly.glb")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
ACTOR_LABEL = "GuangzhouLandmark_GreatSouthGate"
NEW_ASSET = (
    "/Game/Assets/Environment/GuangzhouLandmarks/"
    "GreatSouthGate_Zhengnanmen_HighPoly/"
    "SM_GreatSouthGate_Zhengnanmen_HighPoly"
)
LEGACY_ASSET = (
    "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate/"
    "SM_GreatSouthGate"
)
ASSET_DIR = (
    PROJECT_ROOT
    / "Content"
    / "Assets"
    / "Environment"
    / "GuangzhouLandmarks"
    / "GreatSouthGate_Zhengnanmen_HighPoly"
)
REPORT_PATH = PROJECT_ROOT / "Saved" / "RawModelImport" / "GreatSouthGateZhengnanmen.json"
VALIDATION_PATH = (
    PROJECT_ROOT / "Saved" / "RawModelImport" / "GreatSouthGateZhengnanmen-validation.json"
)


def asset_path(asset):
    return asset.get_path_name() if asset else None


def close_enough(actual, expected, tolerance=2.0):
    return abs(float(actual) - float(expected)) <= tolerance


errors = []
source_exists = SOURCE.is_file()
legacy_asset = unreal.load_asset(LEGACY_ASSET)
new_asset = unreal.load_asset(NEW_ASSET)

assert unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH), LEVEL_PATH
actors = [
    actor
    for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    if actor.get_actor_label() == ACTOR_LABEL
]
if len(actors) != 1:
    errors.append("expected exactly one Great South Gate actor")
    actor = actors[0] if actors else None
else:
    actor = actors[0]

actor_path = actor.get_path_name() if actor else None
component = actor.get_component_by_class(unreal.StaticMeshComponent) if actor else None
active_mesh_path = asset_path(component.static_mesh) if component else None

if not source_exists:
    errors.append("source GLB is missing")
if legacy_asset is None:
    errors.append("legacy Great South Gate asset is missing")
if new_asset is None:
    errors.append("new Great South Gate asset is missing")
if active_mesh_path is None or not active_mesh_path.startswith(NEW_ASSET + "."):
    errors.append("showcase actor does not reference the new Great South Gate mesh")

bounds = None
location = rotation = scale = None
if actor:
    origin, extent = actor.get_actor_bounds(False)
    bounds = {
        "min": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "center": [origin.x, origin.y, origin.z],
        "extent": [extent.x, extent.y, extent.z],
    }
    location_value = actor.get_actor_location()
    rotation_value = actor.get_actor_rotation()
    scale_value = actor.get_actor_scale3d()
    location = [location_value.x, location_value.y, location_value.z]
    rotation = [rotation_value.roll, rotation_value.pitch, rotation_value.yaw]
    scale = [scale_value.x, scale_value.y, scale_value.z]
    if not close_enough(origin.x, -140400.0) or not close_enough(origin.y, 115400.0):
        errors.append("actor bounds are not centered at the authored landmark position")
    if not close_enough(origin.z - extent.z, 99.9993085861206):
        errors.append("actor bounds are not grounded at the authored terrain height")
    if any(not close_enough(value, 0.0, 0.01) for value in rotation):
        errors.append("actor rotation changed")
    if any(not close_enough(value, 1.0, 0.01) for value in scale):
        errors.append("actor scale changed")

asset_files = sorted(path.name for path in ASSET_DIR.glob("*.uasset"))
texture_files = sorted(name for name in asset_files if "texture_" in name)
material_files = sorted(
    name for name in asset_files if name.startswith("M_") and name.endswith(".uasset")
)
mesh_files = sorted(
    name for name in asset_files if name.startswith("SM_") and name.endswith(".uasset")
)
if len(texture_files) != 10:
    errors.append("expected 10 imported texture assets")
if len(material_files) != 8:
    errors.append("expected 8 imported material assets")
if mesh_files != ["SM_GreatSouthGate_Zhengnanmen_HighPoly.uasset"]:
    errors.append("new asset folder has an unexpected static mesh inventory")

import_report = json.loads(REPORT_PATH.read_text(encoding="utf-8")) if REPORT_PATH.is_file() else {}
if import_report.get("nanite") is not True:
    errors.append("import report does not confirm Nanite")
if import_report.get("collision") != "not_generated_for_decorative_nanite_mesh":
    errors.append("import report collision policy changed unexpectedly")

result = {
    "passed": not errors,
    "source": str(SOURCE),
    "source_bytes": SOURCE.stat().st_size if source_exists else None,
    "level": LEVEL_PATH,
    "actor": actor_path,
    "label": actor.get_actor_label() if actor else None,
    "active_mesh": active_mesh_path,
    "legacy_mesh_exists": legacy_asset is not None,
    "new_mesh_exists": new_asset is not None,
    "location": location,
    "rotation": rotation,
    "scale": scale,
    "bounds": bounds,
    "asset_inventory": {
        "textures": texture_files,
        "materials": material_files,
        "meshes": mesh_files,
    },
    "nanite": import_report.get("nanite"),
    "collision": import_report.get("collision"),
    "errors": errors,
}
VALIDATION_PATH.parent.mkdir(parents=True, exist_ok=True)
VALIDATION_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
assert not errors, "; ".join(errors)
print("Great South Gate validation passed")
