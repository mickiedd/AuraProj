"""Update the Great North Gate to the DabeiMen model.

Replaces the superseded GuangzhouLandmark_GreatNorthGate_HighDetail actor (the
work-in-progress wrapper built from the older V2 GLB) with a new wrapper around
the DabeiMen import, at the same footprint centre in the landmark corridor.

The five DabeiMen meshes all sit at identity in one shared building space, so no
per-part transform is needed.  The new root keeps the landmark-park tagging
convention used by the other imported landmarks, with its own root/part tags so
validation can find it without matching the retired HighDetail actor.

Deliberately NOT touched here:
  * GuangzhouLandmark_GreatNorthGate  - the original landmark actor.
  * GuangzhouLandmark_GreatSouthGate / Xiaobeimen / ZhenhaiTower.
  * The GreatNorthGate_HighDetail assets on disk - only the actor is retired, so
    the previous import stays available for comparison or rollback.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatNorthGate_DabeiMen.json"
MANIFEST = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-dabeimen-placement.json"

ROOT_LABEL = "GuangzhouLandmark_GreatNorthGate_DabeiMen"
ROOT_TAG = "GreatNorthGateDabeiMenRoot"
PART_TAG = "GreatNorthGateDabeiMenPart"
ACTOR_TAG = "ImportedGuangzhouLandmark"
PARK_TAG = "GuangzhouLandmarkPark"

# The retired work-in-progress wrapper, matched by tag so we never touch the
# original GuangzhouLandmark_GreatNorthGate actor.
SUPERSEDED_ROOT_TAG = "GreatNorthGateHighDetailRoot"
SUPERSEDED_PART_TAG = "GreatNorthGateHighDetailPart"

# Same footprint centre the superseded actor used, inside the landmark corridor.
CENTER_X = -140400.0
CENTER_Y = 110442.5
CLEARANCE = 500.0
EXPECTED_PARTS = 5
# The superseded HighDetail wrapper was built from the older 6-mesh import.
SUPERSEDED_PART_COUNT = 6
MIN_HEIGHT_CM = 1000.0


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z,
            origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]


def load_level():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
             if loaded else None)
    assert world, LEVEL_PATH
    return world


def trace_ground_z(world, x, y, ignore):
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    if object_type is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -5000.0),
        types, True, ignore, draw, False)
    data = hit.to_tuple()
    return float(data[4].z) if data and data[0] is True else 0.0


import_report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
assert import_report["mesh_count"] == EXPECTED_PARTS, import_report["mesh_count"]

world = load_level()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# ---- retire the superseded HighDetail wrapper ---------------------------------
existing_roots = [a for a in all_actors() if ROOT_TAG in [str(t) for t in a.tags]]
assert len(existing_roots) == 0, "DabeiMen root already exists: " + \
    ", ".join(a.get_actor_label() for a in existing_roots)

retired = []
superseded = [a for a in all_actors()
              if SUPERSEDED_PART_TAG in [str(t) for t in a.tags]
              or SUPERSEDED_ROOT_TAG in [str(t) for t in a.tags]]
# Retiring is idempotent: a previous run may already have removed them without
# reaching the save, in which case the live level has none left to destroy.
assert len(superseded) in (0, SUPERSEDED_PART_COUNT + 1), \
    "Unexpected superseded actor count: {} ({})".format(
        len(superseded), sorted(a.get_actor_label() for a in superseded))
for actor in superseded:
    retired.append(actor.get_actor_label())
    assert actor_subsystem.destroy_actor(actor), actor.get_actor_label()

remaining = [a for a in all_actors()
             if SUPERSEDED_PART_TAG in [str(t) for t in a.tags]
             or SUPERSEDED_ROOT_TAG in [str(t) for t in a.tags]]
assert not remaining, "Superseded actors survived: " + \
    ", ".join(a.get_actor_label() for a in remaining)

# ---- footprint guard ----------------------------------------------------------
# Use the real imported footprint so the guard reflects what will actually be
# placed: the DabeiMen building is a wall-plus-tower assembly and is much wider
# than the model it replaces.
half_x = import_report["union_bounds_cm"]["size"][0] / 2.0
half_y = import_report["union_bounds_cm"]["size"][1] / 2.0
candidate = [CENTER_X - half_x, CENTER_Y - half_y, -1e9,
             CENTER_X + half_x, CENTER_Y + half_y, 1e9]
conflicts = []
for actor in all_actors():
    if not actor.get_class().get_name().startswith("StaticMeshActor"):
        continue
    other = bounds(actor)
    if (candidate[0] - CLEARANCE < other[3] and candidate[3] + CLEARANCE > other[0]
            and candidate[1] - CLEARANCE < other[4] and candidate[4] + CLEARANCE > other[1]):
        conflicts.append(actor.get_actor_label())
assert not conflicts, "Footprint overlaps: " + ", ".join(sorted(conflicts))

ignore = unreal.Array(unreal.Actor)
for actor in all_actors():
    if not actor.get_class().get_name().startswith("Landscape"):
        ignore.append(actor)
ground_z = trace_ground_z(world, CENTER_X, CENTER_Y, ignore)

# ---- spawn the new wrapper ----------------------------------------------------
root = actor_subsystem.spawn_actor_from_class(
    unreal.Actor.static_class(), unreal.Vector(CENTER_X, CENTER_Y, ground_z),
    unreal.Rotator(0.0, 0.0, 0.0), transient=False)
assert root, ROOT_LABEL
root.set_actor_label(ROOT_LABEL)
root.tags = [ACTOR_TAG, PARK_TAG, ROOT_TAG]
# A static root is required before attaching StaticMeshActor roots.
root.root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

root_location = root.get_actor_location()
part_tags = [ACTOR_TAG, PARK_TAG, PART_TAG]
part_labels = []
for entry in import_report["meshes"]:
    mesh = unreal.EditorAssetLibrary.load_asset(entry["path"])
    assert isinstance(mesh, unreal.StaticMesh), entry["path"]
    label = ROOT_LABEL + "__" + entry["name"]
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(), root_location, unreal.Rotator(0.0, 0.0, 0.0),
        transient=False)
    assert actor, label
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component, label
    component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.tags = part_tags
    attached = actor.root_component.attach_to_component(
        root.root_component, unreal.Name(""),
        unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
        unreal.AttachmentRule.KEEP_WORLD, False)
    assert attached, label
    actor.modify()
    part_labels.append(label)

# Ground the assembled group: shift the root so the lowest point sits on terrain.
parts = [a for a in all_actors() if PART_TAG in [str(t) for t in a.tags]]
assert len(parts) == EXPECTED_PARTS, len(parts)
group_min_z = min(bounds(a)[2] for a in parts)
root.set_actor_location(
    unreal.Vector(root_location.x, root_location.y, root_location.z + (ground_z - group_min_z)),
    False, True)

parts = [a for a in all_actors() if PART_TAG in [str(t) for t in a.tags]]
part_bounds = [bounds(a) for a in parts]
actual_bounds = [min(i[0] for i in part_bounds), min(i[1] for i in part_bounds),
                 min(i[2] for i in part_bounds), max(i[3] for i in part_bounds),
                 max(i[4] for i in part_bounds), max(i[5] for i in part_bounds)]
assert abs(actual_bounds[2] - ground_z) < 1.0, "Building is not grounded: {} vs {}".format(
    actual_bounds[2], ground_z)
assert all(a.get_attach_parent_actor() is root for a in parts), "Unparented parts"
assert actual_bounds[5] - actual_bounds[2] > MIN_HEIGHT_CM, "Building has no height"

assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Failed to save showcase level"

manifest = {
    "level": LEVEL_PATH,
    "source": import_report["source"],
    "asset_folder": import_report["destination"],
    "root": root.get_actor_label(),
    "root_actor": root.get_name(),
    "root_tags": [str(t) for t in root.tags],
    "location": [root.get_actor_location().x, root.get_actor_location().y,
                 root.get_actor_location().z],
    "rotation": [root.get_actor_rotation().roll, root.get_actor_rotation().pitch,
                 root.get_actor_rotation().yaw],
    "center_xy": [(actual_bounds[0] + actual_bounds[3]) / 2.0,
                  (actual_bounds[1] + actual_bounds[4]) / 2.0],
    "ground_z": ground_z,
    "bounds": actual_bounds,
    "size_cm": [actual_bounds[3] - actual_bounds[0], actual_bounds[4] - actual_bounds[1],
                actual_bounds[5] - actual_bounds[2]],
    "parts": sorted(part_labels),
    "part_count": len(parts),
    "retired_actors": sorted(retired),
    "retired_asset_folder_kept": "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_HighDetail",
    "passed": True,
}
MANIFEST.parent.mkdir(parents=True, exist_ok=True)
MANIFEST.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print("GREAT_NORTH_GATE_DABEIMEN_PLACEMENT", json.dumps({
    "root": manifest["root"],
    "location": [round(v, 1) for v in manifest["location"]],
    "ground_z": round(ground_z, 2),
    "size_cm": [round(v, 1) for v in manifest["size_cm"]],
    "part_count": manifest["part_count"],
    "retired": len(retired)}))
