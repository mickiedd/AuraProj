"""Deterministic unsaved front/close/rear captures for Deep AAA gate review."""
import json
import time
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V4/DeepAAA"
CONFIG = {
    "Wuxianmen": {
        "level": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/L_Wuxianmen_V4_Preview",
        "label": "Preview_Wuxianmen_V4",
        "name": "Wuxianmen_V4",
    },
    "Zhengximen": {
        "level": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/L_Zhengximen_V4_Preview",
        "label": "Preview_Zhengximen_V4",
        "name": "Zhengximen_V4",
    },
}


def _ensure_preview_lighting(actors):
    all_actors = actors.get_all_level_actors()
    key_label = next((label for label in ("DeepAAA_Key", "V4_Preview_Key") if any(a.get_actor_label() == label for a in all_actors)), None)
    fill_label = next((label for label in ("DeepAAA_Fill", "V4_Preview_Fill") if any(a.get_actor_label() == label for a in all_actors)), None)
    created_labels = []
    if key_label is None:
        key = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
        key.set_actor_label("DeepAAA_Key")
        key.light_component.set_intensity(7.0)
        key_label = "DeepAAA_Key"
        created_labels.append(key_label)
    if fill_label is None:
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-28, yaw=125))
        fill.set_actor_label("DeepAAA_Fill")
        fill.light_component.set_intensity(2.5)
        fill.light_component.set_editor_property("cast_shadows", False)
        fill_label = "DeepAAA_Fill"
        created_labels.append(fill_label)
    if created_labels:
        sky = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "DeepAAA_Sky"), None)
        if sky is None:
            sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3500))
            sky.set_actor_label("DeepAAA_Sky")
            sky.light_component.set_intensity(1.2)
            sky.light_component.set_editor_property("real_time_capture", True)
        for label, pitch, yaw in ((key_label, -38, -55), (fill_label, -28, 125)):
            if label not in created_labels:
                continue
            light = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
            if light:
                light.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw), False)


def capture(gate, view):
    cfg = CONFIG[gate]
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(cfg["level"])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == cfg["label"])
    _ensure_preview_lighting(actors)
    origin, extent = building.get_actor_bounds(False)
    width = max(extent.x * 2.0, 1.0)
    height = max(extent.z * 2.0, 1.0)
    sign = 1.0 if view in ("front", "close") else -1.0
    scale = 0.60 if view == "close" else 1.0
    if gate == "Zhengximen":
        target = unreal.Vector(origin.x, origin.y, origin.z + height * 0.035)
        camera = target + unreal.Vector(width * 0.15 * sign * scale, width * 0.55 * sign * scale, height * 0.18 * scale)
    else:
        target = unreal.Vector(origin.x, origin.y, origin.z + height * 0.04)
        camera = target + unreal.Vector(width * 0.17 * sign * scale, width * 0.42 * sign * scale, height * 0.16 * scale)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / (cfg["name"] + "-deep-" + view + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(
        1600,
        1000,
        str(output),
        None,
        False,
        False,
        unreal.ComparisonTolerance.LOW,
        gate + " Deep AAA visual validation " + view,
        2.0,
        True,
    )
    # HighResShot is queued asynchronously by the editor. Keep the preview
    # level loaded until the file is committed so the next capture cannot
    # contaminate this output path.
    time.sleep(5.0)
    print("GUANGZHOU_DEEP_AAA_CAPTURE_REQUESTED", json.dumps({"gate": gate, "view": view, "output": str(output), "camera": [camera.x, camera.y, camera.z], "target": [target.x, target.y, target.z]}))


if "REQUESTED_GATE" in globals():
    capture(REQUESTED_GATE, REQUESTED_VIEW)
