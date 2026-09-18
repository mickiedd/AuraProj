"""Capture one missing V5 Blueprint validation view per editor invocation."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V5"
VIEWS = ("front", "rear", "side", "close")


def _ensure_scene(actor, level):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current = actors.get_all_level_actors()
    if not any(item.get_actor_label() == "V5_Preview_Key" for item in current):
        key = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
        key.set_actor_label("V5_Preview_Key")
        key.light_component.set_intensity(7.0)
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
        fill.set_actor_label("V5_Preview_Fill")
        fill.light_component.set_intensity(2.0)
        fill.light_component.set_editor_property("cast_shadows", False)
        actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
        sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3500))
        sky.light_component.set_intensity(1.4)
        sky.light_component.set_editor_property("real_time_capture", True)
        post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
        post.set_editor_property("unbound", True)
        settings = post.get_editor_property("settings")
        for name, value in (
            ("override_auto_exposure_min_brightness", True),
            ("override_auto_exposure_max_brightness", True),
            ("auto_exposure_min_brightness", 1.0),
            ("auto_exposure_max_brightness", 1.0),
        ):
            settings.set_editor_property(name, value)
        post.set_editor_property("settings", settings)
        floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -10))
        floor.set_actor_label("V5_Preview_Ground")
        floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        floor.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround"
        material = unreal.EditorAssetLibrary.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None
        if material is None:
            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "M_V5PreviewGround", "/Game/Assets/Environment/GuangzhouLandmarks/V5", unreal.Material, unreal.MaterialFactoryNew()
            )
            color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
            color.set_editor_property("constant", unreal.LinearColor(0.15, 0.17, 0.19, 1.0))
            unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
            roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant)
            roughness.set_editor_property("r", 0.86)
            unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
        floor.static_mesh_component.set_material(0, material)
        origin, extent = actor.get_actor_bounds(False)
        floor.set_actor_location(unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0), False, False)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, level)


def capture(index, view):
    cfg = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))[index]
    report = json.loads((ROOT / (cfg["name"] + "-import.json")).read_text(encoding="utf-8"))
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report["preview_level"])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = next((actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == "Preview_" + cfg["name"]), None)
    if existing:
        transform = existing.get_actor_transform()
        actors.destroy_actor(existing)
    else:
        transform = unreal.Transform()
    blueprint = unreal.EditorAssetLibrary.load_asset(report["blueprint"])
    building = actors.spawn_actor_from_class(blueprint.generated_class(), transform.translation, transform.rotation.rotator())
    assert building
    building.set_actor_scale3d(transform.scale3d)
    building.set_actor_label("Preview_" + cfg["name"])
    _ensure_scene(building, report["preview_level"])
    origin, extent = building.get_actor_bounds(False)
    floor = next((actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == "V5_Preview_Ground"), None)
    if floor:
        floor.set_actor_location(unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0), False, False)
    radius = max(extent.x, extent.y, extent.z)
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * 0.02)
    offsets = {
        "front": unreal.Vector(0, radius * 2.25, radius * 0.62),
        "rear": unreal.Vector(0, -radius * 2.25, radius * 0.62),
        "side": unreal.Vector(radius * 2.25, 0, radius * 0.62),
        "close": unreal.Vector(0, radius * 1.45, radius * 0.42),
    }
    camera = target + offsets[view]
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 0")
    unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 100")
    unreal.SystemLibrary.execute_console_command(world, "ShowFlag.Grid 0")
    unreal.SystemLibrary.execute_console_command(world, "ShowFlag.SelectionOutline 0")
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, report["preview_level"])
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / (cfg["name"] + "-" + view + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(
        1600, 1000, str(output), None, False, False,
        unreal.ComparisonTolerance.LOW,
        cfg["name"] + " Blueprint visual integrity " + view,
        2.0, True,
    )
    print("V5_CAPTURE_REQUESTED", str(output))


def main():
    packages = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))
    for index, cfg in enumerate(packages):
        for view in VIEWS:
            output = ROOT / (cfg["name"] + "-" + view + ".png")
            if not output.exists():
                capture(index, view)
                return
    print("V5_CAPTURE_COMPLETE", len(packages) * len(VIEWS))


if __name__ == "__main__":
    main()
