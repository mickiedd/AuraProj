"""List the SceneCaptureComponent2D properties the capture scripts try to set.

CaptureGuangzhouLandmarkShowcase.py sets three properties inside a try/except:

    capture_every_frame = False
    b_always_persist_rendering_state = True
    b_capture_on_construction = False

The persistence one raises - it does not exist on SceneCaptureComponent2D in this
build - and the try/except swallows that, so the script has been reporting a
configuration it never applied. This probe prints the real names so the capture
scripts can be corrected against them instead of against a guess.

Read-only: nothing spawned, moved or saved.
"""

import unreal

component = unreal.SceneCaptureComponent2D
names = [name for name in dir(component) if not name.startswith("_")]
for needle in ("persist", "capture", "every", "movement", "post_process",
               "texture", "fov", "show", "source"):
    matches = sorted(name for name in names if needle in name.lower())
    print("PROBE_MATCH", needle, matches)

print("PROBE_HAS_CAPTURE_EVERY_FRAME", hasattr(component, "capture_every_frame"))
print("PROBE_HAS_CAPTURE_ON_MOVEMENT", hasattr(component, "capture_on_movement"))
print("PROBE_HAS_ALWAYS_PERSIST", hasattr(component, "b_always_persist_rendering_state"))
print("PROBE_HAS_POST_PROCESS_SETTINGS", hasattr(component, "post_process_settings"))
print("PROBE_HAS_POST_PROCESS_BLEND_WEIGHT", hasattr(component, "post_process_blend_weight"))

# The same question for the light component, since the new landmark-light script
# relies on these names too.
for attribute in ("intensity", "attenuation_radius", "outer_cone_angle",
                  "inner_cone_angle", "cast_shadows", "source_radius",
                  "use_temperature", "temperature", "intensity_units",
                  "affects_world", "mobility"):
    print("PROBE_SPOT_PROPERTY", attribute,
          hasattr(unreal.SpotLightComponent, attribute))
