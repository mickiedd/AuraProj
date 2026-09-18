"""Visual probe for the corrected zero-roll Wuxianmen scene; no map save."""
from __future__ import annotations

from pathlib import Path
import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V5"


def main():
    actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = [actor for actor in actors_api.get_all_level_actors() if actor.get_component_by_class(unreal.StaticMeshComponent) and actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh and "/Wuxianmen_4K_Core/Meshes/ReferenceTuned20260917ZeroRollProbe/" in actor.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_path_name()]
    assert len(actors) >= 5103, len(actors)
    shift = unreal.Vector(100000.0, 100000.0, 0.0)
    for actor in actors:
        actor.set_actor_location(actor.get_actor_location() + shift, False, False)
    mins = [1e30, 1e30, 1e30]
    maxs = [-1e30, -1e30, -1e30]
    for actor in actors:
        origin, extent = actor.get_actor_bounds(False)
        mins = [min(mins[i], (origin.x, origin.y, origin.z)[i] - (extent.x, extent.y, extent.z)[i]) for i in range(3)]
        maxs = [max(maxs[i], (origin.x, origin.y, origin.z)[i] + (extent.x, extent.y, extent.z)[i]) for i in range(3)]
    origin = unreal.Vector((mins[0] + maxs[0]) * 0.5, (mins[1] + maxs[1]) * 0.5, (mins[2] + maxs[2]) * 0.5)
    extent = unreal.Vector((maxs[0] - mins[0]) * 0.5, (maxs[1] - mins[1]) * 0.5, (maxs[2] - mins[2]) * 0.5)
    light = actors_api.spawn_actor_from_class(unreal.DirectionalLight, origin + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
    light.set_actor_label("V5ZEROPROBE_LIGHT")
    light.light_component.set_intensity(7.0)
    sky = actors_api.spawn_actor_from_class(unreal.SkyLight, origin + unreal.Vector(0, 0, 3500))
    sky.set_actor_label("V5ZEROPROBE_SKY")
    sky.light_component.set_intensity(1.4)
    post = actors_api.spawn_actor_from_class(unreal.PostProcessVolume, origin)
    post.set_actor_label("V5ZEROPROBE_POST")
    post.set_editor_property("unbound", False)
    post.set_editor_property("priority", 100.0)
    settings = post.get_editor_property("settings")
    for name, value in (("override_auto_exposure_min_brightness", True), ("override_auto_exposure_max_brightness", True), ("auto_exposure_min_brightness", 1.0), ("auto_exposure_max_brightness", 1.0)):
        settings.set_editor_property(name, value)
    post.set_editor_property("settings", settings)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * 0.48)
    radius = max(extent.x, extent.y, extent.z)
    camera = target + unreal.Vector(0.0, radius * 2.4, radius * 0.38)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    output = ROOT / "Wuxianmen_V5_4K_Core-reference-tuning-zero-roll-probe.png"
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, "Wuxianmen zero-roll reference tuning probe", 2.0, True)
    print("V5_ZERO_ROLL_PROBE_CAPTURE_REQUESTED", str(output), [maxs[i] - mins[i] for i in range(3)])


if __name__ == "__main__":
    main()
