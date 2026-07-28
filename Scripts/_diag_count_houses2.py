# _diag_count_houses2.py
# Use the manifest to count how many of the spawned actors actually hold a
# house mesh (vs. empty shells). Tells us if set_static_mesh worked or not.

import json
import os
import unreal

def _all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()

proj = unreal.SystemLibrary.get_project_directory()
manifest = json.load(open(os.path.join(proj, "Saved/Scripts/desert_houses_manifest.json"), encoding="utf-8"))
names = {e["actor"] for e in manifest}
print("manifest entries: {}".format(len(names)))

actors = _all_actors()
by_name = {a.get_name(): a for a in actors if a is not None}
present = 0
with_mesh = 0
sample_missing = []
for n in names:
    a = by_name.get(n)
    if a is None:
        sample_missing.append(n)
        continue
    present += 1
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    for c in comps:
        # use editor_property — get_static_mesh isn't bound on UE5.5
        try:
            m = c.get_editor_property("StaticMesh")
        except Exception:
            m = None
        if m is not None:
            with_mesh += 1
            break
print("present in level:    {}/{}".format(present, len(names)))
print("holding house mesh:  {}/{}".format(with_mesh, len(names)))
if sample_missing:
    print("missing sample: {}".format(sample_missing[:5]))