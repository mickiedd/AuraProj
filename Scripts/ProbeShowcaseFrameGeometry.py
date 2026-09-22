"""Identify, by ray trace, what geometry is in the bright blue region of the frame.

The pixel probe was inconclusive: replacing the ground plane's material with a neutral
engine material left the blue region's mean colour unchanged (r 0.2396 / g 0.2571 /
b 0.2868 against 0.2393 / 0.2573 / 0.2883), and moving the ground plane 500 m away
left it unchanged again. Either the region is not the ground plane, or those captures
were not re-rendered. Pixels cannot tell the two apart; a trace can.

This traces rays from the capture camera along the directions that correspond to
specific rows of the frame and reports what each one hits, so the question "what is
that blue" gets an answer that does not depend on the capture harness at all.

Read-only: nothing spawned, moved or saved.
"""

import json
import math

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(-4965, 606, 400)
CAMERA_YAW = 173.05
CAMERA_PITCH = 2.0
HORIZONTAL_FOV = 60.0
TRACE_LENGTH_CM = 200000.0

# Rows of the frame to trace, with what the earlier histogram said about them.
ROWS = [
    ("sky", 130),
    ("horizon_band", 300),
    ("building_mid", 380),
    ("just_below_building", 510),
    ("ground_band", 560),
    ("ground_near", 700),
    ("ground_bottom", 860),
]


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL_PATH), world

    vertical_half_fov = math.degrees(math.atan(
        math.tan(math.radians(HORIZONTAL_FOV / 2.0)) * HEIGHT / float(WIDTH)))
    print("TRACE_VERTICAL_HALF_FOV_DEG", round(vertical_half_fov, 3))

    report = {"level": LEVEL_PATH, "camera_location": [CAMERA_LOCATION.x, CAMERA_LOCATION.y,
                                                       CAMERA_LOCATION.z],
              "camera_yaw": CAMERA_YAW, "camera_pitch": CAMERA_PITCH,
              "vertical_half_fov_deg": round(vertical_half_fov, 3), "rays": []}

    for name, row in ROWS:
        # Fraction of the half-height below the optical axis, then the angle.
        offset = (row - HEIGHT / 2.0) / (HEIGHT / 2.0)
        pitch = CAMERA_PITCH - math.degrees(
            math.atan(offset * math.tan(math.radians(vertical_half_fov))))
        yaw = math.radians(CAMERA_YAW)
        pitch_rad = math.radians(pitch)
        direction = unreal.Vector(
            math.cos(pitch_rad) * math.cos(yaw),
            math.cos(pitch_rad) * math.sin(yaw),
            math.sin(pitch_rad))
        end = unreal.Vector(CAMERA_LOCATION.x + direction.x * TRACE_LENGTH_CM,
                            CAMERA_LOCATION.y + direction.y * TRACE_LENGTH_CM,
                            CAMERA_LOCATION.z + direction.z * TRACE_LENGTH_CM)
        hit = unreal.SystemLibrary.line_trace_single(
            world, CAMERA_LOCATION, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            True, [], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            # This build returns None rather than an empty HitResult on a miss,
            # which is itself the answer: nothing is there.
            report["rays"].append({"row": row, "name": name,
                                   "ray_pitch_deg": round(pitch, 3),
                                   "blocked": False, "note": "no geometry hit"})
            print("TRACE_RAY", json.dumps(report["rays"][-1]))
            continue
        values = hit.to_tuple()
        blocked = bool(values[0])
        # HitResult fields are protected in this build, so the tuple is indexed.
        # Order: blocking_hit, initial_overlap, time, distance, location,
        # impact_point, normal, impact_normal, phys_material, hit_actor,
        # hit_component, hit_bone_name, trace_start, trace_end, face_index.
        # (Index 8 is the PhysicalMaterial, not the actor - reading it as the actor
        # is what made the first run of this probe fail.)
        location = values[4] if len(values) > 4 else None
        actor = values[9] if len(values) > 9 else None
        entry = {"row": row, "name": name, "ray_pitch_deg": round(pitch, 3),
                 "blocked": blocked, "tuple_length": len(values)}
        if blocked and actor is not None and hasattr(actor, "get_actor_label"):
            entry["hit_actor"] = actor.get_actor_label()
            entry["hit_class"] = actor.get_class().get_name()
            if location is not None:
                entry["hit_distance_cm"] = round(math.dist(
                    (CAMERA_LOCATION.x, CAMERA_LOCATION.y, CAMERA_LOCATION.z),
                    (location.x, location.y, location.z)), 2)
                entry["hit_z_cm"] = round(float(location.z), 2)
        report["rays"].append(entry)
        print("TRACE_RAY", json.dumps(entry))

    # Sanity: the ground must be directly under the camera if it is there at all.
    down = unreal.SystemLibrary.line_trace_single(
        world, CAMERA_LOCATION,
        unreal.Vector(CAMERA_LOCATION.x, CAMERA_LOCATION.y,
                      CAMERA_LOCATION.z - TRACE_LENGTH_CM),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
        unreal.DrawDebugTrace.NONE, True)
    if down is None:
        report["straight_down"] = {"blocked": False, "note": "no geometry hit"}
    else:
        values = down.to_tuple()
        under = {"blocked": bool(values[0]), "tuple_length": len(values)}
        if values[0] and len(values) > 9 and hasattr(values[9], "get_actor_label"):
            under["hit_actor"] = values[9].get_actor_label()
            under["hit_class"] = values[9].get_class().get_name()
        report["straight_down"] = under
    print("TRACE_STRAIGHT_DOWN", json.dumps(under))

    print("TRACE_REPORT", json.dumps(report))


if __name__ == "__main__":
    main()
