"""Capture a final editor viewport image of the isolated high-fidelity import."""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_south_gate_zhengnanmen_high_fidelity_preview.png")
CAMERA = (-4300.0, -4300.0, 3100.0)
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
        unreal.Vector(0.0, 0.0, 1000.0),
        unreal.Rotator(pitch=-35.0, yaw=-45.0, roll=0.0),
    )
directional.set_actor_rotation(unreal.Rotator(pitch=-35.0, yaw=-45.0, roll=0.0), False)
directional_component = directional.get_component_by_class(unreal.DirectionalLightComponent)
if directional_component:
    directional_component.set_intensity(8.0)

sky = next(
    (actor for actor in actors if actor.get_class().get_name() == "SkyLight"),
    None,
)
if sky is None:
    sky = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight,
        unreal.Vector(0.0, 0.0, 1200.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
if sky_component:
    sky_component.set_intensity(1.2)
    sky_component.recapture_sky()

camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation
)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600,
    900,
    str(OUTPUT),
    None,
    False,
    False,
    unreal.ComparisonTolerance.LOW,
    "Great South Gate Zhengnanmen high-fidelity preview",
    0.5,
    True,
)
assert unreal.EditorLoadingAndSavingUtils.save_map(
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),
    LEVEL_PATH,
), "Failed to save preview level after lighting setup"
print("GREAT_SOUTH_GATE_ZHENGNANMEN_HIGH_FIDELITY_VISUAL_CAPTURE", {
    "path": str(OUTPUT),
    "camera": list(CAMERA),
    "target": list(TARGET),
    "task": str(task),
})
