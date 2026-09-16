"""Close the visible base gaps in the three assembled V4 gate Blueprints.

These are thin, no-collision floor liners authored from the existing Cube asset
and each package's AAA stone material.  They close the black/open base gaps in
the gateway passages without changing the walk-through collision proxy or the
source meshes.
"""
import json
from pathlib import Path
import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V4"
CONFIG = {
    "Guidemen_V4": ("/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen", "M_Stone_BlueGrey_AAA", (1100.0, 600.0, 10.0)),
    "Wuxianmen_V4": ("/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen", "M_WeatheredStone_AAA", (1350.0, 700.0, 10.0)),
    "Zhengximen_V4": ("/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen", "M_StoneFoundation_AAA", (1000.0, 1100.0, 10.0)),
}
PREFIX = "IntactFill_"
CUBE = "/Engine/BasicShapes/Cube.Cube"


def add_or_update(bp, package_name, dest, material_name, dimensions):
    material = unreal.EditorAssetLibrary.load_asset(dest + "/Materials/" + material_name)
    assert material, dest + "/Materials/" + material_name
    mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
    assert mesh
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    root = None
    existing = None
    for handle in ss.k2_gather_subobject_data_for_blueprint(bp):
        data = ss.k2_find_subobject_data_from_handle(handle)
        if lib.is_root_component(data):
            root = handle
        if lib.is_component(data):
            obj = lib.get_object(data)
            if obj and obj.get_name().startswith(PREFIX + package_name.replace("_V4", "")):
                existing = obj
    assert root
    if existing is None:
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", root)
        params.set_editor_property("new_class", unreal.StaticMeshComponent)
        params.set_editor_property("blueprint_context", bp)
        params.set_editor_property("conform_transform_to_parent", False)
        handle, reason = ss.add_new_subobject(params)
        assert lib.is_handle_valid(handle), str(reason)
        name = PREFIX + package_name.replace("_V4", "") + "_Floor"
        ss.rename_subobject(handle, unreal.Text(name))
        existing = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
    existing.set_static_mesh(mesh)
    existing.set_material(0, material)
    existing.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 5.0))
    existing.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    existing.set_editor_property("relative_scale3d", unreal.Vector(dimensions[0] / 100.0, dimensions[1] / 100.0, dimensions[2] / 100.0))
    existing.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    existing.set_collision_profile_name("NoCollision")
    existing.set_editor_property("visible", True)
    existing.set_editor_property("hidden_in_game", False)
    existing.set_editor_property("cast_shadow", False)
    return existing


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before V4 interior fill: " + str(dirty)
    reports = {}
    for name, (dest, material_name, dimensions) in CONFIG.items():
        bp = unreal.EditorAssetLibrary.load_asset(dest + "/BP_" + name)
        assert bp, dest + "/BP_" + name
        component = add_or_update(bp, name, dest, material_name, dimensions)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        reports[name] = {
            "blueprint": dest + "/BP_" + name,
            "component": component.get_name(),
            "material": dest + "/Materials/" + material_name,
            "location_cm": [0.0, 0.0, 5.0],
            "dimensions_cm": list(dimensions),
            "collision": "NoCollision",
        }
    (ROOT / "V4-interior-fill.json").write_text(json.dumps(reports, indent=2), encoding="utf-8")
    print("V4_INTERIOR_FILL_COMPLETE", json.dumps(reports))


if __name__ == "__main__":
    main()
