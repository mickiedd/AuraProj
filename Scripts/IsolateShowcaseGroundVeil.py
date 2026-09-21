"""Isolate which actor is washing the showcase ground to white.

Every ground material - including dark project stone that has been compiled for
weeks - renders the same lower-two-thirds of the frame as pure white, and the
split does not move with sky light intensity. That is the signature of a veil
rather than shading, so this hides the candidate contributors one at a time and
captures the same view after each, to name the culprit.

The level is not saved and all visibility changes are restored.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(0, -42000, 26000)
CAMERA_ROTATION = unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0)
FOV = 60.0
EV100 = 1.0

# Actors hidden cumulatively, in order.
HIDE_ORDER = [
    "Showcase_SkyAtmosphere",
    "Showcase_SkyLight",
    "Showcase_Fill",
    "Showcase_Key",
]


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


def stats(colors):
    buckets = [0] * 8
    total = 0.0
    for color in colors:
        value = (float(color.r) + float(color.g) + float(color.b)) / 3.0
        total += value
        buckets[min(7, int(value * 8))] += 1
    count = len(colors)
    return {"mean": round(total / count, 4),
            "histogram_pct": [round(b / count * 100.0, 1) for b in buckets]}


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH

    by_label = {}
    for actor in actor_subsystem.get_all_level_actors():
        by_label[actor.get_actor_label()] = actor

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_Isolation")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", EV100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", EV100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_IsoCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass

    hidden = []
    results = []
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    def shoot(name):
        capture.set_actor_location(CAMERA_LOCATION, False, True)
        capture.set_actor_rotation(CAMERA_ROTATION, False)
        component.capture_scene()
        colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
        if len(colors) != WIDTH * HEIGHT:
            results.append({"condition": name, "error": "bad pixel count"})
            return
        ppm = OUT_DIR / "iso-{}.ppm".format(name)
        write_ppm(ppm, colors)
        entry = {"condition": name, "file": str(ppm)}
        entry.update(stats(colors))
        results.append(entry)
        print("ISOLATION", json.dumps(entry))

    try:
        shoot("all")
        for label in HIDE_ORDER:
            actor = by_label.get(label)
            if actor is None:
                results.append({"condition": "hide_" + label, "error": "actor not found"})
                continue
            actor.set_actor_hidden_in_game(True)
            hidden.append(actor)
            shoot("hide_" + label)
    finally:
        for actor in hidden:
            actor.set_actor_hidden_in_game(False)
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report = {"results": results, "leaked": leaked, "level_saved": False}
    (OUT_DIR / "isolation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_ISOLATION " + json.dumps(report))
    print("SHOWCASE_ISOLATION", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
