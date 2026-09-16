"""Broad survey: any landmark-ish actor plus occupancy near the landmark row."""
import json
import re
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/great-north-gate-row-survey.json")
PATTERN = re.compile(r"landmark|great|guidemen|xiaobei|zhenhai|gate|tower", re.IGNORECASE)
ROW_X_LIMIT = -130000.0

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
loaded = level_editor.load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, LEVEL_PATH

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return ([round(origin.x - extent.x, 1), round(origin.y - extent.y, 1),
             round(origin.z - extent.z, 1)],
            [round(origin.x + extent.x, 1), round(origin.y + extent.y, 1),
             round(origin.z + extent.z, 1)])


named = []
tag_sets = {}
for actor in actors:
    label = actor.get_actor_label()
    tags = sorted(str(tag) for tag in actor.tags)
    tag_sets.setdefault(tuple(tags), 0)
    tag_sets[tuple(tags)] += 1
    if PATTERN.search(label):
        low, high = bounds(actor)
        named.append({
            "label": label,
            "class": actor.get_class().get_name(),
            "tags": tags,
            "location": [round(actor.get_actor_location().x, 1),
                          round(actor.get_actor_location().y, 1),
                          round(actor.get_actor_location().z, 1)],
            "bounds_min": low,
            "bounds_max": high,
            "parent": (actor.get_attach_parent_actor().get_actor_label()
                       if actor.get_attach_parent_actor() else None),
        })

row_occupancy = []
for actor in actors:
    if not actor.get_class().get_name().startswith("StaticMeshActor"):
        continue
    low, high = bounds(actor)
    if low[0] < ROW_X_LIMIT:
        row_occupancy.append({"label": actor.get_actor_label(), "min": low, "max": high})

row_occupancy.sort(key=lambda item: item["min"][1])
result = {
    "level": LEVEL_PATH,
    "level_actor_count": len(actors),
    "named_actors": sorted(named, key=lambda item: item["label"]),
    "row_occupancy_count": len(row_occupancy),
    "row_occupancy": row_occupancy,
    "tag_histogram": {"|".join(k) if k else "<none>": v
                      for k, v in sorted(tag_sets.items(), key=lambda kv: -kv[1])[:25]},
}
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")

print("LEVEL_ACTORS", len(actors), "ROW_OCCUPANCY", len(row_occupancy))
print("--- named actors ---")
for item in sorted(named, key=lambda entry: entry["label"]):
    print("  {:<54} {} loc={} y=[{}, {}] z_max={}".format(
        item["label"], item["class"], item["location"],
        item["bounds_min"][1], item["bounds_max"][1], item["bounds_max"][2]))
print("--- top tag sets ---")
for key, count in sorted(tag_sets.items(), key=lambda kv: -kv[1])[:12]:
    print("  {:<60} {}".format("|".join(key) or "<none>", count))
print("--- row occupancy Y extent ---")
if row_occupancy:
    print("  y_min", row_occupancy[0]["min"][1], "y_max", row_occupancy[-1]["max"][1])
