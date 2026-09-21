"""A/B the capture harness: showcase level vs an existing project preview level.

The showcase level's captures render the scene unlit - the ground draws at exactly
its own base colour and hiding every light leaves the image unchanged - so the
lighting could not be visually confirmed. The open question is whether that is a
property of the new level or of the capture setup in this environment.

This runs the identical capture against `L_Xiaobeimen_AAA_V3_Preview`, a level this
project has captured successfully before, framing each level from its own content
bounds so the two images are comparable. A lit scene produces a continuous
histogram; an unlit one produces a bimodal spike at the materials' base colours
plus black background.

Read-only; levels are not saved and all temporary actors are destroyed.
"""

import json
import math
from pathlib import Path

import unreal

RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")
WIDTH, HEIGHT = 1600, 900
FOV = 60.0

LEVELS = [
    ("preview", "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/"
                "L_Xiaobeimen_AAA_V3_Preview"),
    ("showcase", "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"),
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
    occupied = sum(1 for b in buckets if b > count * 0.005)
    return {
        "mean": round(total / count, 4),
        "histogram_pct": [round(b / count * 100.0, 1) for b in buckets],
        "occupied_buckets": occupied,
        "continuous": occupied >= 5,
    }


def content_bounds(actors):
    lo = [1e18] * 3
    hi = [-1e18] * 3
    for actor in actors:
        try:
            origin, extent = actor.get_actor_bounds(False)
        except Exception:
            continue
        lo[0] = min(lo[0], origin.x - extent.x); hi[0] = max(hi[0], origin.x + extent.x)
        lo[1] = min(lo[1], origin.y - extent.y); hi[1] = max(hi[1], origin.y + extent.y)
        lo[2] = min(lo[2], origin.z - extent.z); hi[2] = max(hi[2], origin.z + extent.z)
    if lo[0] > hi[0]:
        return (0.0, 0.0, 0.0), 5000.0
    centre = ((lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, (lo[2] + hi[2]) / 2.0)
    radius = max(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) / 2.0
    return centre, max(radius, 1000.0)


def light_summary(actors):
    out = []
    for actor in actors:
        class_name = actor.get_class().get_name()
        if "Light" not in class_name and "Atmosphere" not in class_name:
            continue
        entry = {"label": actor.get_actor_label(), "class": class_name}
        component = actor.get_component_by_class(unreal.LightComponent)
        if component is not None:
            try:
                entry["intensity"] = float(component.get_editor_property("intensity"))
            except Exception:
                pass
            try:
                entry["mobility"] = str(component.get_editor_property("mobility"))
            except Exception:
                pass
        out.append(entry)
    return out


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    report = {"levels": []}
    for name, path in LEVELS:
        entry = {"name": name, "level": path}
        if not level_editor.load_level(path):
            entry["error"] = "load failed"
            report["levels"].append(entry)
            continue
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        actors = actor_subsystem.get_all_level_actors()
        entry["actor_count"] = len(actors)
        entry["lights"] = light_summary(actors)

        centre, radius = content_bounds(actors)
        entry["content_centre"] = [round(v, 1) for v in centre]
        entry["content_radius"] = round(radius, 1)

        distance = radius * 2.2
        height = radius * 1.4
        camera_location = unreal.Vector(centre[0], centre[1] - distance, centre[2] + height)
        pitch = -math.degrees(math.atan2(height, distance))
        camera_rotation = unreal.Rotator(pitch=pitch, yaw=90.0, roll=0.0)
        entry["camera"] = {"location": [round(v, 1) for v in (camera_location.x,
                                                              camera_location.y,
                                                              camera_location.z)],
                           "pitch": round(pitch, 2), "yaw": 90.0, "fov": FOV}

        capture = actor_subsystem.spawn_actor_from_class(
            unreal.SceneCapture2D.static_class(), camera_location, camera_rotation,
            transient=False)
        capture.set_actor_label("TEMP_AB_Capture")
        component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
        component.set_editor_property("texture_target", render_target)
        component.set_editor_property("fov_angle", FOV)
        try:
            component.set_editor_property(
                "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        except Exception:
            pass
        try:
            # Two captures: the first gives the lights' rendering state a frame to
            # register, the second is the one kept.
            capture.set_actor_location(camera_location, False, True)
            capture.set_actor_rotation(camera_rotation, False)
            component.capture_scene()
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            if len(colors) != WIDTH * HEIGHT:
                entry["error"] = "bad pixel count {}".format(len(colors))
            else:
                ppm = OUT_DIR / "ab-{}.ppm".format(name)
                write_ppm(ppm, colors)
                entry["file"] = str(ppm)
                entry.update(stats(colors))
        finally:
            actor_subsystem.destroy_actor(capture)
        report["levels"].append(entry)
        print("AB_CAPTURE", json.dumps(entry))

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report["leaked"] = leaked
    report["level_saved"] = False
    (OUT_DIR / "ab-capture.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AB_CAPTURE_REPORT " + json.dumps(report))
    print("AB_CAPTURE_REPORT", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
