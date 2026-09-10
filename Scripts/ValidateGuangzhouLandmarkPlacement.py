import json
from pathlib import Path
import unreal

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-placement.json")
TAG = "ImportedGuangzhouLandmark"


def box(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z)


def overlaps_xy(a, b, padding=1.0):
    return (a[0] - padding < b[2] and a[2] + padding > b[0] and
            a[1] - padding < b[3] and a[3] + padding > b[1])


world = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
assert world
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
tagged = [a for a in actors if TAG in [str(t) for t in a.tags]]
assert len(tagged) == 4, len(tagged)
manifest = json.loads(MANIFEST.read_text())
expected = {e["label"]: e for e in manifest["actors"]}
assert set(expected) == {a.get_actor_label() for a in tagged}
errors = []
for actor in tagged:
    entry = expected[actor.get_actor_label()]
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert comp and comp.static_mesh
    assert comp.static_mesh.get_path_name() == entry["asset"] + "." + entry["asset"].split("/")[-1]
    actual = box(actor)
    if abs((actual[4] - entry["ground_z"])) > 2.0:
        errors.append(actor.get_actor_label() + " is not grounded")
    if abs(actor.get_actor_location().x - entry["location"][0]) > 2.0:
        errors.append(actor.get_actor_label() + " x transform changed")
    if abs(actor.get_actor_location().y - entry["location"][1]) > 2.0:
        errors.append(actor.get_actor_label() + " y transform changed")
    if abs(actor.get_actor_location().z - entry["location"][2]) > 2.0:
        errors.append(actor.get_actor_label() + " z transform changed")
for i, left in enumerate(tagged):
    for right in tagged[i + 1:]:
        if overlaps_xy(box(left), box(right), padding=100.0):
            errors.append("landmark overlap: {} / {}".format(left.get_actor_label(), right.get_actor_label()))
for landmark in tagged:
    lb = box(landmark)
    for actor in actors:
        if actor in tagged or actor.get_class().get_name().startswith("Landscape"):
            continue
        if actor.get_class().get_name().startswith("StaticMeshActor") and overlaps_xy(lb, box(actor), padding=1.0):
            errors.append("overlap with existing actor: {} / {}".format(landmark.get_actor_label(), actor.get_name()))
assert not errors, errors
result = {"passed": True, "level": LEVEL_PATH,
          "actors": [{"label": a.get_actor_label(), "actor": a.get_name(),
                      "mesh": a.get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_path_name(),
                      "bounds": box(a)} for a in tagged]}
Path("C:/Git/AuraProj/Saved/RawModelImport/placement-validation.json").write_text(json.dumps(result, indent=2))
print("PLACEMENT_VALIDATION", json.dumps(result))
