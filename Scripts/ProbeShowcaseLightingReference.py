"""Dump the lighting and post-process setup of an existing project preview level.

The showcase level renders blown out - the ground plane clips to pure white even
with a mid-grey material, which points at exposure rather than albedo. Rather
than guess at light intensities, this reads the lighting the project already
uses in its own landmark preview levels, plus the engine defaults for a freshly
spawned light, so the showcase can match the established look.

Read-only apart from loading a level and destroying transient probe actors.
"""

import json

import unreal

PREVIEW_LEVELS = [
    "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/L_Xiaobeimen_AAA_V3_Preview",
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/L_Wuxianmen_V5_4K_Core_Preview",
]


def read(object_or_component, attribute):
    try:
        value = object_or_component.get_editor_property(attribute)
    except Exception:
        return None
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    try:
        return float(value)
    except Exception:
        return str(value)


LIGHT_ATTRS = {
    "DirectionalLightComponent": [
        "intensity", "light_color", "mobility", "atmosphere_sun_light",
        "use_temperature", "temperature", "cast_shadows", "indirect_lighting_intensity"],
    "SkyLightComponent": [
        "intensity", "real_time_capture", "mobility", "lower_hemisphere_is_black",
        "source_type", "sky_distance_threshold", "indirect_lighting_intensity"],
    "SkyAtmosphereComponent": [
        "rayleigh_scattering_scale", "mie_scattering_scale", "bottom_radius",
        "atmosphere_height", "multi_scattering_factor", "aerial_perspective_view_distance_km"],
    "ExponentialHeightFogComponent": [
        "fog_density", "fog_height_falloff", "fog_inscattering_luminance",
        "start_distance", "directional_inscattering_luminance", "fog_max_opacity"],
}


def describe_actor(actor):
    entry = {"label": actor.get_actor_label(), "class": actor.get_class().get_name()}
    for component in actor.get_components_by_class(unreal.SceneComponent):
        class_name = component.get_class().get_name()
        if class_name not in LIGHT_ATTRS:
            continue
        entry["component"] = class_name
        entry["settings"] = {name: read(component, name) for name in LIGHT_ATTRS[class_name]}
    return entry


def describe_post_process(actor):
    entry = {"label": actor.get_actor_label(), "class": actor.get_class().get_name()}
    try:
        entry["unbound"] = bool(actor.get_editor_property("unbound"))
    except Exception:
        pass
    try:
        settings = actor.get_editor_property("settings")
    except Exception as exc:
        entry["settings"] = "unreadable: {}".format(exc)
        return entry
    keys = [name for name in dir(settings) if "exposure" in name.lower()]
    entry["settings"] = {}
    for name in keys:
        entry["settings"][name] = read(settings, name)
    return entry


def main():
    report = {}

    # Engine defaults, for comparison.
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    defaults = []
    for actor_class in (unreal.DirectionalLight.static_class(),
                        unreal.SkyLight.static_class(),
                        unreal.SkyAtmosphere.static_class(),
                        unreal.ExponentialHeightFog.static_class(),
                        unreal.PostProcessVolume.static_class()):
        actor = actor_subsystem.spawn_actor_from_class(
            actor_class, unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=True)
        if actor is None:
            continue
        defaults.append(describe_actor(actor)
                        if "PostProcess" not in actor_class.get_name()
                        else describe_post_process(actor))
        actor_subsystem.destroy_actor(actor)
    report["engine_defaults"] = defaults

    report["preview_levels"] = []
    for path in PREVIEW_LEVELS:
        loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path)
        entry = {"level": path, "loaded": bool(loaded), "actors": []}
        if loaded:
            for actor in actor_subsystem.get_all_level_actors():
                class_name = actor.get_class().get_name()
                if any(token in class_name for token in
                       ("Light", "SkyAtmosphere", "Fog", "PostProcess", "SkySphere")):
                    if "PostProcess" in class_name:
                        entry["actors"].append(describe_post_process(actor))
                    else:
                        entry["actors"].append(describe_actor(actor))
            try:
                entry["world_settings"] = {
                    "kill_z": float(unreal.EditorLevelLibrary.get_editor_world()
                                    .get_world_settings().get_editor_property("kill_z")),
                }
            except Exception:
                pass
        report["preview_levels"].append(entry)

    unreal.log("SHOWCASE_LIGHTING_REFERENCE " + json.dumps(report))
    print("SHOWCASE_LIGHTING_REFERENCE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
