"""Find a mechanism that actually controls exposure on the SceneCapture2D path.

Why this exists: BracketShowcaseLandmarkLight.py pinned exposure with a runtime
unbound post-process volume at priority 10 and captured EV100 3.0, 4.0, 5.0 and
6.0. An 8x change in exposure must change the image, but all four frames came back
with identical statistics (mean 222.51 / 240.99, clipped 78.91% / 93.07% - the
same numbers to two decimal places). The post-process volume is therefore NOT
reaching the scene capture, which means every capture this project has taken
through SceneCapture2D has rendered at whatever exposure the capture component
happens to use, not at the EV100 the scripts believed they were setting.

This probe measures both candidate mechanisms on the same view and reports
whether each one actually moves the image:

  A. an unbound PostProcessVolume at priority 10   (what the existing scripts do)
  B. the capture component's own PostProcessSettings

A mechanism "works" only if its two exposures produce different means. The result
is reported as numbers, not as an opinion, because the whole point is that the
previous mechanism looked plausible and did nothing.

Read-only: nothing is saved, temporary actors are destroyed, and the level is
reloaded from disk at the end.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLights")

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(6295, 953, 400)
CAMERA_ROTATION = unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0)
FOV = 60.0

# Two exposures far enough apart that any working mechanism must separate them.
TEST_EXPOSURES = [3.0, 7.0]


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
    total = 0.0
    clipped = 0
    for color in colors:
        value = (float(color.r) + float(color.g) + float(color.b)) / 3.0
        total += value
        if value >= 0.98:
            clipped += 1
    count = len(colors)
    return {"mean": round(total / count, 5),
            "clipped_pct": round(clipped / count * 100.0, 3)}


def exposure_settings(settings, ev100):
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", ev100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", ev100)):
        settings.set_editor_property(attribute, value)
    return settings


def main():
    report = {"level": LEVEL_PATH, "test_exposures": TEST_EXPOSURES, "results": []}
    report["has_component_post_process_settings"] = hasattr(
        unreal.SceneCaptureComponent2D, "post_process_settings")
    report["has_component_post_process_blend_weight"] = hasattr(
        unreal.SceneCaptureComponent2D, "post_process_blend_weight")

    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    before = len(actor_subsystem.get_all_level_actors())

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_ExposureProbe")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_ExposureProbeCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    for attribute, value in (("capture_every_frame", False),
                             ("b_always_persist_rendering_state", True),
                             ("b_capture_on_construction", False)):
        try:
            component.set_editor_property(attribute, value)
        except Exception:
            pass

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        def shoot(tag):
            # Warm-up frame discarded: exposure changes need a frame to land.
            component.capture_scene()
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            assert len(colors) == WIDTH * HEIGHT, len(colors)
            path = OUT_DIR / "probe-{}.ppm".format(tag)
            write_ppm(path, colors)
            entry = {"tag": tag, "file": str(path)}
            entry.update(stats(colors))
            report["results"].append(entry)
            print("EXPOSURE_PROBE", json.dumps(entry))

        for ev100 in TEST_EXPOSURES:
            # A: the mechanism every existing capture script uses.
            settings = volume.get_editor_property("settings")
            volume.set_editor_property("settings", exposure_settings(settings, ev100))
            shoot("volumeA-ev{:02d}".format(int(round(ev100))))

        # Reset the volume out of the way, then try B.
        volume.set_editor_property("unbound", False)
        for ev100 in TEST_EXPOSURES:
            settings = component.get_editor_property("post_process_settings")
            component.set_editor_property(
                "post_process_settings", exposure_settings(settings, ev100))
            try:
                component.set_editor_property("post_process_blend_weight", 1.0)
            except Exception as exc:
                print("EXPOSURE_PROBE_BLEND_FAILED", repr(exc))
            shoot("componentB-ev{:02d}".format(int(round(ev100))))
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    after = len(actor_subsystem.get_all_level_actors())
    report["leaked_actors"] = leaked
    report["actor_count_before"] = before
    report["actor_count_after"] = after
    assert not leaked, leaked
    assert before == after, (before, after)

    # Verdict, computed rather than asserted by eye: a mechanism works only if its
    # two exposures separate.
    by_tag = {entry["tag"]: entry["mean"] for entry in report["results"]}
    report["verdict"] = {}
    for name, prefix in (("unbound_post_process_volume", "volumeA"),
                         ("capture_component_post_process", "componentB")):
        low = by_tag.get("{}-ev03".format(prefix))
        high = by_tag.get("{}-ev07".format(prefix))
        if low is None or high is None:
            report["verdict"][name] = "not measured"
            continue
        report["verdict"][name] = {
            "mean_ev03": low, "mean_ev07": high,
            "difference": round(low - high, 5),
            "controls_exposure": abs(low - high) > 0.005,
        }

    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    report["level_reloaded_from_disk"] = True

    (OUT_DIR / "exposure-probe-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_EXPOSURE_PROBE " + json.dumps(report))
    print("SHOWCASE_EXPOSURE_PROBE_VERDICT", json.dumps(report["verdict"], indent=2))


if __name__ == "__main__":
    main()
