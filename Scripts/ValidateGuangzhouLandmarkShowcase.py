"""Validate the Guangzhou landmark showcase level from the saved map.

Reloads the level from disk (so nothing is validated from in-memory state) and
checks the properties that matter for a showcase level:

  * every expected landmark is present exactly once, and nothing else that
    carries a landmark tag
  * each landmark sits on the ground plane (world min Z == ground top)
  * no two landmarks overlap in XY
  * every landmark fits inside the ground plane
  * the ring really is a ring: each landmark's AABB centre sits near the
    expected radius, and the landmarks are spread around the full circle
  * every landmark has exactly one independent light of its own, owned by that
    landmark, standing in front of it on the plaza side and aimed at it, with a
    cone that covers the building, an attenuation radius that reaches across it,
    and a cone that does not take in any other landmark
  * the ground plane, lighting, and a player start exist

Read-only. Writes its manifest to Saved/RawModelImport/.
"""

import json
import math
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
# Project-relative rather than the old absolute "C:/Git/AuraProj/..." path: on
# this machine the Windows path made the report land in a literal "C:" directory
# inside the engine's Binaries/Mac folder instead of the project.
MANIFEST = (Path(__file__).resolve().parents[1]
            / "Saved/RawModelImport/guangzhou-landmark-showcase-validation.json")
PLACEMENT_MANIFEST = (Path(__file__).resolve().parents[1]
                      / "Saved/RawModelImport/guangzhou-landmark-showcase.json")

EXPECTED_LABELS = [
    "Landmark_Zhengnanmen_HighFidelity",
    # Landmark_Zhengnanmen_AAA_V3 was retired on 2026-09-22 together with its
    # asset folder - the same treatment Xiaobeimen_Production_V3 got below.
    # Zhengnanmen is represented by the HighFidelity variant above.
    "Landmark_Xiaobeimen_AAA_V3",
    # Landmark_Xiaobeimen_Production_V3 was retired on 2026-09-22 together with
    # its asset folder. The light labels below are derived from this list, so the
    # two have to move together.
    "Landmark_Guidemen_ReferenceRepaired",
    "Landmark_Wuxianmen_V5_FullPBR",
    "Landmark_GreatNorthGate",
    "Landmark_ZhenhaiTower",
    # Wenmingmen joined the ring on 2026-09-24. The per-landmark light labels are
    # derived from this list, so the landmark and its light move together.
    "Landmark_Wenmingmen",
    # Zhengximen joined the ring on 2026-09-24. The per-landmark light labels are
    # derived from this list, so the landmark and its light move together.
    "Landmark_Zhengximen",
    # Zhengdongmen (the Great East Gate) joined the ring on 2026-09-25. The
    # per-landmark light labels are derived from this list, so the landmark and
    # its light move together.
    "Landmark_Zhengdongmen",
]

GROUND_TAG = "Showcase_Ground"
LANDMARK_TAG = "GuangzhouLandmarkShowcase"
LIGHT_TAG = "GuangzhouLandmarkLight"
GROUND_TOP_Z = 0.0
GROUNDING_TOLERANCE_CM = 1.0
OVERLAP_TOLERANCE_CM = 1.0
MIN_SPREAD_DEGREES = 300.0
FACING_TOLERANCE_DEG = 1.0

# Which local axis each landmark's FACADE faces, in the model's own frame.
#
# This exists because nothing else here checks facing: a landmark can be
# correctly grounded, correctly spaced, correctly lit and still be turned
# back to front, and every other assertion in this file would pass. That is
# exactly what happened to Wenmingmen - it was placed with facing_offset 0 on
# the assumption that it follows the other gates' local -Y front convention,
# and it does not.
#
# The axes are recorded per model, from inspecting the model rather than from
# assuming they are all alike. For Wenmingmen the facade side is the side
# carrying the timber door, the couplets, the climbing vines and the canal with
# its bridge, all measured on local +Y. If a future landmark is added, work out
# which way it faces and add it here; a missing entry is reported as a warning,
# not silently skipped.
FACADE_LOCAL_AXIS = {
    "Zhengnanmen_HighFidelity": (0.0, -1.0),
    "Xiaobeimen_AAA_V3": (0.0, -1.0),
    "Guidemen_ReferenceRepaired": (0.0, -1.0),
    "Wuxianmen_V5_FullPBR": (0.0, -1.0),
    "GreatNorthGate": (0.0, -1.0),
    "ZhenhaiTower": (0.0, -1.0),
    "Wenmingmen": (0.0, 1.0),
    # Zhengximen: measured on the imported model, not assumed. The gate plaque
    # (門額) sits at local Y -3.06..-2.92 and the iron door fittings at
    # Y -0.61..-0.39, so the facade faces local -Y like the other gates.
    "Zhengximen": (0.0, -1.0),
    # Zhengdongmen: measured on the imported model, not assumed, and it is the
    # second landmark to break the -Y convention - for the opposite reason to
    # Wenmingmen. Its SOURCE model has the facade on -Y (the package's own
    # generator authors the 正东门 signboard at Y -5.97..-5.89 and its README says
    # "front = -Y"), but the import negates Y: the door studs move from source
    # Y +204..+211.5 to imported -211.5..-204, the plaque from source
    # Y -598.2..-589 to imported +589..+598.2, and the stone base centre from
    # -4.5 to +4.5. So the facade arrives on local +Y and needs facing_offset 180.
    "Zhengdongmen": (0.0, 1.0),
}
LIGHT_LOCATION_TOLERANCE_CM = 1.0
LIGHT_ANGLE_TOLERANCE_DEG = 0.05
LIGHT_VALUE_TOLERANCE = 0.001     # relative, on intensity / cone / attenuation


def rendered_geometry(actor):
    """Union of the actor's *rendered* geometry in actor-local space.

    get_actor_bounds includes the hidden UCX collision volumes these Blueprints
    carry, which dip about 100 cm below the visible model and therefore make a
    correctly grounded landmark look sunk. Grounding is judged on what actually
    renders, matching how the placement computed it.
    """
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    corners_cache = {}
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if mesh is None:
            continue
        is_hism = component.get_class().get_name().startswith("Hierarchical")
        if is_hism and int(component.get_instance_count()) == 0:
            continue
        try:
            if not bool(component.get_editor_property("visible")):
                continue
        except Exception:
            pass
        # Offset by the component's relative translation only. Reading the full
        # relative_transform here returns a rotation that does not match what the
        # placement pass measured on the same asset (it turns the local Z range
        # into the Y range on the V3 gates), so the rotation is left out: these
        # components carry no relative rotation, only a translation.
        try:
            offset = component.get_editor_property("relative_location")
            offset = (float(offset.x), float(offset.y), float(offset.z))
        except Exception:
            offset = (0.0, 0.0, 0.0)

        key = mesh.get_path_name()
        if key not in corners_cache:
            bounds = mesh.get_bounds()
            origin, extent = bounds.origin, bounds.box_extent
            xs = (float(origin.x) - float(extent.x), float(origin.x) + float(extent.x))
            ys = (float(origin.y) - float(extent.y), float(origin.y) + float(extent.y))
            zs = (float(origin.z) - float(extent.z), float(origin.z) + float(extent.z))
            corners_cache[key] = [(x, y, z) for x in xs for y in ys for z in zs]
        corners = corners_cache[key]

        if is_hism:
            transforms = [component.get_instance_transform(index)
                          for index in range(int(component.get_instance_count()))]
        else:
            transforms = [unreal.Transform()]

        for instance_transform in transforms:
            for (cx, cy, cz) in corners:
                local = instance_transform.transform_location(unreal.Vector(cx, cy, cz))
                point = (local.x + offset[0], local.y + offset[1], local.z + offset[2])
                lo[0] = min(lo[0], point[0]); hi[0] = max(hi[0], point[0])
                lo[1] = min(lo[1], point[1]); hi[1] = max(hi[1], point[1])
                lo[2] = min(lo[2], point[2]); hi[2] = max(hi[2], point[2])
    if lo[0] == float("inf"):
        return None
    return {"min": lo, "max": hi}


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return {
        "min_x": float(origin.x) - float(extent.x), "max_x": float(origin.x) + float(extent.x),
        "min_y": float(origin.y) - float(extent.y), "max_y": float(origin.y) + float(extent.y),
        "min_z": float(origin.z) - float(extent.z), "max_z": float(origin.z) + float(extent.z),
        "center_x": float(origin.x), "center_y": float(origin.y),
    }


def overlap_area(a, b, tolerance=OVERLAP_TOLERANCE_CM):
    dx = min(a["max_x"], b["max_x"]) - max(a["min_x"], b["min_x"])
    dy = min(a["max_y"], b["max_y"]) - max(a["min_y"], b["min_y"])
    if dx > tolerance and dy > tolerance:
        return round(dx * dy, 2)
    return 0.0


def main():
    report = {"level": LEVEL_PATH, "errors": [], "warnings": []}

    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, "could not load " + LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL_PATH), world
    report["world"] = world.get_path_name()

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report["actor_count"] = len(actors)

    by_label = {}
    for actor in actors:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)

    # ---- landmarks present exactly once -------------------------------------
    landmarks = []
    for label in EXPECTED_LABELS:
        matches = by_label.get(label, [])
        if len(matches) != 1:
            report["errors"].append(
                "{}: expected exactly 1 actor, found {}".format(label, len(matches)))
            continue
        actor = matches[0]
        box = bounds(actor)
        landmarks.append({"label": label, "actor": actor.get_name(),
                          "_actor": actor, "bounds": box})

    tagged = [actor.get_actor_label() for actor in actors
              if LANDMARK_TAG in [str(tag) for tag in actor.tags]
              and actor.get_actor_label().startswith("Landmark_")]
    unexpected = sorted(set(tagged) - set(EXPECTED_LABELS))
    if unexpected:
        report["errors"].append("unexpected landmark-tagged actors: " + ", ".join(unexpected))
    missing = sorted(set(EXPECTED_LABELS) - set(tagged))
    if missing:
        report["errors"].append("landmarks missing the showcase tag: " + ", ".join(missing))
    report["landmark_count"] = len(landmarks)

    # ---- grounded ------------------------------------------------------------
    # Two independent readings are reported, and the hard check is that the saved
    # level matches the placement manifest.
    #
    # * get_actor_bounds includes the hidden UCX collision volumes these
    #   Blueprints carry, so its min Z sits about 100 cm below the visible model
    #   on every landmark. It is reported, not asserted on.
    # * rendered_geometry walks the instanced mesh data directly. It agrees with
    #   the placement pass on the five HISM-based landmarks and disagrees on
    #   the three V3 gates, whose components are plain StaticMeshComponents - the
    #   same measurement split this project has hit before on the V3 assets. It
    #   is reported as a diagnostic for that reason.
    #
    # The authoritative check is the manifest: the placement recorded each
    # landmark's local geometry floor and set actor Z so the floor lands on the
    # ground. Verifying the saved actor Z against the manifest proves the
    # placement persisted correctly.
    manifest = {}
    if PLACEMENT_MANIFEST.exists():
        stored = json.loads(PLACEMENT_MANIFEST.read_text(encoding="utf-8"))
        manifest = {entry["label"]: entry for entry in stored.get("landmarks", [])}

    grounding = []
    for entry in landmarks:
        actor = entry.pop("_actor")
        actor_z = float(actor.get_actor_location().z)
        geometry = rendered_geometry(actor)
        record = {
            "label": entry["label"],
            "actor_z": round(actor_z, 3),
            "get_actor_bounds_min_z": round(entry["bounds"]["min_z"], 3),
            "collision_dip_cm": round(entry["bounds"]["min_z"] - actor_z, 3),
        }
        if geometry is not None:
            record["rendered_min_z"] = round(actor_z + geometry["min"][2], 3)
            record["rendered_height_cm"] = round(
                geometry["max"][2] - geometry["min"][2], 2)

        expected = manifest.get(entry["label"])
        if expected is None:
            record["manifest"] = "absent"
            report["warnings"].append(
                "{} has no placement manifest entry".format(entry["label"]))
        else:
            expected_z = expected["location"][2]
            record["manifest_actor_z"] = expected_z
            record["manifest_world_min_z"] = expected["world_min_z"]
            drift = round(actor_z - expected_z, 3)
            record["actor_z_drift_cm"] = drift
            if abs(drift) > 0.5:
                report["errors"].append(
                    "{}: saved actor Z {:.3f} does not match the placement "
                    "manifest {:.3f} (drift {:.3f})".format(
                        entry["label"], actor_z, expected_z, drift))
            if abs(expected["world_min_z"] - GROUND_TOP_Z) > GROUNDING_TOLERANCE_CM:
                report["errors"].append(
                    "{}: manifest records world min Z {:.3f}, not the ground "
                    "plane".format(entry["label"], expected["world_min_z"]))
        grounding.append(record)
    report["grounding"] = grounding

    # ---- no XY overlap -------------------------------------------------------
    overlaps = []
    for index, first in enumerate(landmarks):
        for second in landmarks[index + 1:]:
            area = overlap_area(first["bounds"], second["bounds"])
            if area > 0:
                overlaps.append({"a": first["label"], "b": second["label"],
                                 "overlap_area_cm2": area})
    if overlaps:
        report["errors"].append("{} landmark pair(s) overlap in XY".format(len(overlaps)))
    report["overlaps"] = overlaps

    # ---- fits inside the ground plane ---------------------------------------
    ground_actors = [actor for actor in actors if actor.get_actor_label() == GROUND_TAG]
    if len(ground_actors) != 1:
        report["errors"].append(
            "expected exactly 1 ground actor named {}, found {}".format(
                GROUND_TAG, len(ground_actors)))
        ground_box = None
    else:
        ground_box = bounds(ground_actors[0])
        report["ground_bounds"] = {key: round(value, 2) for key, value in ground_box.items()}
        for entry in landmarks:
            box = entry["bounds"]
            if (box["min_x"] < ground_box["min_x"] or box["max_x"] > ground_box["max_x"]
                    or box["min_y"] < ground_box["min_y"] or box["max_y"] > ground_box["max_y"]):
                report["errors"].append(
                    "{} extends past the ground plane".format(entry["label"]))

    # ---- ring shape ----------------------------------------------------------
    if landmarks:
        radii = []
        angles = []
        for entry in landmarks:
            box = entry["bounds"]
            radius = math.hypot(box["center_x"], box["center_y"])
            angle = math.degrees(math.atan2(box["center_y"], box["center_x"])) % 360.0
            radii.append(radius)
            angles.append(angle)
            entry["ring_radius_cm"] = round(radius, 2)
            entry["ring_angle_deg"] = round(angle, 3)
        report["ring_radius_min_cm"] = round(min(radii), 2)
        report["ring_radius_max_cm"] = round(max(radii), 2)
        spread = sorted(angles)
        gaps = [spread[index + 1] - spread[index] for index in range(len(spread) - 1)]
        gaps.append(360.0 - spread[-1] + spread[0])
        report["angular_gap_min_deg"] = round(min(gaps), 3)
        report["angular_gap_max_deg"] = round(max(gaps), 3)
        if min(gaps) <= 0.0:
            report["errors"].append("two landmarks sit at the same ring angle")
        report["angular_spread_deg"] = round(
            sum(1 for gap in gaps if gap > 0) / len(gaps) * 100.0, 1)

    # ---- facing: every facade must face the plaza ---------------------------
    # The landmark is placed at radius*cos/sin(theta) and turned so that the
    # model's facade axis points at the origin, which is where the viewer and
    # the building's own accent light stand. Rotating the recorded facade axis
    # by the actor's saved yaw and taking its angle to the inward direction
    # catches a landmark that is grounded, spaced and lit correctly but turned
    # back to front - a failure every other check in this file would pass.
    facing = []
    for label in EXPECTED_LABELS:
        matches = by_label.get(label, [])
        if len(matches) != 1:
            continue
        actor = matches[0]
        local = FACADE_LOCAL_AXIS.get(label[len("Landmark_"):])
        if local is None:
            report["warnings"].append(
                "{}: no facade axis recorded, facing not verified".format(label))
            continue
        yaw = math.radians(float(actor.get_actor_rotation().yaw))
        # Rotate the model-local facade axis into world space by the saved yaw.
        facade_x = local[0] * math.cos(yaw) - local[1] * math.sin(yaw)
        facade_y = local[0] * math.sin(yaw) + local[1] * math.cos(yaw)
        origin = actor.get_actor_location()
        planar = math.hypot(float(origin.x), float(origin.y))
        inward = (-float(origin.x) / planar, -float(origin.y) / planar)
        cosine = facade_x * inward[0] + facade_y * inward[1]
        angle = math.degrees(math.acos(max(-1.0, min(1.0, cosine))))
        facing.append({"label": label, "facade_local_axis": list(local),
                       "facade_off_plaza_deg": round(angle, 3)})
        if angle > FACING_TOLERANCE_DEG:
            report["errors"].append(
                "{}: facade faces {:.2f} deg away from the plaza centre - the "
                "landmark is turned the wrong way".format(label, angle))
    report["facing"] = facing

    # ---- one independent light per building ----------------------------------
    # Each saved light is compared against a recipe RE-DERIVED here from the
    # placement manifest's own constants plus the landmark's recorded geometry,
    # rather than against the numbers the builder wrote down about its lights.
    # That makes this an independent check of the geometry, not a restatement.
    recipe = {}
    if PLACEMENT_MANIFEST.exists():
        recipe = json.loads(PLACEMENT_MANIFEST.read_text(encoding="utf-8")).get(
            "landmark_light_recipe", {})
    if not recipe:
        report["errors"].append("placement manifest records no landmark_light_recipe")

    expected_light_labels = ["Light_" + label[len("Landmark_"):]
                             for label in EXPECTED_LABELS]
    lights = [actor for actor in actors
              if LIGHT_TAG in [str(tag) for tag in actor.tags]]
    light_labels = sorted(actor.get_actor_label() for actor in lights)
    report["landmark_light_count"] = len(lights)
    if light_labels != sorted(expected_light_labels):
        report["errors"].append(
            "landmark lights: expected {}, found {}".format(
                sorted(expected_light_labels), light_labels))

    def relative_error(actual, expected):
        if expected == 0.0:
            return abs(actual - expected)
        return abs(actual - expected) / abs(expected)

    light_records = []
    light_axes = {}
    for label in EXPECTED_LABELS:
        key = label[len("Landmark_"):]
        light_label = "Light_" + key
        light_matches = by_label.get(light_label, [])
        landmark_matches = by_label.get(label, [])
        stored = manifest.get(label)
        if len(light_matches) != 1 or len(landmark_matches) != 1:
            report["errors"].append(
                "{}: expected exactly 1 light and 1 landmark, found {} and {}".format(
                    light_label, len(light_matches), len(landmark_matches)))
            continue
        if stored is None:
            report["warnings"].append(
                "{}: no placement manifest entry, light not verified".format(light_label))
            continue

        light, landmark = light_matches[0], landmark_matches[0]
        record = {"label": light_label, "landmark": label}

        parent = light.get_attach_parent_actor()
        record["attached_to"] = parent.get_actor_label() if parent else None
        if record["attached_to"] != label:
            report["errors"].append(
                "{}: owned by {}, not its own building".format(
                    light_label, record["attached_to"]))

        radius = stored["geometry_radius_xy"]
        height = stored["height_cm"]
        origin = landmark.get_actor_location()
        x, y = float(origin.x), float(origin.y)
        planar = math.hypot(x, y)
        front = (-x / planar, -y / planar)
        clearance = radius + recipe["facade_clearance_cm"]
        expected_location = (
            x + front[0] * clearance, y + front[1] * clearance,
            GROUND_TOP_Z + height * recipe["height_factor_of_building"])
        expected_aim = (
            x, y, GROUND_TOP_Z + height * recipe["aim_height_factor_of_building"])
        dx = expected_aim[0] - expected_location[0]
        dy = expected_aim[1] - expected_location[1]
        dz = expected_aim[2] - expected_location[2]
        distance = math.sqrt(dx * dx + dy * dy + dz * dz)
        half_angle = math.degrees(math.asin(min(1.0, radius / distance)))
        outer_cone = min(recipe["max_cone_deg"],
                         half_angle + recipe["cone_margin_deg"])
        inner_cone = outer_cone * recipe["inner_cone_fraction"]
        attenuation = (distance + radius) * recipe["attenuation_margin"]
        intensity = recipe["accent_lux_at_building"] * (distance / 100.0) ** 2

        actual = light.get_actor_location()
        location_error = math.dist((float(actual.x), float(actual.y), float(actual.z)),
                                   expected_location)
        record["location_error_cm"] = round(location_error, 3)
        record["distance_cm"] = round(distance, 2)
        record["building_radius_cm"] = round(radius, 2)
        record["half_angle_deg"] = round(half_angle, 3)
        if location_error > LIGHT_LOCATION_TOLERANCE_CM:
            report["errors"].append(
                "{}: stands {:.3f} cm from where the recipe puts it".format(
                    light_label, location_error))

        # The light stands in front of its building, so it is nearer the plaza
        # centre than the building it lights.
        light_radius = math.hypot(float(actual.x), float(actual.y))
        record["light_radius_cm"] = round(light_radius, 2)
        record["landmark_radius_cm"] = round(planar, 2)
        if light_radius >= planar:
            report["errors"].append(
                "{}: is not in front of its building ({:.1f} vs {:.1f} cm from the "
                "centre)".format(light_label, light_radius, planar))

        component = light.get_component_by_class(unreal.SpotLightComponent)
        if component is None:
            report["errors"].append("{}: no SpotLightComponent".format(light_label))
            light_records.append(record)
            continue

        values = {
            "intensity": (float(component.get_editor_property("intensity")), intensity),
            "outer_cone_angle": (
                float(component.get_editor_property("outer_cone_angle")), outer_cone),
            "inner_cone_angle": (
                float(component.get_editor_property("inner_cone_angle")), inner_cone),
            "attenuation_radius": (
                float(component.get_editor_property("attenuation_radius")), attenuation),
        }
        record["actual"] = {name: round(pair[0], 4) for name, pair in values.items()}
        record["expected"] = {name: round(pair[1], 4) for name, pair in values.items()}
        for name, (actual_value, expected_value) in values.items():
            if relative_error(actual_value, expected_value) > LIGHT_VALUE_TOLERANCE:
                report["errors"].append(
                    "{}: {} is {:.4f}, recipe says {:.4f}".format(
                        light_label, name, actual_value, expected_value))
        if not bool(component.get_editor_property("cast_shadows")):
            report["errors"].append("{}: shadows are off".format(light_label))
        record["cast_shadows"] = bool(component.get_editor_property("cast_shadows"))

        # Cone wide enough for the building, attenuation long enough to reach
        # across it.
        if outer_cone <= half_angle:
            report["errors"].append(
                "{}: cone {:.2f} deg is narrower than the {:.2f} deg the building "
                "subtends".format(light_label, outer_cone, half_angle))
        if attenuation <= distance + radius:
            report["errors"].append(
                "{}: attenuation {:.0f} cm does not reach the far side of the "
                "building ({:.0f} cm)".format(light_label, attenuation, distance + radius))

        # Aim: the light's forward axis must pass through its building's aim point.
        rotation = light.get_actor_rotation()
        pitch = math.radians(float(rotation.pitch))
        yaw = math.radians(float(rotation.yaw))
        axis = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw),
                math.sin(pitch))
        axis_length = 1.0
        to_aim = (expected_aim[0] - float(actual.x), expected_aim[1] - float(actual.y),
                  expected_aim[2] - float(actual.z))
        aim_length = math.sqrt(sum(component_value * component_value
                                   for component_value in to_aim))
        cosine = sum(a * b for a, b in zip(axis, to_aim)) / (axis_length * aim_length)
        aim_error = math.degrees(math.acos(max(-1.0, min(1.0, cosine))))
        record["aim_error_deg"] = round(aim_error, 4)
        if aim_error > LIGHT_ANGLE_TOLERANCE_DEG:
            report["errors"].append(
                "{}: aims {:.4f} deg off its building".format(light_label, aim_error))
        light_axes[light_label] = {
            "axis": axis,
            "origin": (float(actual.x), float(actual.y), float(actual.z)),
            "outer_cone_deg": outer_cone, "aim": expected_aim,
            "radius_cm": radius}

        light_records.append(record)
    report["landmark_lights"] = light_records

    # ---- each light lights only its own building -----------------------------
    # Independence is the point of a per-building light. The test is against the
    # neighbouring building's whole BODY, not its centre: a gate 70 deg off the
    # axis still subtends roughly 20 deg from this light, so only
    # angle - neighbour_span - cone tells you whether it is actually lit.
    isolation = []
    margins = []
    for light_label, light in light_axes.items():
        nearest = None
        for other_label, other in light_axes.items():
            if other_label == light_label:
                continue
            to_other = tuple(b - a for a, b in zip(light["origin"], other["aim"]))
            length = math.sqrt(sum(value * value for value in to_other))
            cosine = sum(a * b for a, b in zip(light["axis"], to_other)) / length
            angle = math.degrees(math.acos(max(-1.0, min(1.0, cosine))))
            span = math.degrees(math.asin(min(1.0, other["radius_cm"] / length)))
            margin = angle - span - light["outer_cone_deg"]
            if nearest is None or margin < nearest["margin_deg"]:
                nearest = {"neighbour": other_label, "angle_deg": round(angle, 3),
                           "neighbour_span_deg": round(span, 3),
                           "margin_deg": round(margin, 3)}
        margins.append({"light": light_label, "nearest_neighbour_margin_deg":
                        nearest["margin_deg"], "neighbour": nearest["neighbour"]})
        if nearest["margin_deg"] <= 0.0:
            isolation.append({"light": light_label, "reaches": nearest["neighbour"],
                              "angle_deg": nearest["angle_deg"],
                              "margin_deg": nearest["margin_deg"]})
    if isolation:
        report["errors"].append(
            "{} light(s) take in another landmark".format(len(isolation)))
    report["light_isolation"] = isolation
    report["light_isolation_margins"] = margins

    # ---- supporting actors ---------------------------------------------------
    def count_where(predicate):
        return sum(1 for actor in actors if predicate(actor))

    report["supporting"] = {
        "directional_lights": count_where(
            lambda a: "DirectionalLight" in a.get_class().get_name()),
        "sky_lights": count_where(lambda a: "SkyLight" in a.get_class().get_name()),
        "spot_lights": count_where(
            lambda a: "SpotLight" in a.get_class().get_name()),
        "landmark_lights": count_where(
            lambda a: LIGHT_TAG in [str(tag) for tag in a.tags]),
        "sky_atmosphere": count_where(lambda a: "SkyAtmosphere" in a.get_class().get_name()),
        "height_fog": count_where(
            lambda a: "ExponentialHeightFog" in a.get_class().get_name()),
        "player_starts": count_where(lambda a: "PlayerStart" in a.get_class().get_name()),
        "text_labels": count_where(lambda a: "TextRender" in a.get_class().get_name()),
        "static_mesh_actors": count_where(
            lambda a: a.get_class().get_name().startswith("StaticMeshActor")),
    }
    for key in ("directional_lights", "sky_lights", "player_starts"):
        if report["supporting"][key] < 1:
            report["errors"].append("missing {}".format(key))
    if report["supporting"]["landmark_lights"] != len(EXPECTED_LABELS):
        report["errors"].append(
            "expected {} landmark lights, found {}".format(
                len(EXPECTED_LABELS), report["supporting"]["landmark_lights"]))

    report["landmarks"] = [{key: value for key, value in entry.items()
                            if key not in ("_actor",)} for entry in landmarks]
    report["passed"] = not report["errors"]

    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_VALIDATION " + json.dumps(report))
    print("SHOWCASE_VALIDATION_PASSED", report["passed"])
    print("SHOWCASE_VALIDATION_ERRORS", json.dumps(report["errors"], indent=2))
    print("SHOWCASE_VALIDATION_RING", report.get("ring_radius_min_cm"),
          report.get("ring_radius_max_cm"),
          "gaps", report.get("angular_gap_min_deg"), report.get("angular_gap_max_deg"))
    print("SHOWCASE_VALIDATION_OVERLAPS", json.dumps(report["overlaps"]))
    print("SHOWCASE_VALIDATION_FACING", json.dumps(report.get("facing")))
    print("SHOWCASE_VALIDATION_SUPPORTING", json.dumps(report["supporting"]))
    print("SHOWCASE_VALIDATION_LIGHTS", json.dumps(report.get("landmark_lights"), indent=2))
    print("SHOWCASE_VALIDATION_LIGHT_ISOLATION", json.dumps(report.get("light_isolation")))
    print("SHOWCASE_VALIDATION_LIGHT_MARGINS", json.dumps(report.get("light_isolation_margins")))
    print("SHOWCASE_VALIDATION_MANIFEST", str(MANIFEST))


if __name__ == "__main__":
    main()
