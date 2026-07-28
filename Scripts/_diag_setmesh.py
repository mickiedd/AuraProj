# _diag_setmesh.py
# Try the various ways to assign a UStaticMesh to a freshly spawned
# StaticMeshActor's StaticMeshComponent, to find which one works on UE5.5.

import unreal

def _all():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()

actors = _all()
# pick the last (empty) StaticMeshActor
sms = sorted([a for a in actors if a.get_class().get_name().startswith("StaticMeshActor")],
             key=lambda a: a.get_name())
target = sms[-1]
print("target: {}".format(target.get_name()))
comps = target.get_components_by_class(unreal.StaticMeshComponent)
c = comps[0]
names = [n for n in dir(c) if "mesh" in n.lower() or "Mesh" in n]
print("methods/attrs with 'mesh': {}".format(names))

# Load a known house mesh
mesh = unreal.EditorAssetLibrary.load_asset(
    "/Game/Scifi_desert_city/Meshes/Houses/SM_house_01.SM_house_01")
print("loaded mesh: {}".format(mesh))

# Attempt 1: set_static_mesh
try:
    r1 = c.set_static_mesh(mesh)
    print("A1 set_static_mesh -> r={}".format(r1))
except Exception as e:
    print("A1 EXC: {}".format(e))

# Attempt 2: set_editor_property('StaticMesh', mesh)
try:
    c.set_editor_property("StaticMesh", mesh)
    print("A2 set_editor_property('StaticMesh') ok")
except Exception as e:
    print("A2 EXC: {}".format(e))

# Attempt 3: set_editor_property('static_mesh', mesh)
try:
    c.set_editor_property("static_mesh", mesh)
    print("A3 set_editor_property('static_mesh') ok")
except Exception as e:
    print("A3 EXC: {}".format(e))

# verify via set_editor_property('StaticMesh') which is the underlying UPROPERTY
try:
    cur = c.get_editor_property("StaticMesh")
    print("A4 get_editor_property('StaticMesh') = {}".format(cur))
except Exception as e:
    print("A4 EXC: {}".format(e))

# Final state via set_editor_property
print("DONE")