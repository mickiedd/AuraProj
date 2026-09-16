"""Capture the final visual check for the intact infill variant."""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_south_gate_zhengnanmen_intact_preview.png")
CAMERA = (0.0, 4700.0, 1900.0)
TARGET = (0.0, 0.0, 1100.0)

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level_editor.load_level(LEVEL_PATH), LEVEL_PATH
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors()

directional = next(
    (actor for actor in actors if actor.get_class().get_name() == "DirectionalLight"),
    None,
)
if directional is None:
    directional = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 2500.0),
        unreal.Rotator(pitch=-42.0, yaw=-35.0, roll=0.0),
    )
directional.set_actor_rotation(unreal.Rotator(pitch=-42.0, yaw=-35.0, roll=0.0), False)
directional_component = directional.get_component_by_class(unreal.DirectionalLightComponent)
if directional_component:
    directional_component.set_intensity(25.0)

sky = next(
    (actor for actor in actors if actor.get_class().get_name() == "SkyLight"),
    None,
)
if sky is None:
    sky = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 2500.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
if sky_component:
    sky_component.set_intensity(8.0)
    sky_component.recapture_sky()

camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal_editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal_editor.set_level_viewport_camera_info(camera, rotation)
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
unreal.AutomationLibrary.take_high_res_screenshot(
    1600,
    900,
    str(OUTPUT),
    None,
    False,
    False,
    unreal.ComparisonTolerance.LOW,
    "Great South Gate Zhengnanmen intact infill visual check",
    0.5,
    True,
)
assert unreal.EditorLoadingAndSavingUtils.save_map(unreal_editor.get_editor_world(), LEVEL_PATH)
print("GREAT_SOUTH_GATE_ZHENGNANMEN_INTACT_VISUAL_CAPTURE", {"path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET)})
