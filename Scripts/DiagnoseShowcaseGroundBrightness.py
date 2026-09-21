"""Isolate why the showcase ground plane renders pure white.

Changing its material to a dark 0.165-albedo surface did not change the render,
so albedo is not the cause. This captures the same view under three conditions
in one pass - as built, with the ground component forced movable, and with the
ground hidden entirely - to identify which factor produces the white surface.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
GROUND_LABEL = "Showcase_Ground"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")

WIDTH, HEIGHT = 1600, 900  # must match the existing render target size
CAMERA_LOCATION = unreal.Vector(0, -42000, 26000)
CAMERA_ROTATION = unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0)
FOV = 60.0


def write_ppm(path, colors):
    buffer = bytearray()
    for row in range(HEIGHT - 1, -1, -1):
        base = row * WIDTH
        for column in range(WIDTH):
            color = colors[base + column]
            buffer.append(min(255, max(0, int(float(color.r) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.g) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.b) * 255.0))))
    with open(path, "wb") as handle:
        handle.write("P6\n{} {}\n255\n".format(WIDTH, HEIGHT).encode("ascii"))
        handle.write(bytes(buffer))


def mean_brightness(colors):
    total = 0.0
    for color in colors:
        total += float(color.r) + float(color.g) + float(color.b)
    return total / (3.0 * len(colors))


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    ground = next((a for a in actor_subsystem.get_all_level_actors()
                   if a.get_actor_label() == GROUND_LABEL), None)
    assert ground, GROUND_LABEL
    ground_component = ground.get_component_by_class(unreal.StaticMeshComponent)

    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []

    def shoot(name):
        capture.set_actor_location(CAMERA_LOCATION, False, True)
        capture.set_actor_rotation(CAMERA_ROTATION, False)
        component.capture_scene()
        colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
        if len(colors) != WIDTH * HEIGHT:
            results.append({"condition": name, "error": "bad pixel count"})
            return
        path = OUT_DIR / "diag-{}.ppm".format(name)
        write_ppm(path, colors)
        results.append({"condition": name, "file": str(path),
                        "mean_brightness": round(mean_brightness(colors), 4)})

    try:
        report = {
            "ground_material": (ground_component.get_material(0).get_path_name()
                                if ground_component.get_material(0) else None),
            "ground_mobility": str(ground_component.get_editor_property("mobility")),
        }
        shoot("asbuilt")

        ground_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        shoot("movable")

        ground.set_actor_hidden_in_game(True)
        ground_component.set_visibility(False, True)
        shoot("hidden")

        # restore
        ground_component.set_visibility(True, True)
        ground.set_actor_hidden_in_game(False)
    finally:
        actor_subsystem.destroy_actor(capture)

    report["results"] = results
    report["level_saved"] = False
    unreal.log("SHOWCASE_GROUND_DIAGNOSTIC " + json.dumps(report))
    print("SHOWCASE_GROUND_DIAGNOSTIC", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
