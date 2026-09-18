"""Capture one transient post-fix Wuxianmen Core view; set VIEW before running."""
from pathlib import Path
import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
VIEW = "front"
OFFSET = (0.00, 2.60, 0.40)


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    origin, extent = building.get_actor_bounds(False)
    ground = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0))
    ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    ground.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    ground.static_mesh_component.set_material(0, unreal.EditorAssetLibrary.load_asset("/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround"))
    spawned = [building, ground]
    key = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
    key.light_component.set_intensity(7.0)
    spawned.append(key)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
    fill.light_component.set_intensity(2.0)
    fill.light_component.set_editor_property("cast_shadows", False)
    spawned.append(fill)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawned.append(sky)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    radius = max(extent.x, extent.y, extent.z)
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * 0.50)
    camera = target + unreal.Vector(OFFSET[0] * radius, OFFSET[1] * radius, OFFSET[2] * radius)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / ("Wuxianmen_V5_4K_Core-roof-tiling-after-" + VIEW + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, "Wuxianmen roof tiling after " + VIEW, 2.0, True)
    for actor in spawned:
        actors.destroy_actor(actor)
    print("V5_WUXIANMEN_ROOF_SINGLE_CAPTURE_REQUESTED", str(output))


if __name__ == "__main__":
    main()
