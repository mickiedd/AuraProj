"""Place the imported Guangzhou landmarks in the Scifi Desert showcase level.

The applied pass is recorded below with the reviewed transforms. Set
PREVIEW_ONLY=True in a deliberate local copy to preview a future arrangement.
The applied actors are tagged and written to a manifest
so this operation is identifiable and reversible without touching other level
actors.
"""

import json
import math
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
DEST_ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-placement.json")
PREVIEW_ONLY = False
ACTOR_TAG = "ImportedGuangzhouLandmark"
CLEARANCE = 2500.0
GRID_STEP = 5000.0
EDGE_MARGIN = 12000.0

LANDMARKS = [
    ("GreatSouthGate", DEST_ROOT + "/GreatSouthGate/SM_GreatSouthGate"),
    ("GreatNorthGate", DEST_ROOT + "/GreatNorthGate/SM_GreatNorthGate"),
    ("Xiaobeimen", DEST_ROOT + "/Xiaobeimen/SM_Xiaobeimen"),
    ("ZhenhaiTower", DEST_ROOT + "/ZhenhaiTower/SM_ZhenhaiTower"),
]


def vector(v):
    return [float(v.x), float(v.y), float(v.z)]


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z)


def overlaps(a, b, padding=CLEARANCE):
    return (a[0] - padding < b[2] and a[2] + padding > b[0] and
            a[1] - padding < b[3] and a[3] + padding > b[1])


def load_level():
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH))
    except Exception:
        loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
        world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
                 if loaded else None)
    assert world, LEVEL_PATH
    return world


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def trace_ground_z(world, x, y, ignore):
    start = unreal.Vector(x, y, 60000.0)
    end = unreal.Vector(x, y, -5000.0)
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    if object_type is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, start, end, types, True, ignore, draw, False)
    try:
        data = hit.to_tuple()
        if data and data[0] is True:
            return float(data[4].z)
    except Exception:
        pass
    return 0.0


def mesh_geometry():
    result = []
    for name, path in LANDMARKS:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        assert isinstance(mesh, unreal.StaticMesh), path
        mesh_bounds = mesh.get_bounds()
        o = mesh_bounds.origin
        e = mesh_bounds.box_extent
        # Candidate x/y is the desired world footprint center. Actor location
        # is offset by the mesh origin so the visible bounds land on that point.
        result.append({
            "name": name,
            "path": path,
            "mesh": mesh,
            "half": (float(e.x), float(e.y)),
            "origin": (float(o.x), float(o.y), float(o.z)),
            "size": (float(e.x * 2.0), float(e.y * 2.0), float(e.z * 2.0)),
        })
    return result


def find_terrain_rect(actors):
    lands = [a for a in actors if a.get_class().get_name().startswith("Landscape")]
    assert lands, "No Landscape actors found"
    boxes = [bounds(a) for a in lands]
    return (min(b[0] for b in boxes) + EDGE_MARGIN,
            min(b[1] for b in boxes) + EDGE_MARGIN,
            max(b[2] for b in boxes) - EDGE_MARGIN,
            max(b[3] for b in boxes) - EDGE_MARGIN)


def find_slots(actors, geometry, rect):
    existing = []
    for actor in actors:
        cls = actor.get_class().get_name()
        if not cls.startswith("StaticMeshActor"):
            continue
        existing.append(bounds(actor))
    # Search from the northern edge inward. This leaves the established center
    # showcase area intact when a clear outer patch is available.
    min_x, min_y, max_x, max_y = rect
    candidates = []
    x = min_x
    while x <= max_x:
        y = max_y
        while y >= min_y:
            candidates.append((x, y))
            y -= GRID_STEP
        x += GRID_STEP
    chosen = []
    for item in geometry:
        found = None
        for cx, cy in candidates:
            candidate = (cx - item["half"][0], cy - item["half"][1],
                         cx + item["half"][0], cy + item["half"][1], 0.0, 0.0)
            if any(overlaps(candidate, old) for old in existing):
                continue
            if any(overlaps(candidate, prior["bbox"], padding=5000.0) for prior in chosen):
                continue
            found = (cx, cy, candidate)
            break
        assert found, "No clear footprint found for " + item["name"]
        cx, cy, candidate = found
        chosen.append({"name": item["name"], "item": item, "center": (cx, cy), "bbox": candidate})
    return chosen


def spawn_slot(world, slot, ignore):
    item = slot["item"]
    cx, cy = slot["center"]
    ground = trace_ground_z(world, cx, cy, ignore)
    ox, oy, oz = item["origin"]
    bottom = oz - item["size"][2] / 2.0
    location = unreal.Vector(cx - ox, cy - oy, ground - bottom)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(), location, unreal.Rotator(0.0, 0.0, 0.0), transient=False)
    assert actor, item["name"]
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(item["mesh"])
    actor.set_actor_label("GuangzhouLandmark_" + item["name"])
    actor.tags = [ACTOR_TAG, "GuangzhouLandmarkPark"]
    return actor, ground


def main():
    world = load_level()
    actors = all_actors()
    existing_tagged = [a for a in actors if ACTOR_TAG in [str(t) for t in a.tags]]
    if existing_tagged:
        raise RuntimeError("Placement already exists: " + ", ".join(a.get_name() for a in existing_tagged))
    geometry = mesh_geometry()
    rect = find_terrain_rect(actors)
    ignore = unreal.Array(unreal.Actor)
    for actor in actors:
        if not actor.get_class().get_name().startswith("Landscape"):
            ignore.append(actor)
    slots = find_slots(actors, geometry, rect)
    manifest = {"level": LEVEL_PATH, "tag": ACTOR_TAG, "preview_only": PREVIEW_ONLY,
                "terrain_rect": list(rect), "actors": []}
    for slot in slots:
        item = slot["item"]
        entry = {"label": "GuangzhouLandmark_" + item["name"], "asset": item["path"],
                 "center_xy": list(slot["center"]), "size_cm": list(item["size"])}
        if PREVIEW_ONLY:
            entry["ground_z"] = trace_ground_z(world, *slot["center"], ignore)
        else:
            actor, ground = spawn_slot(world, slot, ignore)
            entry.update({"actor": actor.get_name(), "location": vector(actor.get_actor_location()),
                          "ground_z": ground})
        manifest["actors"].append(entry)
        print("LANDMARK_SLOT", json.dumps(entry))
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(manifest, indent=2))
    if not PREVIEW_ONLY:
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
        print("LEVEL_SAVED", LEVEL_PATH)
    else:
        print("PREVIEW_ONLY", MANIFEST)


main()
