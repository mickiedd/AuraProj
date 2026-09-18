"""Capture one isolated reference-tuning view per invocation; never save the map."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V5"
VIEWS = ("hero", "front", "rear", "side_l", "side_r", "low", "top", "arch_close")
OFFSETS = {
    "hero": (1.70, 1.70, 0.92), "front": (0.00, 2.60, 0.40), "rear": (0.00, -2.60, 0.40),
    "side_l": (-2.60, 0.00, 0.48), "side_r": (2.60, 0.00, 0.48), "low": (0.00, 2.00, 0.18),
    "top": (0.00, 0.00, 3.20), "arch_close": (0.00, 1.18, 0.30),
}


def _path(value):
    return value.get_path_name() if value else ""


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _cleanup():
    actors = _actors()
    removed = 0
    for actor in list(actors.get_all_level_actors()):
        if actor.get_actor_label().startswith("V5TUNE_"):
            actors.destroy_actor(actor)
            removed += 1
    return removed


def _setup(package):
    actors = _actors()
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    import_report = json.loads((ROOT / (package["name"] + "-import.json")).read_text(encoding="utf-8"))
    blueprint = unreal.EditorAssetLibrary.load_asset(import_report["blueprint"])
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    building.set_actor_label("V5TUNE_BUILDING")
    origin, extent = building.get_actor_bounds(False)
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0))
    floor.set_actor_label("V5TUNE_GROUND")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    ground_material = unreal.EditorAssetLibrary.load_asset("/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround")
    if ground_material:
        floor.static_mesh_component.set_material(0, ground_material)
    key = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
    key.set_actor_label("V5TUNE_KEY")
    key.light_component.set_intensity(7.0)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
    fill.set_actor_label("V5TUNE_FILL")
    fill.light_component.set_intensity(2.0)
    fill.light_component.set_editor_property("cast_shadows", False)
    sky_atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, center)
    sky_atmosphere.set_actor_label("V5TUNE_ATMOSPHERE")
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.set_actor_label("V5TUNE_SKY")
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, center)
    post.set_actor_label("V5TUNE_POST")
    post.set_editor_property("unbound", False)
    post.set_editor_property("priority", 100.0)
    settings = post.get_editor_property("settings")
    for name, value in (("override_auto_exposure_min_brightness", True), ("override_auto_exposure_max_brightness", True), ("auto_exposure_min_brightness", 1.0), ("auto_exposure_max_brightness", 1.0)):
        settings.set_editor_property(name, value)
    post.set_editor_property("settings", settings)
    post.set_actor_location(center, False, False)
    return building, origin, extent


def _capture(index):
    packages = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))
    package_index = index // len(VIEWS)
    view = VIEWS[index % len(VIEWS)]
    package = packages[package_index]
    _cleanup()
    building, origin, extent = _setup(package)
    radius = max(extent.x, extent.y, extent.z)
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * (0.34 if view == "arch_close" else 0.50))
    offset = OFFSETS[view]
    camera = target + unreal.Vector(offset[0] * radius, offset[1] * radius, offset[2] * radius)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / (package["name"] + "-reference-tuning-" + view + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, package["name"] + " reference tuning " + view, 2.0, True)
    state = {"next_index": index + 1, "last": {"package": package["name"], "view": view, "output": str(output)}, "saved_map": False}
    (ROOT / "reference-tuning-capture-state-20260917.json").write_text(json.dumps(state, indent=2), encoding="utf-8")
    print("V5_REFERENCE_STEP_REQUESTED", index, package["name"], view, str(output))


def main():
    if len(sys.argv) > 1 and sys.argv[1].lower() == "cleanup":
        print("V5_REFERENCE_STEP_CLEANUP", _cleanup())
        return
    state_path = ROOT / "reference-tuning-capture-state-20260917.json"
    index = json.loads(state_path.read_text(encoding="utf-8")).get("next_index", 0) if state_path.exists() else 0
    if index >= 24:
        print("V5_REFERENCE_STEP_COMPLETE", index)
        return
    _capture(index)


if __name__ == "__main__":
    main()
