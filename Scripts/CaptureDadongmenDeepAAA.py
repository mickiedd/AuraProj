"""Deterministic Dadongmen Deep AAA front/close/rear captures."""
from __future__ import annotations

import json
import time
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V4/DeepAAA"
LEVEL = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/L_Dadongmen_V4_Preview"
LABEL = "Preview_Dadongmen_V4"


def capture(view):
    assert view in ("front", "close", "rear"), view
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    building = next(actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == LABEL)
    lights = {actor.get_actor_label(): actor for actor in actors.get_all_level_actors()}
    assert "V4_Preview_Key" in lights and "V4_Preview_Fill" in lights, "preview lighting missing"
    origin, extent = building.get_actor_bounds(False)
    width = max(extent.x * 2.0, 1.0)
    depth = max(extent.y * 2.0, 1.0)
    height = max(extent.z * 2.0, 1.0)
    sign = -1.0 if view in ("front", "close") else 1.0
    scale = 0.58 if view == "close" else 1.0
    # The imported source pivot is below the visual centre and the additive
    # detail mesh has a taller world-space bounds envelope.  Use a stable
    # frame-relative aim for each review view so the full gate, threshold, and
    # roof silhouette are all visible instead of letting the largest bounds
    # component drive the composition.
    target = unreal.Vector(origin.x, origin.y, origin.z + height * (0.08 if view == "close" else 0.18))
    camera = target + unreal.Vector(width * 0.14 * scale, sign * depth * 0.78 * scale, height * 0.12 * scale)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    ROOT.mkdir(parents=True, exist_ok=True)
    output = ROOT / ("Dadongmen_V4-deep-" + view + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, "Dadongmen Deep AAA " + view, 2.0, True)
    time.sleep(5.0)
    print("DADONGMEN_DEEP_AAA_CAPTURE_REQUESTED", json.dumps({"view": view, "output": str(output), "camera": [camera.x, camera.y, camera.z], "target": [target.x, target.y, target.z]}))


if "REQUESTED_VIEW" in globals():
    capture(REQUESTED_VIEW)
