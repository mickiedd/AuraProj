"""Find a readable accessor on HitResult in this build."""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
hit = unreal.SystemLibrary.line_trace_single(
    world, unreal.Vector(0, 0, 10000), unreal.Vector(0, 0, -10000),
    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
print("HIT_TYPE", type(hit).__name__ if hit else None)
if hit:
    names = [n for n in dir(hit) if not n.startswith("_")]
    print("HIT_MEMBERS", names[:40])
    for attr in ("location", "impact_point", "hit_location", "to_tuple"):
        try:
            value = getattr(hit, attr)
            print("HIT_ATTR_OK", attr, value() if callable(value) else value)
        except Exception as error:
            print("HIT_ATTR_FAIL", attr, repr(error)[:90])
else:
    print("HIT_NONE (no geometry under the probe point; accessor test inconclusive)")
