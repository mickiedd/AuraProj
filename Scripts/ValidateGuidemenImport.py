"""Read-only validation for the imported Guidemen GLB asset."""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve()
assert PROJECT_ROOT == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"

DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen"
MESH_PATH = DESTINATION + "/SM_Guidemen.SM_Guidemen"
REPORT_PATH = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
) / "RawModelImport" / "Guidemen-validation.json"

mesh = unreal.load_asset(MESH_PATH)
assert isinstance(mesh, unreal.StaticMesh), MESH_PATH

bounds = mesh.get_bounds()
slots = [slot.material_interface for slot in mesh.static_materials]
assert slots and all(slots), "Every static-mesh material slot must be assigned"
assert mesh.get_editor_property("nanite_settings").enabled, "Nanite is disabled"

assets = unreal.EditorAssetLibrary.list_assets(
    DESTINATION, recursive=False, include_folder=False
)
textures = {}
materials = {}
for asset_path in assets:
    obj = unreal.load_asset(asset_path)
    if isinstance(obj, unreal.Texture2D):
        textures[obj.get_name()] = {
            "path": obj.get_path_name(),
            "srgb": bool(obj.get_editor_property("srgb")),
            "compression": str(obj.get_editor_property("compression_settings")),
        }
    elif isinstance(obj, unreal.Material):
        materials[obj.get_name()] = {
            "path": obj.get_path_name(),
            "assigned_to_imported_mesh": obj in slots,
        }

basecolor = {name: value for name, value in textures.items() if "basecolor" in name}
normal = {name: value for name, value in textures.items() if "normal" in name}
orm = {name: value for name, value in textures.items() if "_orm" in name}
assert len(textures) == 15, f"Expected 15 saved embedded textures, found {len(textures)}"
assert len(basecolor) == 5 and len(normal) == 5 and len(orm) == 5, (
    f"Expected five BaseColor, five Normal, and five ORM textures; "
    f"found {len(basecolor)}, {len(normal)}, {len(orm)}: {sorted(textures)}"
)
assert basecolor and all(value["srgb"] for value in basecolor.values()), (
    "BaseColor textures must use sRGB"
)
assert normal and all(not value["srgb"] for value in normal.values()), (
    "Normal textures must use linear sampling"
)
assert orm and all(not value["srgb"] for value in orm.values()), (
    "ORM textures must use linear sampling"
)

result = {
    "mesh": mesh.get_path_name(),
    "size_cm": [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2],
    "nanite": mesh.get_editor_property("nanite_settings").enabled,
    "material_slots": [slot.get_path_name() for slot in slots],
    "materials": materials,
    "textures": textures,
    "checks": {
        "mesh_is_static": True,
        "all_material_slots_assigned": True,
        "nanite_enabled": True,
        "basecolor_srgb": True,
        "normal_linear": True,
        "orm_linear": True,
        "collision": "intentionally not generated; author simple gameplay collision separately",
    },
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("VALIDATION_PASSED", json.dumps(result))
