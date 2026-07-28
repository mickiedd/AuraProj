# _diag_count_houses.py
# Quick count of houses vs total actors in the current editor world.
# Used to verify the level state before re-spawning houses with yaw=0.

import unreal

def _all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()

actors = _all_actors()
print("total actors in level: {}".format(len(actors)))
# count by class
from collections import Counter
cn = Counter()
for a in actors:
    try:
        cn[a.get_class().get_name()] += 1
    except Exception:
        cn["<unknown>"] += 1
print("class histogram (top 10):")
for k, v in sorted(cn.items(), key=lambda kv: -kv[1])[:10]:
    print("  {:5d}  {}".format(v, k))
# also: any actor whose name starts with StaticMeshActor and lives in the Houses folder
import os
from pathlib import Path
houses_actors = 0
houses_sample = []
for a in actors:
    try:
        name = a.get_name()
        cn_name = a.get_class().get_name()
    except Exception:
        continue
    if not cn_name.startswith("StaticMeshActor"):
        continue
    # check if any of its components holds a static mesh under .../Houses/...
    try:
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        comps = []
    for c in comps:
        try:
            m = c.get_static_mesh()
        except Exception:
            m = None
        if m is None:
            continue
        try:
            p = m.get_path_name()
        except Exception:
            continue
        if "Houses" in p:
            houses_actors += 1
            if len(houses_sample) < 5:
                houses_sample.append((name, p.split("/")[-1]))
            break
print("actors holding a house mesh: {}".format(houses_actors))
for n, m in houses_sample:
    print("  sample: {} ({})".format(n, m))