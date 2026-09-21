"""Test the showcase ground against existing, long-compiled project materials.

The ground plane renders near-white even with a dark material and is unaffected
by sky light intensity, so either the material authored for it
(M_ShowcaseGround, created minutes ago) is not compiling to what its graph says,
or the plane geometry itself is at fault. This swaps in existing project stone
materials that have been compiled for a long time and captures the same view, so
the two explanations can be told apart.

The level is not saved; the original material is restored afterwards.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")
GROUND_LABEL = "Showcase_Ground"

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(0, -42000, 26000)
CAMERA_ROTATION = unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0)
FOV = 60.0
EV100 = 1.0

CANDIDATES = [
    ("showcaseground", "/Game/Assets/Environment/GuangzhouLandmarks/M_ShowcaseGround"),
    ("stoneroad", "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/Materials/M_StoneRoad"),
    ("stonedark", "/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen/Materials/M_StoneDark"),
    ("previewground", "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround"),
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


def describe_material(path):
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        return {"path": path, "exists": False}
    entry = {"path": path, "class": material.get_class().get_name()}
    for attribute in ("shading_model", "blend_mode", "two_sided"):
        try:
            entry[attribute] = str(material.get_editor_property(attribute))
        except Exception:
            pass
    # Report which material properties have an expression connected.
    try:
        connected = []
        for prop in ("MP_BASE_COLOR", "MP_EMISSIVE_COLOR", "MP_ROUGHNESS", "MP_METALLIC"):
            try:
                expression = unreal.MaterialEditingLibrary.get_material_property_input_node(
                    material, getattr(unreal.MaterialProperty, prop))
                connected.append("{}={}".format(
                    prop, expression.get_class().get_name() if expression else None))
            except Exception:
                connected.append("{}?".format(prop))
        entry["connected"] = connected
    except Exception as exc:
        entry["connected"] = "unreadable: {}".format(exc)
    return entry


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)

    ground = next((a for a in actor_subsystem.get_all_level_actors()
                   if a.get_actor_label() == GROUND_LABEL), None)
    assert ground, GROUND_LABEL
    ground_component = ground.get_component_by_class(unreal.StaticMeshComponent)
    original = ground_component.get_material(0)

    report = {"materials": [describe_material(path) for _, path in CANDIDATES],
              "results": []}

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_MatTest")
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
    capture.set_actor_label("TEMP_MatCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        for name, path in CANDIDATES:
            material = unreal.EditorAssetLibrary.load_asset(path)
            if material is None:
                report["results"].append({"name": name, "error": "material missing"})
                continue
            ground_component.set_material(0, material)
            capture.set_actor_location(CAMERA_LOCATION, False, True)
            capture.set_actor_rotation(CAMERA_ROTATION, False)
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            if len(colors) != WIDTH * HEIGHT:
                report["results"].append({"name": name, "error": "bad pixel count"})
                continue
            ppm = OUT_DIR / "mat-{}.ppm".format(name)
            write_ppm(ppm, colors)
            entry = {"name": name, "material": path, "file": str(ppm)}
            entry.update(stats(colors))
            report["results"].append(entry)
            print("GROUND_MATERIAL_TEST", json.dumps(entry))
    finally:
        if original:
            ground_component.set_material(0, original)
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report["leaked"] = leaked
    report["level_saved"] = False
    (OUT_DIR / "ground-material-test.json").write_text(json.dumps(report, indent=2),
                                                       encoding="utf-8")
    unreal.log("SHOWCASE_GROUND_MATERIAL_TEST " + json.dumps(report))
    print("SHOWCASE_GROUND_MATERIAL_TEST", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
