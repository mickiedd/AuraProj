"""Probe the live level for Great North Gate actors and their parts.

Reports every actor whose label contains "GreatNorthGate" (plus the other
Guangzhou landmarks for context) together with its class, tags, transform,
bounds and child StaticMeshActor parts.  Read-only.
"""
import json
from pathlib import Path

import unreal

PROJECT_ROOT = Path("C:/Git/AuraProj")
OUT = PROJECT_ROOT / "Saved/RawModelImport/dabeimen-level-probe.json"

actors = unreal.EditorLevelLibrary.get_all_level_actors()
report = {"level": unreal.EditorLevelLibrary.get_editor_world().get_name(), "actors": []}

for actor in actors:
    label = actor.get_actor_label()
    if "GreatNorthGate" not in label and "GuangzhouLandmark" not in label:
        continue
    loc = actor.get_actor_location()
    bounds = actor.get_actor_bounds(only_colliding_components=False)
    origin, extent = bounds[0], bounds[1]
    parts = []
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.get_editor_property("static_mesh")
        parts.append({
            "component": comp.get_name(),
            "mesh": mesh.get_path_name() if mesh else None,
        })
    report["actors"].append({
        "label": label,
        "class": actor.get_class().get_name(),
        "path": actor.get_path_name(),
        "tags": [str(t) for t in actor.tags],
        "location": [loc.x, loc.y, loc.z],
        "rotation": [actor.get_actor_rotation().roll, actor.get_actor_rotation().pitch,
                     actor.get_actor_rotation().yaw],
        "bounds_min": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "bounds_max": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "size_cm": [extent.x * 2, extent.y * 2, extent.z * 2],
        "component_count": len(parts),
        "components": parts,
        "children": [c.get_actor_label() for c in actor.get_attached_actors()],
    })

report["actors"].sort(key=lambda a: a["label"])
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

print("GREAT_NORTH_GATE_LEVEL_PROBE", json.dumps({"actor_count": len(report["actors"])}))
for a in report["actors"]:
    print("  {:<52} {:<26} loc={} size={}".format(
        a["label"], a["class"], [round(v, 1) for v in a["location"]],
        [round(v, 1) for v in a["size_cm"]]))
    for c in a["components"]:
        print("      part:", c["mesh"])
