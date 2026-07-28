# _diag_inspect_house.py
# Pick the most recently spawned StaticMeshActor and dump its component + mesh
# state to figure out why the populated houses aren't holding a house mesh.

import unreal

def _all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()

actors = _all_actors()
sms = [a for a in actors if a.get_class().get_name().startswith("StaticMeshActor")]
# most recently created ones have the highest _N suffix
sms_sorted = sorted(sms, key=lambda a: a.get_name())
print("static-mesh actors: {}".format(len(sms)))
print("first 3 names: {}".format([a.get_name() for a in sms_sorted[:3]]))
print("last 3 names:  {}".format([a.get_name() for a in sms_sorted[-3:]]))
# pick the last one
target = sms_sorted[-1]
name = target.get_name()
print("\ninspecting: {}".format(name))
try:
    comps = target.get_components_by_class(unreal.StaticMeshComponent)
    print("  StaticMeshComponents: {}".format(len(comps)))
    for i, c in enumerate(comps):
        try:
            cn = c.get_class().get_name()
        except Exception:
            cn = "?"
        try:
            m = c.get_static_mesh()
        except Exception as e:
            m = "EXC:{}".format(e)
        try:
            m_path = m.get_path_name() if m is not None and not isinstance(m, str) else "None"
        except Exception:
            m_path = "?"
        print("    [{}] class={} mesh={}".format(i, cn, m_path))
except Exception as e:
    print("  exception: {}".format(e))