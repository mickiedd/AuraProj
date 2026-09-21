"""Report the raw value range returned by RenderingLibrary.read_render_target.

The captured images looked blown out, but hiding the ground plane drops the mean
pixel value from 40.05 to 0.75 - so the ground is not white, and the PPM writer
may be saturating. read_render_target is called with normalize=True; this checks
whether the returned Colors are 0..1 or 0..255 so the writer scales correctly.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH

    report = {
        "rt_size": [int(render_target.get_editor_property("size_x")),
                    int(render_target.get_editor_property("size_y"))],
    }

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), unreal.Vector(0, -42000, 26000),
        unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0), transient=False)
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", 60.0)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    try:
        component.capture_scene()
        for normalize in (True, False):
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, normalize)
            values = []
            for color in colors[:20000]:
                values.append(float(color.r))
                values.append(float(color.g))
                values.append(float(color.b))
            values.sort()
            report["normalize_{}".format(normalize)] = {
                "pixel_count": len(colors),
                "min": round(values[0], 5),
                "median": round(values[len(values) // 2], 5),
                "p90": round(values[int(len(values) * 0.9)], 5),
                "max": round(values[-1], 5),
                "over_1": sum(1 for value in values if value > 1.0),
                "over_255": sum(1 for value in values if value > 255.0),
            }
    finally:
        actor_subsystem.destroy_actor(capture)

    unreal.log("SHOWCASE_RT_RANGE " + json.dumps(report))
    print("SHOWCASE_RT_RANGE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
