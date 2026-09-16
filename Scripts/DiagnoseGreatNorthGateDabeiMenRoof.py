"""Diagnostic captures that isolate the DabeiMen roof mesh.

The source GNG_ROOF mesh turned out to be a downward-facing soffit with no top
surface (measured 0.30% up / 83.65% down), so it disappears from above once
backface culling is on.  These two views are what established that:

  VIEW = "overhead"    hide the other parts and look down.  A solid roof plate
                       would fill the frame; a soffit shows only edge slivers.
  VIEW = "silhouette"  hide the other parts and look up from ground level.  The
                       roof's underside (rafters, and the hanging plaque between
                       the two eaves) is clearly visible from below.

One view per run - the high-res screenshot task only flushes after this script
returns.  The part hiding is deliberately NOT saved, and this script reloads the
map from disk at the start, so running any other capture script afterwards
restores the hidden parts automatically.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"

VIEW = "overhead"

VIEWS = {
    "overhead": {
        "output": "great_north_gate_dabeimen_roof_isolated.png",
        "camera": (-137500.0, 113500.0, 9000.0),
        "target": (-140400.0, 110442.0, 1500.0),
        "label": "GreatNorthGate DabeiMen roof isolated",
        "tag": "GREAT_NORTH_GATE_DABEIMEN_ROOF_ISOLATED",
    },
    "silhouette": {
        "output": "great_north_gate_dabeimen_roof_silhouette.png",
        "camera": (-140400.0, 113900.0, 700.0),
        "target": (-140400.0, 110442.0, 2100.0),
        "label": "GreatNorthGate DabeiMen roof silhouette",
        "tag": "GREAT_NORTH_GATE_DABEIMEN_ROOF_SILHOUETTE",
    },
}

# Hide everything except the roof and the plaque, so the roof is unambiguous.
HIDE_TOKENS = ("GNG_WOOD", "GNG_STONE", "GNG_PLASTER")

view = VIEWS[VIEW]
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport") / view["output"]

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH

hidden = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    label = actor.get_actor_label()
    if any(token in label for token in HIDE_TOKENS):
        actor.set_is_temporarily_hidden_in_editor(True)
        actor.set_actor_hidden_in_game(True)
        hidden.append(label)
assert len(hidden) == len(HIDE_TOKENS), "Expected to hide {} parts, hid {}".format(
    len(HIDE_TOKENS), hidden)

camera = unreal.Vector(*view["camera"])
target = unreal.Vector(*view["target"])
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, view["label"], 0.5, True)
print(view["tag"], {
    "view": VIEW, "path": str(OUTPUT), "hidden": sorted(hidden), "task": str(task)})
