# analyze_showcase_level.py
# Run inside Unreal Editor's Python (Window > Developer Tools > Python, or console:  py analyze_showcase_level.py)
# Analyzes /Game/Scifi_desert_city/Level/L_showcase_level and prints a structural report:
#   - actor inventory by class
#   - static-mesh instances grouped by source mesh asset, with world counts + bbox
#   - landscape / terrain summary
#   - building arrangement (cluster analysis of the larger structural meshes)

import math
import unreal
from collections import defaultdict, Counter

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"

def fmt_loc(v):
    if v is None:
        return "None"
    return "({:7.0f},{:7.0f},{:7.0f})".format(float(v.x), float(v.y), float(v.z))

def fmt_rot(r):
    # r: unreal.Rotator (pitch, yaw, roll) in degrees
    if r is None:
        return "None"
    return "P{:7.1f} Y{:7.1f} R{:7.1f}".format(float(r.pitch), float(r.yaw), float(r.roll))

def fmt_scale(s):
    if s is None:
        return "None"
    return "({:5.2f},{:5.2f},{:5.2f})".format(float(s.x), float(s.y), float(s.z))

def fmt_transform(actor):
    """Full actor transform: translation, rotation (deg), scale."""
    try:
        loc = actor.get_actor_location()
    except Exception:
        loc = None
    try:
        rot = actor.get_actor_rotation()
    except Exception:
        rot = None
    try:
        scl = actor.get_actor_scale3d()
    except Exception:
        scl = None
    return "loc={} rot={} scale={}".format(fmt_loc(loc), fmt_rot(rot), fmt_scale(scl))

def fmt_transform_from(b):
    """Format transform from a cached building/actor dict (loc/rot/scale keys)."""
    return "loc={} rot={} scale={}".format(
        fmt_loc(b.get("loc")), fmt_rot(b.get("rot")), fmt_scale(b.get("scale")))

def bbox_str(b):
    # b: dict with min/max x/y/z in world space
    return "min=({:7.0f},{:7.0f},{:7.0f}) max=({:7.0f},{:7.0f},{:7.0f}) size=({:6.0f},{:6.0f},{:6.0f})".format(
        b["min_x"], b["min_y"], b["min_z"],
        b["max_x"], b["max_y"], b["max_z"],
        b["max_x"]-b["min_x"], b["max_y"]-b["min_y"], b["max_z"]-b["min_z"],
    )

def merge_bbox(a, b):
    if a is None:
        return dict(b)
    if b is None:
        return dict(a)
    out = dict(a)
    out["min_x"]=min(out["min_x"], b["min_x"]); out["min_y"]=min(out["min_y"], b["min_y"]); out["min_z"]=min(out["min_z"], b["min_z"])
    out["max_x"]=max(out["max_x"], b["max_x"]); out["max_y"]=max(out["max_y"], b["max_y"]); out["max_z"]=max(out["max_z"], b["max_z"])
    return out

def actor_world_bbox(actor):
    try:
        bb = actor.get_actor_bounds(only_colliding_components=False)  # (origin, extent) in world space
        origin, extent = bb[0], bb[1]
        return {
            "min_x": origin.x - extent.x, "max_x": origin.x + extent.x,
            "min_y": origin.y - extent.y, "max_y": origin.y + extent.y,
            "min_z": origin.z - extent.z, "max_z": origin.z + extent.z,
        }
    except Exception:
        return None

def get_static_mesh_paths(actor):
    """Robustly collect UStaticMesh asset paths from any static-mesh-like component on an actor.
    Works around UE5.5 binding differences by iterating ALL components and reading the
    `static_mesh` property by name rather than relying on a single typed component class."""
    paths = []
    try:
        comps = actor.get_components_by_class(unreal.ActorComponent)
    except Exception:
        try:
            comps = actor.get_components_by_class(unreal.SceneComponent)
        except Exception:
            return paths
    for c in comps:
        try:
            cn = c.get_class().get_name()
        except Exception:
            cn = ""
        # only care about components that actually hold a static mesh
        if "StaticMesh" not in cn and "Mesh" not in cn:
            continue
        mesh = None
        # try the typed getter first
        try:
            mesh = c.get_static_mesh()
        except Exception:
            pass
        if mesh is None:
            # fall back to reading the property by name (works for ISM/Hierarchical/etc.)
            try:
                mesh = c.get_editor_property("static_mesh")
            except Exception:
                pass
        if mesh is not None:
            try:
                paths.append(mesh.get_path_name())
            except Exception:
                pass
    return paths

def main():
    unreal.log("==== analyze_showcase_level start ====")
    world = None
    # Load the level as the current world so we can iterate actors.
    # Try several load_map call shapes to be robust across UE5 versions.
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH), False, False)
    except Exception:
        try:
            world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH, False, False)
        except Exception:
            try:
                # UE5.5 exposes LevelEditorSubsystem.load_level; the old
                # EditorLevelLibrary.load_editor_level binding was removed.
                level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                loaded = level_editor.load_level(LEVEL_PATH)
                if loaded:
                    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            except Exception as e1:
                unreal.log_warning("load_map variants failed: {}".format(e1))
    # fallback: use whatever world is currently loaded (e.g. user already opened this level)
    if world is None:
        world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        unreal.log_error("Failed to load level " + LEVEL_PATH + ". Open it in the editor first, then run the script.")
        return

    try:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
    total = len(actors)
    unreal.log("Total actors in level: {}".format(total))

    # ---- 1. Class inventory ----
    class_counts = Counter()
    for a in actors:
        try:
            class_counts[str(a.get_class().get_name())] += 1
        except Exception:
            class_counts["<unknown>"] += 1
    unreal.log("\n--- Actor class inventory ---")
    for cls, n in sorted(class_counts.items(), key=lambda kv: -kv[1]):
        unreal.log("  {:5d}  {}".format(n, cls))

    # ---- 2. Static mesh instances grouped by source mesh ----
    mesh_groups = defaultdict(lambda: {"count":0, "bbox":None, "sample_locs":[]})
    sm_actors = []
    for a in actors:
        cn = ""
        try:
            cn = a.get_class().get_name()
        except Exception:
            pass
        if not cn.startswith("StaticMeshActor"):
            continue
        sm_actors.append(a)
        for key in get_static_mesh_paths(a):
            g = mesh_groups[key]
            g["count"] += 1
            bb = actor_world_bbox(a)
            if bb:
                g["bbox"] = merge_bbox(g["bbox"], bb)
            if len(g["sample_locs"]) < 3:
                try:
                    g["sample_locs"].append(fmt_loc(a.get_actor_location()))
                except Exception:
                    pass

    unreal.log("\n--- Static mesh instances grouped by source mesh ({}) ---".format(len(mesh_groups)))
    rows = []
    for path, g in mesh_groups.items():
        rows.append((g["count"], path, g))
    for n, path, g in sorted(rows, key=lambda r:-r[0]):
        bb = g["bbox"]
        unreal.log("  {:4d}  {}  | {}".format(n, path, bbox_str(bb) if bb else "(no bbox)"))

    # ---- 2b. Per-actor transform dump for static-mesh actors ----
    # The mesh-group section only keeps 3 sample locations; this lists every
    # static-mesh actor's full transform (translation + rotation + scale) so the
    # level's actual placement data is recoverable from the report.
    unreal.log("\n--- Static-mesh actor transforms ({}) ---".format(len(sm_actors)))
    for a in sm_actors:
        try:
            paths = get_static_mesh_paths(a)
        except Exception:
            paths = []
        mesh = paths[0] if paths else "?"
        unreal.log("  {:30s} {} | {}".format(a.get_name(), fmt_transform(a), mesh))

    # ---- 3. Landscape / terrain ----
    # Collect every Landscape* actor (Landscape, LandscapeStreamingProxy, etc.)
    # with its name + full transform so each terrain tile is identifiable in the
    # report (the earlier version printed only an anonymous location line, which
    # made it look as if the landscapes hadn't been found).
    landscape_actors = []
    for a in actors:
        try:
            cn = a.get_class().get_name()
            if cn.startswith("Landscape"):
                landscape_actors.append(a)
        except Exception:
            pass
    unreal.log("\n--- Landscape / terrain ({} found) ---".format(len(landscape_actors)))
    if not landscape_actors:
        unreal.log("  No Landscape actor found. Terrain may be a static mesh instead.")
    for a in landscape_actors:
        try:
            bb = actor_world_bbox(a)
            try:
                comps = a.get_components_by_class(unreal.LandscapeComponent)
                ncomps = len(comps)
            except Exception:
                ncomps = -1
            unreal.log("  {:24s} {} | {}".format(
                a.get_name(), fmt_transform(a), bbox_str(bb) if bb else "(no bbox)"))
            if ncomps >= 0:
                unreal.log("    Landscape components: {}".format(ncomps))
            # Note: LandscapeComponent has no get_local_bounds() in the 5.5 python binding;
            # the actor bbox above already gives the full terrain extent, so we skip per-section detail.
        except Exception as e:
            unreal.log("  {:24s} (failed to read: {})".format(a.get_name(), e))

    # ---- 4. Building arrangement: cluster larger structural meshes ----
    # Treat as "building candidates" any static-mesh actor whose footprint is at least MIN_FOOTPRINT
    # and which comes from a path containing Houses/ Round_buildings/ Base_modules/.
    BUILDING_HINTS = ("Houses", "Round_buildings", "Base_modules", "House_detail")
    MIN_FOOTPRINT = 80.0  # cm — small enough to catch desert shacks / house details

    buildings = []
    for a in sm_actors:
        try:
            paths = get_static_mesh_paths(a)
            if not any(any(h in p for h in BUILDING_HINTS) for p in paths):
                continue
            bb = actor_world_bbox(a)
            if not bb:
                continue
            footprint = max(bb["max_x"]-bb["min_x"], bb["max_y"]-bb["min_y"])
            if footprint < MIN_FOOTPRINT:
                continue
            try:
                rot = a.get_actor_rotation()
            except Exception:
                rot = None
            try:
                scl = a.get_actor_scale3d()
            except Exception:
                scl = None
            buildings.append({
                "actor": a.get_name(),
                "loc": a.get_actor_location(),
                "rot": rot,
                "scale": scl,
                "bbox": bb,
                "footprint": footprint,
                "meshes": paths,
            })
        except Exception:
            pass

    unreal.log("\n--- Building candidates ({} found) ---".format(len(buildings)))
    if not buildings:
        # fallback: any static mesh actor with footprint >= 500 cm, regardless of folder
        for a in sm_actors:
            bb = actor_world_bbox(a)
            if not bb: continue
            footprint = max(bb["max_x"]-bb["min_x"], bb["max_y"]-bb["min_y"])
            if footprint >= 500.0:
                try:
                    rot = a.get_actor_rotation()
                except Exception:
                    rot = None
                try:
                    scl = a.get_actor_scale3d()
                except Exception:
                    scl = None
                buildings.append({
                    "actor": a.get_name(),
                    "loc": a.get_actor_location(),
                    "rot": rot,
                    "scale": scl,
                    "bbox": bb,
                    "footprint": footprint,
                    "meshes": [],
                })
        unreal.log("  (no folder-hint matches; fell back to large static-mesh actors: {})".format(len(buildings)))

    # ---- 3b. Central density: how many static meshes sit in concentric rings around origin ----
    # The terrain is centered at (0,0); a "big city in the center" should show up as a spike
    # of mesh density within ~100-200 m of the origin, then falling off toward the 508 m edge.
    if sm_actors:
        rings = [(5000, "0-50m"), (10000, "50-100m"), (20000, "100-200m"), (40000, "200-400m"), (60000, "400m+")]
        # also compute centroid of ALL static-mesh actors to find the true center of mass
        sx=0.0; sy=0.0; n=0
        for a in sm_actors:
            try:
                l = a.get_actor_location(); sx+=l.x; sy+=l.y; n+=1
            except Exception:
                pass
        if n>0:
            unreal.log("\n--- Central density (static-mesh actor count by distance from origin) ---")
            unreal.log("  Center of mass of all {} static meshes: ({:7.0f}, {:7.0f})".format(n, sx/n, sy/n))
            counts = [0]*len(rings)
            for a in sm_actors:
                try:
                    l = a.get_actor_location()
                    d = math.hypot(l.x, l.y)
                    for ri,(r,_label) in enumerate(rings):
                        if d < r:
                            counts[ri]+=1
                            break
                except Exception:
                    pass
            for (r,label),c in zip(rings, counts):
                unreal.log("  {:>9s}: {:4d} actors".format(label, c))

    # cluster buildings by XY proximity (simple greedy within CLUSTER_RADIUS)
    # 80 m radius: wide enough to bridge streets/plazas so a real city block layout
    # merges into one mass instead of fragmenting every ~30 m.
    CLUSTER_RADIUS = 8000.0  # cm
    clusters = []
    used = [False]*len(buildings)
    for i, b in enumerate(buildings):
        if used[i]: continue
        used[i] = True
        cluster = [b]
        cx = b["loc"].x; cy = b["loc"].y
        changed = True
        while changed:
            changed = False
            for j, b2 in enumerate(buildings):
                if used[j]: continue
                dx = b2["loc"].x - cx; dy = b2["loc"].y - cy
                if math.hypot(dx, dy) <= CLUSTER_RADIUS:
                    cluster.append(b2); used[j] = True
                    # update centroid
                    n = len(cluster)
                    cx = (cx*(n-1) + b2["loc"].x)/n
                    cy = (cy*(n-1) + b2["loc"].y)/n
                    changed = True
        clusters.append(cluster)

    unreal.log("  Detected {} building cluster(s) (XY proximity <= {:.0f} cm):".format(len(clusters), CLUSTER_RADIUS))
    # overall footprint bbox
    if buildings:
        allb = buildings[0]["bbox"]
        for b in buildings[1:]:
            allb = merge_bbox(allb, b["bbox"])
        unreal.log("  Overall building bbox: {}".format(bbox_str(allb)))

    clusters_sorted = sorted(clusters, key=lambda c:-len(c))
    for ci, cluster in enumerate(clusters_sorted):
        xs=[b["loc"].x for b in cluster]; ys=[b["loc"].y for b in cluster]; zs=[b["loc"].z for b in cluster]
        cb = cluster[0]["bbox"]
        for b in cluster[1:]:
            cb = merge_bbox(cb, b["bbox"])
        unreal.log("  Cluster {:2d}: {:3d} buildings | centroid=({:7.0f},{:7.0f}) z_range=({:6.0f}..{:.0f}) | footprint=({:.0f}x{:.0f})".format(
            ci, len(cluster),
            sum(xs)/len(xs), sum(ys)/len(ys),
            min(zs), max(zs),
            cb["max_x"]-cb["min_x"], cb["max_y"]-cb["min_y"]))
        # show first few building entries in the cluster
        for b in cluster[:5]:
            unreal.log("      - {:28s} {} footprint={:.0f} mesh={}".format(
                b["actor"], fmt_transform_from(b), b["footprint"], b["meshes"][0] if b["meshes"] else "?"))
        if len(cluster) > 5:
            unreal.log("      ... and {} more".format(len(cluster)-5))

    # ---- 5. Lighting / misc notable actors ----
    # Actors whose *placement* matters (spawn points, lights, fog, triggers):
    # these we dump with a full transform, not just a tally.
    NOTABLE_TRANSFORM = ("PlayerStart", "PlayerStartPIE", "PointLight", "SpotLight",
                          "DirectionalLight", "SkyLight", "ExponentialHeightFog",
                          "AtmosphericFog", "SkySphere", "Trigger", "Billboard", "Decal")
    notable = NOTABLE_TRANSFORM + ("Volume",)
    counts = Counter()
    transform_actors = []  # (kind, actor) for actors that warrant a transform dump
    for a in actors:
        cn = a.get_class().get_name()
        for n in notable:
            if n.lower() in cn.lower():
                counts[n] += 1
                if n in NOTABLE_TRANSFORM:
                    transform_actors.append((n, a))
                break
    unreal.log("\n--- Notable actors ---")
    for n, c in sorted(counts.items(), key=lambda kv:-kv[1]):
        unreal.log("  {:3d}  {}".format(c, n))

    # Placement-meaningful notables get a full transform line so their position /
    # rotation / scale is recoverable (PlayerStart's spawn transform in particular).
    if transform_actors:
        unreal.log("\n--- Notable actor transforms ({}) ---".format(len(transform_actors)))
        for kind, a in sorted(transform_actors, key=lambda kv: kv[0]):
            try:
                bb = actor_world_bbox(a)
                unreal.log("  {:22s} {:30s} {} | {}".format(
                    kind, a.get_name(), fmt_transform(a),
                    bbox_str(bb) if bb else "(no bbox)"))
            except Exception as e:
                unreal.log("  {:22s} {:30s} (failed to read transform: {})".format(kind, a.get_name(), e))

    unreal.log("\n==== analyze_showcase_level done ====")

main()
