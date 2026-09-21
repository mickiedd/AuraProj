"""Explain the landmark bounds discrepancy in the showcase level.

When the landmarks were placed, each was measured at spawn and then re-measured
after being moved and rotated. Several reported a much larger Z extent the
second time (Guidemen 2078 cm -> 5730 cm), which is too tall for a 20 m gate.
This walks the component tree of two landmarks to find which component actually
drives the world bounds, and confirms the placed actor carries the same
component and instance counts the asset is known to have.

Read-only.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"

TARGETS = [
    "Landmark_Guidemen_V5_4K",
    "Landmark_GreatNorthGate",
    "Landmark_ZhenhaiTower",
]

# Component/instance counts recorded for these assets in the project memory.
KNOWN = {
    "Landmark_Guidemen_V5_4K": {"components": 47, "instances": 18822},
}


def component_bounds(component):
    try:
        box = component.get_editor_property("bounds")
        return None
    except Exception:
        return None


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    by_label = {}
    for actor in actors:
        by_label.setdefault(actor.get_actor_label(), []).append(actor)

    report = []
    for label in TARGETS:
        matches = by_label.get(label, [])
        if not matches:
            report.append({"label": label, "error": "not found"})
            continue
        actor = matches[0]
        origin, extent = actor.get_actor_bounds(False)
        entry = {
            "label": label,
            "actor_class": actor.get_class().get_name(),
            "actor_bounds_min_z": round(float(origin.z) - float(extent.z), 2),
            "actor_bounds_max_z": round(float(origin.z) + float(extent.z), 2),
            "actor_bounds_size": [round(float(extent.x) * 2, 2),
                                  round(float(extent.y) * 2, 2),
                                  round(float(extent.z) * 2, 2)],
        }
        components = actor.get_components_by_class(unreal.ActorComponent)
        rows = []
        instances = 0
        for component in components:
            class_name = component.get_class().get_name()
            row = {"class": class_name, "name": component.get_name()}
            count = None
            for getter in ("get_instance_count", "get_num_instances"):
                if hasattr(component, getter):
                    try:
                        count = int(getattr(component, getter)())
                        break
                    except Exception:
                        pass
            if count is not None:
                row["instances"] = count
                instances += count
            if hasattr(component, "get_bounds"):
                try:
                    # Component-space bounds of the component's own geometry.
                    box = component.get_bounds()
                    row["local_bounds_extent"] = [round(float(box.box_extent.x), 2),
                                                  round(float(box.box_extent.y), 2),
                                                  round(float(box.box_extent.z), 2)]
                except Exception:
                    pass
            try:
                row["relative_location"] = [
                    round(float(component.get_editor_property("relative_location").x), 2),
                    round(float(component.get_editor_property("relative_location").y), 2),
                    round(float(component.get_editor_property("relative_location").z), 2)]
            except Exception:
                pass
            try:
                row["is_editor_only"] = bool(
                    component.get_editor_property("is_editor_only"))
            except Exception:
                pass
            rows.append(row)
        entry["component_count"] = len(rows)
        entry["instance_count"] = instances
        if label in KNOWN:
            entry["expected"] = KNOWN[label]
            entry["component_count_matches"] = (
                len(rows) == KNOWN[label]["components"])
            entry["instance_count_matches"] = (instances == KNOWN[label]["instances"])
        # Rank components by the Z they could contribute.
        ranked = sorted(
            rows,
            key=lambda row: max(row.get("local_bounds_extent", [0, 0, 0])[2],
                                row.get("relative_location", [0, 0, 0])[2]),
            reverse=True)[:8]
        entry["tallest_components"] = ranked
        report.append(entry)

    unreal.log("SHOWCASE_BOUNDS_PROBE " + json.dumps(report))
    print("SHOWCASE_BOUNDS_PROBE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
