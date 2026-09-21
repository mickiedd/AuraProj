"""Measure the placed landmarks with raycasts instead of get_actor_bounds.

get_actor_bounds reports a Z extent for these landmarks that does not reconcile
with their transforms (Guidemen reports 5730 cm tall; the asset is a 20 m gate).
This measures the same geometry a completely independent way:

  * transform the HISM's own mesh bounds by the component's world transform and
    the instance transform, giving an expected world AABB, and
  * line-trace down onto the landmark to find its real top, and up from below to
    find its real bottom.

If the raycast bottom is at the ground plane the placement is grounded, whatever
get_actor_bounds claims.

Read-only.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"

TARGETS = [
    "Landmark_GreatNorthGate",
    "Landmark_ZhenhaiTower",
    "Landmark_Guidemen_V5_4K",
    "Landmark_Zhengnanmen_HighFidelity",
]


def to_tuple(hit):
    try:
        return hit.to_tuple()
    except Exception:
        return None


def trace(world, start, end, ignore):
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, start, end, types, True, ignore, draw, False)
    data = to_tuple(hit)
    if data and data[0] is True:
        return float(data[4].z)
    return None


def transformed_mesh_aabb(mesh, component_transform):
    """World AABB of the mesh bounds under a transform, via its 8 corners."""
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    xs = [float(origin.x) - float(extent.x), float(origin.x) + float(extent.x)]
    ys = [float(origin.y) - float(extent.y), float(origin.y) + float(extent.y)]
    zs = [float(origin.z) - float(extent.z), float(origin.z) + float(extent.z)]
    lo = [1e18, 1e18, 1e18]
    hi = [-1e18, -1e18, -1e18]
    for x in xs:
        for y in ys:
            for z in zs:
                point = component_transform.transform_location(unreal.Vector(x, y, z))
                lo = [min(lo[0], point.x), min(lo[1], point.y), min(lo[2], point.z)]
                hi = [max(hi[0], point.x), max(hi[1], point.y), max(hi[2], point.z)]
    return lo, hi


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
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
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        origin, extent = actor.get_actor_bounds(False)
        entry = {
            "label": label,
            "actor_location": [round(float(location.x), 2), round(float(location.y), 2),
                               round(float(location.z), 2)],
            "actor_yaw": round(float(rotation.yaw), 3),
            "get_actor_bounds_z": [round(float(origin.z) - float(extent.z), 2),
                                   round(float(origin.z) + float(extent.z), 2)],
        }

        # Expected AABB straight from the instanced mesh, ignoring actor bounds.
        component = actor.get_component_by_class(
            unreal.HierarchicalInstancedStaticMeshComponent)
        if component:
            mesh = component.static_mesh
            entry["mesh"] = mesh.get_path_name() if mesh else None
            entry["instance_count"] = int(component.get_instance_count())
            if mesh and entry["instance_count"] > 0:
                instance_transform = component.get_instance_transform(0)
                actor_transform = actor.get_actor_transform()
                combined = instance_transform * actor_transform
                lo, hi = transformed_mesh_aabb(mesh, combined)
                entry["expected_from_mesh_z"] = [round(lo[2], 2), round(hi[2], 2)]
                entry["expected_from_mesh_xy_size"] = [
                    round(hi[0] - lo[0], 2), round(hi[1] - lo[1], 2)]

        # Independent measurement: raycasts against this actor alone.
        ignore = unreal.Array(unreal.Actor)
        for other in actors:
            if other is not actor:
                ignore.append(other)
        top = trace(world, unreal.Vector(location.x, location.y, 200000.0),
                    unreal.Vector(location.x, location.y, -20000.0), ignore)
        bottom = trace(world, unreal.Vector(location.x, location.y, -20000.0),
                       unreal.Vector(location.x, location.y, 200000.0), ignore)
        entry["raycast_top_z"] = round(top, 2) if top is not None else None
        entry["raycast_bottom_z"] = round(bottom, 2) if bottom is not None else None
        if top is not None and bottom is not None:
            entry["raycast_height_cm"] = round(top - bottom, 2)
        report.append(entry)

    unreal.log("SHOWCASE_RAYCAST_PROBE " + json.dumps(report))
    print("SHOWCASE_RAYCAST_PROBE", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
