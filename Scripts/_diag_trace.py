# _diag_trace.py  — fast, print-based trace diagnostic for remote execution.
# Does NOT reload the level (uses the currently open editor world) and prints
# results to stdout so they come back in the remote-exec command_result.
# Throwaway helper for iterating on the ground-trace method.

import random
import unreal


def _world_static_obj_type():
    for name in ("OBJECT_TYPE_QUERY_WORLD_STATIC", "OBJECT_TYPE_QUERY1"):
        v = getattr(unreal.ObjectTypeQuery, name, None)
        if v is not None:
            return v
    return None


def _draw_debug_none():
    for name in ("NONE", "NO_TRACE", "DDT_None", "NONE_"):
        v = getattr(unreal.DrawDebugTrace, name, None)
        if v is not None:
            return v
    return None


def _visibility_trace_query():
    for name in ("TRACE_QUERY_VISIBILITY", "VISIBILITY", "TRACE_QUERY1", "TRACE_TYPE_QUERY1"):
        v = getattr(unreal.TraceTypeQuery, name, None)
        if v is not None:
            return v
    return None


def _extract_hit(res):
    if res is None:
        return None
    if isinstance(res, tuple):
        if len(res) >= 2 and res[0]:
            return res[1]
        if len(res) >= 2 and res[1] is True and res[0] is not None:
            return res[0]
        return None
    return res


def _hit_z(hit):
    if hit is None:
        return None
    for attr in ("impact_point", "location"):
        try:
            v = getattr(hit, attr)
            if v is not None:
                return float(v.z)
        except Exception:
            continue
    for prop in ("impact_point", "location"):
        try:
            v = hit.get_editor_property(prop)
            if v is not None:
                return float(v.z)
        except Exception:
            continue
    return None


def _editor_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        return unreal.EditorLevelLibrary.get_editor_world()


def _all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()


def _hit_z_from_struct(hit):
    """Read impact Z from a HitResult via to_tuple() (properties are protected)."""
    if hit is None:
        return None
    try:
        t = hit.to_tuple()
    except Exception:
        return None
    if not t or t[0] is not True:  # bBlockingHit at index 0
        return None
    # UE5.5 FHitResult order: ImpactPoint=4, Location=5. Validate it's a Vector.
    for idx in (4, 5):
        if idx < len(t):
            v = t[idx]
            try:
                return float(v.z)
            except Exception:
                continue
    return None


def _hit_actor_name(hit):
    try:
        t = hit.to_tuple()
    except Exception:
        return "?"
    # HitActor at idx 9, HitComponent at idx 10 in UE5.5 FHitResult order
    for idx in (9, 10):
        if idx < len(t):
            el = t[idx]
            try:
                return "{}({})".format(type(el).__name__, el.get_name())
            except Exception:
                continue
    return "(none)"


def trace_z(world, x, y, ignore):
    start = unreal.Vector(x, y, 60000.0)
    end = unreal.Vector(x, y, -5000.0)
    ddt = _draw_debug_none()
    ot = _world_static_obj_type()
    if ot is None or ddt is None:
        print("  enums not resolved")
        return None
    otypes = unreal.Array(unreal.ObjectTypeQuery)
    otypes.append(ot)
    try:
        res = unreal.SystemLibrary.line_trace_single_for_objects(
            world, start, end, otypes, True, ignore, ddt, False)
    except Exception as e:
        print("  for_objects EXCEPTION: {}".format(e))
        return None
    z = _hit_z_from_struct(res)
    actor = _hit_actor_name(res) if z is not None else "(no hit)"
    print("  hit z={} actor={}".format(z if z is not None else "None", actor))
    return z


def main():
    print("==== _diag_trace start ====")
    world = _editor_world()
    print("editor world: {}".format(world))
    actors = _all_actors()
    lands = [a for a in actors if a.get_class().get_name().startswith("Landscape")]
    print("landscape actors: {}".format(len(lands)))
    if not lands:
        print("NO LANDSCAPES — cannot test. Open L_showcase_level in the editor.")
        return
    min_x = min_y = float("inf"); max_x = max_y = float("-inf")
    for a in lands:
        o, e = a.get_actor_bounds(only_colliding_components=False)
        min_x = min(min_x, o.x - e.x); min_y = min(min_y, o.y - e.y)
        max_x = max(max_x, o.x + e.x); max_y = max(max_y, o.y + e.y)
    cx = (min_x + max_x) / 2.0
    cy = (min_y + max_y) / 2.0
    print("terrain center=({:.0f},{:.0f}) bbox x[{:.0f},{:.0f}] y[{:.0f},{:.0f}]".format(
        cx, cy, min_x, max_x, min_y, max_y))

    # Ignore every actor that ISN'T a landscape, so the trace can only hit terrain
    # (ProceduralFoliageVolumes and other blockers sit above the landscape).
    ignore = unreal.Array(unreal.Actor)
    n_ignore = 0
    for a in actors:
        try:
            cn = a.get_class().get_name()
        except Exception:
            cn = ""
        if not cn.startswith("Landscape"):
            ignore.append(a)
            n_ignore += 1
    print("ignore list: {} non-landscape actors".format(n_ignore))

    print("-- trace at terrain center --")
    trace_z(world, cx, cy, ignore)

    print("-- trace at 30 random points (z variance check) --")
    random.seed(7)
    zs = []
    for i in range(30):
        x = random.uniform(min_x + 5000, max_x - 5000)
        y = random.uniform(min_y + 5000, max_y - 5000)
        z = trace_z(world, x, y, ignore)
        if z is not None:
            zs.append(z)
    if zs:
        uniq = sorted(set([round(zv, 1) for zv in zs]))
        print("z stats: n={} min={:.2f} max={:.2f} unique={}".format(
            len(zs), min(zs), max(zs), uniq[:20]))
    print("==== _diag_trace done ====")


main()