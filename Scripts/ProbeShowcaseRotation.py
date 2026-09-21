"""Check whether actor rotation and location actually stick on these landmarks.

The placement manifest recorded the yaw it intended, but reading the saved level
back gives 0 for landmarks that were rotated. Location did stick. This applies a
known rotation and reads it back through several routes to find out which one is
authoritative, and whether the actor class rejects rotation.

Read-only apart from a rotation it restores.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
TARGET = "Landmark_Guidemen_V5_4K"


def rotations(actor):
    out = {}
    try:
        rotator = actor.get_actor_rotation()
        out["get_actor_rotation"] = [round(float(rotator.roll), 3),
                                     round(float(rotator.pitch), 3),
                                     round(float(rotator.yaw), 3)]
    except Exception as exc:
        out["get_actor_rotation"] = "failed: {}".format(exc)
    try:
        transform = actor.get_actor_transform()
        rotator = transform.rotation.rotator()
        out["get_actor_transform"] = [round(float(rotator.roll), 3),
                                      round(float(rotator.pitch), 3),
                                      round(float(rotator.yaw), 3)]
    except Exception as exc:
        out["get_actor_transform"] = "failed: {}".format(exc)
    try:
        rotator = actor.root_component.get_world_rotation()
        out["root_world_rotation"] = [round(float(rotator.roll), 3),
                                      round(float(rotator.pitch), 3),
                                      round(float(rotator.yaw), 3)]
    except Exception as exc:
        out["root_world_rotation"] = "failed: {}".format(exc)
    try:
        out["root_mobility"] = str(actor.root_component.get_editor_property("mobility"))
    except Exception as exc:
        out["root_mobility"] = "failed: {}".format(exc)
    for attribute in ("b_lock_location", "b_lock_rotation", "actor_rotation"):
        try:
            out[attribute] = str(actor.get_editor_property(attribute))
        except Exception:
            pass
    return out


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    actor = next((item for item in actors if item.get_actor_label() == TARGET), None)
    assert actor, TARGET

    report = {"actor_class": actor.get_class().get_name(),
              "before": rotations(actor)}

    # Try each rotation route in turn and read back.
    attempts = []
    for name, apply in (
        ("set_actor_rotation", lambda: actor.set_actor_rotation(unreal.Rotator(0.0, 45.0, 0.0), False)),
        ("set_actor_rotation_sweep", lambda: actor.set_actor_rotation(unreal.Rotator(0.0, 90.0, 0.0), True)),
        ("root_set_world_rotation",
         lambda: actor.root_component.set_world_rotation(unreal.Rotator(0.0, 135.0, 0.0), False, False)),
        ("k2_set_actor_rotation",
         lambda: actor.k2_set_actor_rotation(unreal.Rotator(0.0, 20.0, 0.0), False)),
    ):
        try:
            apply()
            attempts.append({"route": name, "after": rotations(actor)})
        except Exception as exc:
            attempts.append({"route": name, "error": str(exc)})
    report["attempts"] = attempts

    unreal.log("ROTATION_PROBE " + json.dumps(report))
    print("ROTATION_PROBE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
