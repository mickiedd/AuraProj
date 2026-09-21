"""Frame the same landmark in two levels and compare the renders.

The previous A/B framed each level from its whole content bounds, which is
dominated by the ground plane, so both images were mostly floor. This frames the
SAME asset - Xiaobeimen AAA V3 - tightly in both the level it ships with
(`L_Xiaobeimen_AAA_V3_Preview`) and the new showcase level, from the front, so the
two renders are directly comparable. If the asset renders lit in its preview level
and unlit in the showcase level, the showcase level's lighting is at fault; if both
look the same, it is the capture setup.

Read-only; no level is saved and all temporary actors are destroyed.
"""

import json
import math
from pathlib import Path

import unreal

RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")
WIDTH, HEIGHT = 1600, 900
FOV = 55.0

# (name, level path, actor label to frame on)
TARGETS = [
    ("preview", "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/"
                "L_Xiaobeimen_AAA_V3_Preview", "Preview_Xiaobeimen_AAA_V3"),
    ("showcase", "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase",
     "Landmark_Xiaobeimen_AAA_V3"),
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
    return {"mean": round(total / count, 4),
            "histogram_pct": [round(b / count * 100.0, 1) for b in buckets],
            "occupied_buckets": occupied,
            "continuous": occupied >= 5}


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    report = {"targets": []}
    for name, level_path, label in TARGETS:
        entry = {"name": name, "level": level_path, "actor_label": label}
        if not level_editor.load_level(level_path):
            entry["error"] = "load failed"
            report["targets"].append(entry)
            continue
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        actor = next((a for a in actor_subsystem.get_all_level_actors()
                      if a.get_actor_label() == label), None)
        if actor is None:
            entry["error"] = "actor not found; labels present: " + ", ".join(
                a.get_actor_label() for a in actor_subsystem.get_all_level_actors())
            report["targets"].append(entry)
            continue

        origin, extent = actor.get_actor_bounds(False)
        radius = max(float(extent.x), float(extent.y), float(extent.z))
        distance = radius * 3.0
        height = radius * 1.3
        location = unreal.Vector(float(origin.x), float(origin.y) - distance,
                                 float(origin.z) + height * 0.4)
        pitch = -math.degrees(math.atan2(height, distance))
        rotation = unreal.Rotator(pitch=pitch, yaw=90.0, roll=0.0)
        entry["actor_bounds_centre"] = [round(float(origin.x), 1), round(float(origin.y), 1),
                                        round(float(origin.z), 1)]
        entry["actor_radius"] = round(radius, 1)
        entry["camera"] = {"location": [round(location.x, 1), round(location.y, 1),
                                        round(location.z, 1)],
                           "pitch": round(pitch, 2), "yaw": 90.0}

        capture = actor_subsystem.spawn_actor_from_class(
            unreal.SceneCapture2D.static_class(), location, rotation, transient=False)
        capture.set_actor_label("TEMP_Tight_Capture")
        component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
        component.set_editor_property("texture_target", render_target)
        component.set_editor_property("fov_angle", FOV)
        try:
            component.set_editor_property(
                "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        except Exception:
            pass
        try:
            capture.set_actor_location(location, False, True)
            capture.set_actor_rotation(rotation, False)
            component.capture_scene()
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            if len(colors) != WIDTH * HEIGHT:
                entry["error"] = "bad pixel count {}".format(len(colors))
            else:
                ppm = OUT_DIR / "tight-{}.ppm".format(name)
                write_ppm(ppm, colors)
                entry["file"] = str(ppm)
                entry.update(stats(colors))
        finally:
            actor_subsystem.destroy_actor(capture)
        report["targets"].append(entry)
        print("TIGHT_CAPTURE", json.dumps(entry))

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report["leaked"] = leaked
    report["level_saved"] = False
    (OUT_DIR / "tight-capture.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("TIGHT_CAPTURE_REPORT " + json.dumps(report))
    print("TIGHT_CAPTURE_REPORT", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
