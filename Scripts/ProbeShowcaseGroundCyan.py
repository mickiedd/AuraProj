"""Find out what is rendering the ground plane cyan in the showcase captures.

The showcase ground plane is supposed to be neutral dark paving (M_ShowcaseGround,
base colour 0.165, roughness 0.88, authored by CreateShowcaseGroundMaterial.py). In
the new lights-on/off captures the whole lower half of the frame comes back bright
cyan. The archived 2026-09-21 captures do not show cyan, but they are blown so hard
that a cyan surface would clip to white there, so their silence proves nothing.

This probe captures the same view in three states and reports the mean RGB of three
regions of the frame in each, so the answer is a measurement rather than a guess:

  A. as-is
  B. ground material temporarily replaced with a known neutral engine material
  C. ground plane temporarily moved far away, so whatever is behind it shows

If the cyan survives B it is not the material; if it survives C it is not the ground
plane at all. Everything is restored, the level is never saved, and the level is
reloaded from disk at the end.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLights")

WIDTH, HEIGHT = 1600, 900
EXPOSURE_EV100 = 4.0            # from the bracket: the least blown usable step
GROUND_LABEL = "Showcase_Ground"
NEUTRAL_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial"
CAMERA_LOCATION = unreal.Vector(-4965, 606, 400)
CAMERA_ROTATION = unreal.Rotator(pitch=2.0, yaw=173.05, roll=0.0)
FOV = 60.0
GROUND_LIFT_CM = 50000.0

# Regions of the frame, in image coordinates with row 0 at the TOP. read_render_target
# returns rows bottom-up, so rows are flipped when sampling.
REGIONS = {
    "ground_near": (560, 840, 700, 900),
    "ground_far": (480, 545, 700, 900),
    "sky": (60, 200, 700, 900),
    "building": (240, 430, 380, 620),
}


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


def region_stats(colors, box):
    """Mean RGB (clamped 0..1) and mean luminance over one image-space box."""
    top, bottom, left, right = box
    total = [0.0, 0.0, 0.0]
    count = 0
    for image_row in range(top, bottom):
        row = HEIGHT - 1 - image_row          # read_render_target is bottom-up
        base = row * WIDTH
        for column in range(left, right):
            color = colors[base + column]
            total[0] += min(1.0, max(0.0, float(color.r)))
            total[1] += min(1.0, max(0.0, float(color.g)))
            total[2] += min(1.0, max(0.0, float(color.b)))
            count += 1
    mean = [round(value / count, 5) for value in total]
    return {"r": mean[0], "g": mean[1], "b": mean[2],
            "luminance": round(sum(mean) / 3.0, 5),
            "rg_diff": round(mean[0] - mean[1], 5),
            "gb_diff": round(mean[1] - mean[2], 5)}


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    before = len(actor_subsystem.get_all_level_actors())

    ground = [actor for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label() == GROUND_LABEL]
    assert len(ground) == 1, "expected 1 {}, found {}".format(GROUND_LABEL, len(ground))
    ground = ground[0]
    ground_component = ground.get_component_by_class(unreal.StaticMeshComponent)
    assert ground_component, "ground StaticMeshComponent"
    original_material = ground_component.get_material(0)
    original_location = ground.get_actor_location()
    neutral = unreal.EditorAssetLibrary.load_asset(NEUTRAL_MATERIAL)

    report = {"level": LEVEL_PATH, "exposure_ev100": EXPOSURE_EV100,
              "ground_material_assigned": original_material.get_path_name()
              if original_material else None,
              "neutral_material": NEUTRAL_MATERIAL if neutral else "NOT FOUND",
              "regions": {name: list(box) for name, box in REGIONS.items()},
              "states": []}

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_GroundProbeExposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", EXPOSURE_EV100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", EXPOSURE_EV100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_GroundProbeCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    component.set_editor_property("capture_source",
                                  unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)

    try:
        def shoot(tag):
            for _ in range(3):
                component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            assert len(colors) == WIDTH * HEIGHT, len(colors)
            path = OUT_DIR / "ground-{}.ppm".format(tag)
            write_ppm(path, colors)
            entry = {"state": tag, "file": str(path),
                     "regions": {name: region_stats(colors, box)
                                 for name, box in REGIONS.items()}}
            report["states"].append(entry)
            print("GROUND_PROBE", json.dumps(entry))
            return entry

        shoot("as-is")

        if neutral:
            ground_component.set_material(0, neutral)
            shoot("neutral-material")
            ground_component.set_material(0, original_material)
            assert ground_component.get_material(0).get_path_name() == \
                original_material.get_path_name(), "ground material not restored"

        ground.set_actor_location(
            unreal.Vector(original_location.x, original_location.y,
                          original_location.z - GROUND_LIFT_CM), False, True)
        shoot("ground-removed")
        ground.set_actor_location(original_location, False, True)
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    after = len(actor_subsystem.get_all_level_actors())
    assert not leaked, leaked
    assert before == after, (before, after)

    # Restore-then-reload, then prove the saved level still has the original ground.
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    ground = [actor for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label() == GROUND_LABEL][0]
    on_disk = ground.get_component_by_class(unreal.StaticMeshComponent).get_material(0)
    report["ground_material_on_disk"] = on_disk.get_path_name() if on_disk else None
    report["ground_location_on_disk"] = [
        round(float(ground.get_actor_location().x), 2),
        round(float(ground.get_actor_location().y), 2),
        round(float(ground.get_actor_location().z), 2)]
    report["level_reloaded_from_disk"] = True

    (OUT_DIR / "ground-probe-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_GROUND_PROBE " + json.dumps(report))
    print("SHOWCASE_GROUND_PROBE_MATERIAL", report["ground_material_assigned"])
    print("SHOWCASE_GROUND_PROBE_ON_DISK", report["ground_material_on_disk"])


if __name__ == "__main__":
    main()
