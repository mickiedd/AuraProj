"""Read back the showcase level's post-process exposure settings.

The level was rebuilt with a pinned EV100 of 3.5, but its captures still look
auto-exposed. `get_editor_property("settings")` returns a struct copy in UE
Python, so the modified struct must be written back; this checks whether the
override actually persisted into the saved level.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
LABEL = "Showcase_PostProcess"

KEYS = ("override_auto_exposure_min_brightness", "auto_exposure_min_brightness",
        "override_auto_exposure_max_brightness", "auto_exposure_max_brightness",
        "override_auto_exposure_method", "auto_exposure_method",
        "override_auto_exposure_bias", "auto_exposure_bias")

loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
assert loaded, LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

report = {"level": LEVEL_PATH, "post_process_actors": []}
for actor in actors:
    if "PostProcess" not in actor.get_class().get_name():
        continue
    entry = {"label": actor.get_actor_label(),
             "class": actor.get_class().get_name(),
             "name": actor.get_name()}
    for attribute in ("unbound", "priority", "enabled"):
        try:
            value = actor.get_editor_property(attribute)
            entry[attribute] = float(value) if isinstance(value, (int, float)) else value
        except Exception:
            pass
    try:
        settings = actor.get_editor_property("settings")
        entry["settings"] = {}
        for key in KEYS:
            try:
                value = settings.get_editor_property(key)
                entry["settings"][key] = (float(value) if isinstance(value, (int, float))
                                          else str(value))
            except Exception as exc:
                entry["settings"][key] = "unreadable: {}".format(exc)
    except Exception as exc:
        entry["settings"] = "unreadable: {}".format(exc)
    report["post_process_actors"].append(entry)

unreal.log("SHOWCASE_PPV_READBACK " + json.dumps(report))
print("SHOWCASE_PPV_READBACK", json.dumps(report, indent=2))
