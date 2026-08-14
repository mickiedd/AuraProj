# populate_desert_houses.py
# Run inside Unreal Editor's Python (console:  py populate_desert_houses.py).
# Scatters house meshes from /Game/Scifi_desert_city/Meshes/Houses across the
# desert terrain of L_showcase_level, sparsely and randomly, avoiding the
# existing city and other structures.
#
# Method:
#   - terrain area = union of the 9 Landscape actor bboxes (queried at runtime,
#     so it matches the grid fixed by fix_landscape_grid.py)
#   - rejection sampling with a minimum spacing (no two houses closer than
#     MIN_SPACING) and a clearance from any existing building
#   - ground Z found by a downward line trace against the landscape
#   - each house's base is settled onto the ground regardless of its pivot
#   - random yaw for variety; optional scale jitter
#
# IMPORTANT:
#   APPLY = False  -> dry run: prints the plan + a per-house table, spawns nothing.
#   APPLY = True   -> spawns the StaticMeshActors and writes a manifest JSON you
#                     can use to select/delete them later.
#
# After applying, SAVE THE LEVEL (Ctrl+S) to persist.

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
HOUSES_MESH_PATH = "/Game/Scifi_desert_city/Meshes/Houses"
ROCKS_MESH_PATH = "/Game/Scifi_desert_city/Meshes/Rocks"

# ---- placement parameters (all in centimeters; 100 cm = 1 m) ----
# Cluster mode: scatter N villages of HOUSES_PER_CLUSTER[min..max] houses each,
# each village randomly placed, houses within CLUSTER_RADIUS of the village
# centroid. Inter-village spacing enforced via CLUSTER_SPACING.
USE_CLUSTERS = True
CLUSTER_COUNT = 36                 # how many villages to scatter
HOUSES_PER_CLUSTER = (12, 55)        # random count per village
CLUSTER_RADIUS = 5500.0            # 15 m — max XY offset of a house from its village centroid
CLUSTER_SPACING = 30000.0          # 600 m — minimum distance between village centroids
# Per-house constraints (apply to every placed house, cluster or not):
BUILDING_CLEARANCE = 15000.0       # 150 m — keep houses away from existing structures
INTRA_CLUSTER_SPACING = 1600.0      # 6 m — minimum distance between houses within a village
MIN_HOUSE_FOOTPRINT = 300.0        # skip meshes smaller than this (tiny detail pieces)
SCALE_RANGE = (1.0, 1.0)           # set e.g. (0.9, 1.1) for subtle scale jitter; (1,1) = none
RANDOM_YAW = False                 # False -> all houses axis-aligned (yaw=0); True -> random 0-360 yaw
EDGE_MARGIN = 8000.0               # 80 m — keep houses away from the terrain border
# Rock ring around each cluster (only used when USE_CLUSTERS=True):
ROCK_COUNT_PER_CLUSTER = (3, 7)    # random rock count per village
ROCK_RING_INNER_MULT = 1.5         # inner radius = CLUSTER_RADIUS * this (just outside houses)
ROCK_RING_OUTER_MULT = 3.0         # outer radius = CLUSTER_RADIUS * this (village halo)
ROCK_TO_ROCK_SPACING = 400.0       # 4 m — minimum distance between rocks
ROCK_TO_HOUSE_SPACING = 500.0      # 5 m — minimum distance between a rock and any placed house
ROCK_MIN_FOOTPRINT = 50.0          # skip rocks smaller than this (skip pebbles)
# PlayerStart per village (only used when USE_CLUSTERS=True): place one
# untagged PlayerStart near each village centroid. AAuraGameModeBase::
# ChoosePlayerStart picks randomly among ALL PlayerStarts when no tag matches
# (the fresh-game case), so the player is born in a random village with no
# tagging required. (Tag matching still handles checkpoint/portal respawns.)
ADD_PLAYER_START_PER_CLUSTER = True
PLAYER_START_Z_OFFSET = 100.0         # raise the capsule base slightly above the ground
PLAYER_START_MAX_OFFSET = 2500.0     # 25 m — max XY search offset from the centroid
PLAYER_START_CLEAR_RADIUS = 1800.0   # 18 m — min distance from any house or rock
PLAYER_START_SEARCH_TRIES = 24
SEED = 12345                       # reproducible randomness
MAX_TOTAL_ATTEMPTS = 40000         # safety cap on candidate generation

# Legacy single-house mode (USE_CLUSTERS=False) keeps the original behavior:
TARGET_COUNT = 50                  # how many houses to place (single-house mode)
MIN_SPACING = 35000.0              # 350 m — minimum distance between placed houses (single-house mode)

APPLY = True
MANIFEST_FILE_NAME = "Saved/Scripts/desert_houses_manifest.json"
USE_CURRENT_WORLD = True  # if True and the level is already open, don't reload it (faster, no save prompts)

BUILDING_HINTS = ("Houses", "Round_buildings", "Base_modules", "House_detail")
EXISTING_FOOTPRINT_MIN = 500.0  # static meshes below this footprint aren't treated as buildings


def _editor_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        return unreal.EditorLevelLibrary.get_editor_world()


def _all_level_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()


def _world_has_landscapes(world):
    if world is None:
        return False
    try:
        actors = _all_level_actors()
    except Exception:
        return False
    for a in actors:
        try:
            if a.get_class().get_name().startswith("Landscape"):
                return True
        except Exception:
            pass
    return False


def load_world():
    # Prefer the currently open world, but only if it actually contains the
    # landscapes we need — otherwise the user has a different level open and we
    # must load L_showcase_level ourselves.
    if USE_CURRENT_WORLD:
        w = _editor_world()
        if w is not None and _world_has_landscapes(w):
            return w
        unreal.log_warning("Current editor world has no landscapes; loading {}...".format(LEVEL_PATH))
    try:
        return unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH), False, False)
    except Exception:
        try:
            return unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH, False, False)
        except Exception:
            try:
                level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                if level_editor.load_level(LEVEL_PATH):
                    return _editor_world()
            except Exception:
                return _editor_world()


def actor_bbox(a):
    o, e = a.get_actor_bounds(only_colliding_components=False)
    return (o.x - e.x, o.y - e.y, o.z - e.z,
            o.x + e.x, o.y + e.y, o.z + e.z)


def get_static_mesh_paths(actor):
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
        if "StaticMesh" not in cn and "Mesh" not in cn:
            continue
        mesh = None
        try:
            mesh = c.get_static_mesh()
        except Exception:
            pass
        if mesh is None:
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


def discover_static_meshes(folder_path, min_footprint):
    """Return list of (asset_path, StaticMesh, footprint_cm) for every StaticMesh
    asset under `folder_path` whose footprint (max X/Y extent) is at least
    `min_footprint` cm. Searches recursively."""
    out = []
    try:
        assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True, include_folder=False)
    except Exception as e:
        unreal.log_error("list_assets failed for {}: {}".format(folder_path, e))
        return out
    for path in assets:
        try:
            obj = unreal.EditorAssetLibrary.load_asset(path)
        except Exception:
            obj = None
        if obj is None:
            continue
        try:
            if obj.get_class().get_name() != "StaticMesh":
                continue
        except Exception:
            continue
        try:
            b = obj.get_bounds()
            ext = b.box_extent
            footprint = 2.0 * max(float(ext.x), float(ext.y))
        except Exception:
            footprint = 0.0
        if footprint < min_footprint:
            continue
        out.append((path, obj, footprint))
    return out


def discover_house_meshes():
    return discover_static_meshes(HOUSES_MESH_PATH, MIN_HOUSE_FOOTPRINT)


def collect_existing_buildings(actors):
    """Return list of (x, y, radius) for existing structure actors to avoid."""
    buildings = []
    for a in actors:
        try:
            cn = a.get_class().get_name()
        except Exception:
            continue
        if not cn.startswith("StaticMeshActor"):
            continue
        try:
            paths = get_static_mesh_paths(a)
        except Exception:
            paths = []
        if not any(any(h in p for h in BUILDING_HINTS) for p in paths):
            continue
        try:
            b = actor_bbox(a)
        except Exception:
            continue
        footprint = max(b[3] - b[0], b[4] - b[1])
        if footprint < EXISTING_FOOTPRINT_MIN:
            continue
        cx = (b[0] + b[3]) / 2.0
        cy = (b[1] + b[4]) / 2.0
        radius = max(b[3] - b[0], b[4] - b[1]) / 2.0
        buildings.append((cx, cy, radius))
    return buildings


def _world_static_obj_type():
    """EObjectTypeQuery value for WorldStatic (enum name varies across UE versions)."""
    for name in ("OBJECT_TYPE_QUERY_WORLD_STATIC", "OBJECT_TYPE_QUERY1"):
        v = getattr(unreal.ObjectTypeQuery, name, None)
        if v is not None:
            return v
    return None


def _draw_debug_none():
    """EDrawDebugTrace value for 'no debug draw' (UE5 uses an enum, not a bool)."""
    for name in ("NONE", "NO_TRACE", "DDT_None", "NONE_"):
        v = getattr(unreal.DrawDebugTrace, name, None)
        if v is not None:
            return v
    return None


def _visibility_trace_query():
    """ETraceTypeQuery value for the Visibility trace channel (member name varies)."""
    for name in ("TRACE_QUERY_VISIBILITY", "VISIBILITY", "TRACE_QUERY1", "TRACE_TYPE_QUERY1"):
        v = getattr(unreal.TraceTypeQuery, name, None)
        if v is not None:
            return v
    return None


def _extract_hit(res):
    """UE5.5 python returns a bare HitResult struct (not a tuple). Pass it through."""
    if res is None:
        return None
    if isinstance(res, tuple):  # defensive: some bindings return (bool, HitResult)
        if len(res) >= 2 and res[0]:
            return res[1]
        return None
    return res


def _hit_z(hit):
    """Read impact Z from a HitResult. The UPROPERTY fields are 'protected' in the
    python binding (getattr/get_editor_property fail), so read via to_tuple().
    UE5.5 FHitResult tuple order: [0]=bBlockingHit, [4]=ImpactPoint, [5]=Location."""
    if hit is None:
        return None
    try:
        t = hit.to_tuple()
    except Exception:
        return None
    if not t or t[0] is not True:  # bBlockingHit
        return None
    for idx in (4, 5):
        if idx < len(t):
            try:
                return float(t[idx].z)
            except Exception:
                continue
    return None


def _hit_actor_name(hit):
    if hit is None:
        return "(none)"
    try:
        t = hit.to_tuple()
    except Exception:
        return "?"
    for idx in (9, 10):  # HitActor, HitComponent
        if idx < len(t):
            el = t[idx]
            try:
                return "{}({})".format(type(el).__name__, el.get_name())
            except Exception:
                continue
    return "(none)"


def trace_ground_z(world, x, y, ignore):
    """Downward object-type trace against WorldStatic to find terrain height.
    `ignore` must contain every non-landscape actor so the trace reaches the
    terrain (ProceduralFoliageVolumes and other blockers sit above it)."""
    start = unreal.Vector(x, y, 60000.0)
    end = unreal.Vector(x, y, -5000.0)
    ddt = _draw_debug_none()
    ot = _world_static_obj_type()
    if ot is None or ddt is None:
        return None
    try:
        otypes = unreal.Array(unreal.ObjectTypeQuery)
        otypes.append(ot)
        res = unreal.SystemLibrary.line_trace_single_for_objects(
            world, start, end, otypes, True, ignore, ddt, False)
        return _hit_z(_extract_hit(res))
    except Exception as e:
        if not trace_ground_z._warned:
            trace_ground_z._warned = True
            unreal.log_warning("line_trace_single_for_objects failed: {}".format(e))
        return None

trace_ground_z._warned = False


def diagnose_trace(world, x, y, ignore):
    """One-shot diagnostic: trace at (x,y) and log hit/z/actor."""
    start = unreal.Vector(x, y, 60000.0)
    end = unreal.Vector(x, y, -5000.0)
    ddt = _draw_debug_none()
    ot = _world_static_obj_type()
    unreal.log("Trace diagnostic at ({:.0f},{:.0f}):".format(x, y))
    if ot is None or ddt is None:
        unreal.log("  enums not resolved (DrawDebugTrace/ObjectTypeQuery)")
        return
    try:
        otypes = unreal.Array(unreal.ObjectTypeQuery)
        otypes.append(ot)
        res = unreal.SystemLibrary.line_trace_single_for_objects(
            world, start, end, otypes, True, ignore, ddt, False)
        hit = _extract_hit(res)
        z = _hit_z(hit)
        unreal.log("  hit={} z={} actor={}".format(
            z is not None, z if z is not None else "None",
            _hit_actor_name(hit) if z is not None else "(no hit)"))
    except Exception as e:
        unreal.log("  EXCEPTION {}".format(e))


def main():
    random.seed(SEED)
    unreal.log("==== populate_desert_houses start (APPLY={}) ====".format(APPLY))
    world = load_world()
    if world is None:
        unreal.log_error("Could not load level " + LEVEL_PATH)
        return

    actors = _all_level_actors()

    # ---- terrain rect from landscape bboxes ----
    lands = [a for a in actors if a.get_class().get_name().startswith("Landscape")]
    if not lands:
        unreal.log_error("No Landscape actors found — cannot determine terrain area.")
        return
    min_x = min_y = float("inf")
    max_x = max_y = float("-inf")
    for a in lands:
        b = actor_bbox(a)
        min_x = min(min_x, b[0]); min_y = min(min_y, b[1])
        max_x = max(max_x, b[3]); max_y = max(max_y, b[4])
    rect = (min_x + EDGE_MARGIN, min_y + EDGE_MARGIN,
            max_x - EDGE_MARGIN, max_y - EDGE_MARGIN)
    unreal.log("Terrain rect (with margin): x[{:.0f},{:.0f}] y[{:.0f},{:.0f}]".format(
        rect[0], rect[2], rect[1], rect[3]))

    # Build the trace ignore-list: every actor that ISN'T a landscape. The
    # downward trace must reach the terrain, but ProceduralFoliageVolumes (and
    # any other blockers) sit above it and would intercept the trace. Ignoring
    # all non-landscape actors guarantees the hit is the terrain surface.
    ignore = unreal.Array(unreal.Actor)
    n_ignore = 0
    for a in actors:
        try:
            cn = a.get_class().get_name()
        except Exception:
            cn = ""
        if not cn.startswith("Landscape"):
            ignore.append(a)
            n_ignore += 1
    unreal.log("Trace ignore-list: {} non-landscape actors.".format(n_ignore))

    # one-shot trace diagnostic at the terrain center
    diagnose_trace(world, (min_x + max_x) / 2.0, (min_y + max_y) / 2.0, ignore)

    # ---- discover house meshes ----
    meshes = discover_house_meshes()
    if not meshes:
        unreal.log_error("No house meshes found under {}.".format(HOUSES_MESH_PATH))
        return
    unreal.log("Discovered {} house meshes (footprint >= {:.0f} cm).".format(len(meshes), MIN_HOUSE_FOOTPRINT))

    # ---- discover rock meshes (only used in cluster mode) ----
    rock_meshes = []
    if USE_CLUSTERS:
        rock_meshes = discover_static_meshes(ROCKS_MESH_PATH, ROCK_MIN_FOOTPRINT)
        unreal.log("Discovered {} rock meshes (footprint >= {:.0f} cm) at {}.".format(
            len(rock_meshes), ROCK_MIN_FOOTPRINT, ROCKS_MESH_PATH))

    # ---- existing buildings to avoid ----
    buildings = collect_existing_buildings(actors)
    unreal.log("Existing structures to avoid: {}.".format(len(buildings)))

    # ---- sampling ----
    # points items: (x, y, gz, cluster_id). cluster_id == -1 for single-house mode.
    lo_x, lo_y, hi_x, hi_y = rect
    points = []
    rejected_clearance = rejected_spacing = rejected_intra = rejected_cluster_spacing = trace_miss = 0
    attempts = 0

    def _place_house_at(x, y, cluster_id):
        """Try to place one house at (x,y) inside cluster `cluster_id`.
        Returns (x,y,gz) on success, None on rejection (updates counters)."""
        nonlocal rejected_clearance, rejected_spacing, rejected_intra, trace_miss
        # clearance from existing buildings
        for bx, by, br in buildings:
            if math.hypot(x - bx, y - by) < br + BUILDING_CLEARANCE:
                rejected_clearance += 1
                return None
        # spacing against all already-placed houses
        for px, py, _pgz, _pcid in points:
            del _pgz  # gz not needed in this loop body
            d = math.hypot(x - px, y - py)
            same_cluster = (cluster_id >= 0 and _pcid == cluster_id)
            if same_cluster:
                # within a village: tight spacing so houses form a cluster
                if d < INTRA_CLUSTER_SPACING:
                    rejected_intra += 1
                    return None
            else:
                # between villages (or single-house mode): wide spacing
                if d < MIN_SPACING:
                    rejected_spacing += 1
                    return None
        # ground height
        gz = trace_ground_z(world, x, y, ignore)
        if gz is None:
            trace_miss += 1
            return None
        points.append((x, y, gz, cluster_id))
        return (x, y, gz)

    if USE_CLUSTERS:
        # 1) pick village centroids
        centroids = []  # (cx, cy)
        while len(centroids) < CLUSTER_COUNT and attempts < MAX_TOTAL_ATTEMPTS:
            attempts += 1
            cx = random.uniform(lo_x, hi_x)
            cy = random.uniform(lo_y, hi_y)
            # existing-city clearance
            ok = True
            for bx, by, br in buildings:
                if math.hypot(cx - bx, cy - by) < br + BUILDING_CLEARANCE:
                    ok = False; break
            if not ok:
                rejected_clearance += 1
                continue
            # inter-village spacing
            ok = True
            for pcx, pcy in centroids:
                if math.hypot(cx - pcx, cy - pcy) < CLUSTER_SPACING:
                    ok = False; break
            if not ok:
                rejected_cluster_spacing += 1
                continue
            centroids.append((cx, cy))
        unreal.log("Sampled {}/{} village centroids in {} attempts. rejected: clearance={} cluster_spacing={}".format(
            len(centroids), CLUSTER_COUNT, attempts, rejected_clearance, rejected_cluster_spacing))

        # 2) for each centroid, scatter HOUSES_PER_CLUSTER houses around it
        for cid, (cx, cy) in enumerate(centroids):
            n_houses = random.randint(*HOUSES_PER_CLUSTER)
            placed_in_cluster = 0
            per_cluster_attempts = 0
            per_cluster_cap = n_houses * 30  # give up on a village if it can't fill
            while placed_in_cluster < n_houses and per_cluster_attempts < per_cluster_cap and attempts < MAX_TOTAL_ATTEMPTS:
                attempts += 1
                per_cluster_attempts += 1
                # angle in [0, 2π), radius biased toward the center (sqrt) for a tighter feel
                ang = random.uniform(0.0, 2.0 * math.pi)
                r = math.sqrt(random.random()) * CLUSTER_RADIUS
                hx = cx + r * math.cos(ang)
                hy = cy + r * math.sin(ang)
                if _place_house_at(hx, hy, cid) is not None:
                    placed_in_cluster += 1
        unreal.log("Sampled {}/{} houses total across {} villages.".format(
            len(points), sum(HOUSES_PER_CLUSTER) // 2 * len(centroids) if not points else len(points),
            len(centroids)))
    else:
        # single-house mode (legacy)
        while len(points) < TARGET_COUNT and attempts < MAX_TOTAL_ATTEMPTS:
            attempts += 1
            x = random.uniform(lo_x, hi_x)
            y = random.uniform(lo_y, hi_y)
            if _place_house_at(x, y, -1) is not None:
                pass

    # Rejection summary
    if USE_CLUSTERS:
        target_total = CLUSTER_COUNT * ((HOUSES_PER_CLUSTER[0] + HOUSES_PER_CLUSTER[1]) // 2)
        unreal.log("Sampling done: {} houses in {} clusters (target ~{}). rejected: clearance={} spacing={} intra={} cluster_spacing={} trace_miss={}".format(
            len(points), len(centroids), target_total,
            rejected_clearance, rejected_spacing, rejected_intra, rejected_cluster_spacing, trace_miss))
    else:
        unreal.log("Sampled {}/{} houses in {} attempts. rejected: clearance={} spacing={} trace_miss={}".format(
            len(points), TARGET_COUNT, attempts, rejected_clearance, rejected_spacing, trace_miss))
    if not points:
        unreal.log_warning("No valid placement points found. Try relaxing CLUSTER_SPACING / BUILDING_CLEARANCE / CLUSTER_RADIUS.")
        return

    # ---- spawn ----
    manifest = []
    smin, smax = SCALE_RANGE
    last_cluster = None
    for i, (x, y, gz, cluster_id) in enumerate(points):
        if USE_CLUSTERS and cluster_id != last_cluster:
            last_cluster = cluster_id
            unreal.log("  --- cluster {:2d} ---".format(cluster_id))
        path, mesh, footprint = random.choice(meshes)
        # Consume the yaw draw every iteration so the RNG stream stays in sync
        # with the prior dry run (mesh + spacing/clearance sampling are already
        # deterministic from SEED). If RANDOM_YAW is off, the yaw value is
        # discarded and every house is placed axis-aligned (yaw=0).
        _yaw_draw = random.uniform(0.0, 360.0)
        yaw = _yaw_draw if RANDOM_YAW else 0.0
        rot = unreal.Rotator(0.0, yaw, 0.0)
        if APPLY:
            try:
                actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                    unreal.StaticMeshActor.static_class(),
                    unreal.Vector(x, y, gz), rot, transient=False)
            except Exception as e:
                unreal.log_error("spawn failed for #{} ({}): {}".format(i, path, e))
                continue
            if actor is None:
                unreal.log_error("spawn returned None for #{} ({})".format(i, path))
                continue
            try:
                name = actor.get_name()
            except Exception:
                name = "house_{:03d}".format(i)
            # assign mesh
            try:
                comps = actor.get_components_by_class(unreal.StaticMeshComponent)
                if comps:
                    comps[0].set_static_mesh(mesh)
                else:
                    unreal.log_warning("  {} has no StaticMeshComponent".format(name))
            except Exception as e:
                unreal.log_warning("  {} set_static_mesh failed: {}".format(name, e))
            # scale (before settling so bbox reflects scale)
            if smax > smin:
                s = random.uniform(smin, smax)
                try:
                    actor.set_actor_scale3d(unreal.Vector(s, s, s))
                except Exception as e:
                    unreal.log_warning("  {} set_scale failed: {}".format(name, e))
            # settle base onto ground regardless of pivot
            try:
                o, e = actor.get_actor_bounds(only_colliding_components=False)
                bottom_z = o.z - e.z
                loc = actor.get_actor_location()
                shift = gz - bottom_z
                if abs(shift) > 0.5:
                    actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + shift), False, True)
            except Exception as ex:
                unreal.log_warning("  {} settle failed: {}".format(name, ex))
            try:
                final_loc = actor.get_actor_location()
            except Exception:
                final_loc = None
            manifest.append({
                "actor": name,
                "mesh": path,
                "loc": [float(final_loc.x), float(final_loc.y), float(final_loc.z)] if final_loc else [x, y, gz],
                "yaw": yaw,
                "cluster_id": int(cluster_id) if USE_CLUSTERS else -1,
            })
            unreal.log("  #{:2d} {:24s} {:30s} loc=({:8.0f},{:8.0f},{:7.0f}) yaw={:5.1f} cluster={}".format(
                i, name, path, x, y, gz, yaw, cluster_id))
        else:
            unreal.log("  #{:2d} (dry-run) {:30s} loc=({:8.0f},{:8.0f},{:7.0f}) yaw={:5.1f} cluster={}".format(
                i, path, x, y, gz, yaw, cluster_id))

    # ---- rock ring around each cluster ----
    # Houses went into `points` with cluster_id >= 0; sample N rocks per cluster
    # in an annulus around the centroid (inner/outer radius scaled from
    # CLUSTER_RADIUS so the ring tracks the village size automatically).
    n_rocks_placed = 0
    # Per-cluster house/rock lookups — built unconditionally so the PlayerStart
    # pass (which runs even when there are no rock meshes) can avoid them.
    houses_by_cluster = {}
    rocks_by_cluster = {}
    if USE_CLUSTERS:
        for px, py, _pgz, pcid in points:
            del _pgz
            houses_by_cluster.setdefault(pcid, []).append((px, py))
    if USE_CLUSTERS and rock_meshes and centroids:
        # houses_by_cluster / rocks_by_cluster are initialized above the rock
        # pass so the PlayerStart pass can reuse them too.
        ring_inner = CLUSTER_RADIUS * ROCK_RING_INNER_MULT
        ring_outer = CLUSTER_RADIUS * ROCK_RING_OUTER_MULT
        unreal.log("Rock ring: inner={:.0f}cm  outer={:.0f}cm  per cluster={}".format(
            ring_inner, ring_outer, ROCK_COUNT_PER_CLUSTER))

        for cid, (cx, cy) in enumerate(centroids):
            n_rocks = random.randint(*ROCK_COUNT_PER_CLUSTER)
            rocks_here = []  # local list for the rock↔rock spacing check
            per_cluster_attempts = 0
            per_cluster_cap = n_rocks * 40
            placed = 0
            while placed < n_rocks and per_cluster_attempts < per_cluster_cap:
                per_cluster_attempts += 1
                # sample in an annulus: r is uniform in [inner, outer]
                ang = random.uniform(0.0, 2.0 * math.pi)
                r = random.uniform(ring_inner, ring_outer)
                rx = cx + r * math.cos(ang)
                ry = cy + r * math.sin(ang)
                # clearance from existing buildings (city)
                too_close = False
                for bx, by, br in buildings:
                    if math.hypot(rx - bx, ry - by) < br + BUILDING_CLEARANCE:
                        too_close = True
                        break
                if too_close:
                    continue
                # rock↔rock spacing (local to this cluster)
                too_close = False
                for ox, oy in rocks_here:
                    if math.hypot(rx - ox, ry - oy) < ROCK_TO_ROCK_SPACING:
                        too_close = True
                        break
                if too_close:
                    continue
                # rock↔house spacing (against houses in the same cluster, plus
                # any other cluster's houses)
                too_close = False
                for hx, hy in houses_by_cluster.get(cid, []):
                    if math.hypot(rx - hx, ry - hy) < ROCK_TO_HOUSE_SPACING:
                        too_close = True
                        break
                if too_close:
                    continue
                # ground height
                gz = trace_ground_z(world, rx, ry, ignore)
                if gz is None:
                    continue
                # pick a random rock mesh
                rpath, rmesh, _ = random.choice(rock_meshes)
                # Rocks respect RANDOM_YAW just like houses. Consume the yaw draw
                # unconditionally so the RNG stream (and thus rock positions/mesh
                # picks) stays in sync with the prior dry run regardless of the
                # toggle.
                _yaw_draw = random.uniform(0.0, 360.0)
                yaw = _yaw_draw if RANDOM_YAW else 0.0
                rot = unreal.Rotator(0.0, yaw, 0.0)
                if APPLY:
                    try:
                        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                            unreal.StaticMeshActor.static_class(),
                            unreal.Vector(rx, ry, gz), rot, transient=False)
                    except Exception as e:
                        unreal.log_warning("  rock spawn failed: {}".format(e))
                        continue
                    if actor is None:
                        continue
                    try:
                        rname = actor.get_name()
                    except Exception:
                        rname = "rock_{:03d}".format(n_rocks_placed)
                    try:
                        rcomps = actor.get_components_by_class(unreal.StaticMeshComponent)
                        if rcomps:
                            rcomps[0].set_static_mesh(rmesh)
                    except Exception as e:
                        unreal.log_warning("  {} rock set_static_mesh failed: {}".format(rname, e))
                    # settle base onto ground (rocks often have bottom-pivot)
                    try:
                        o, e = actor.get_actor_bounds(only_colliding_components=False)
                        bottom_z = o.z - e.z
                        loc = actor.get_actor_location()
                        shift = gz - bottom_z
                        if abs(shift) > 0.5:
                            actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + shift), False, True)
                    except Exception:
                        pass
                    try:
                        fl = actor.get_actor_location()
                        finalloc = [float(fl.x), float(fl.y), float(fl.z)]
                    except Exception:
                        finalloc = [rx, ry, gz]
                    manifest.append({
                        "actor": rname,
                        "mesh": rpath,
                        "loc": finalloc,
                        "yaw": yaw,
                        "cluster_id": int(cid),
                        "kind": "rock",
                    })
                    unreal.log("  rock     {:24s} {:30s} loc=({:8.0f},{:8.0f},{:7.0f}) yaw={:5.1f} cluster={}".format(
                        rname, rpath, rx, ry, gz, yaw, cid))
                else:
                    manifest.append({
                        "actor": "(dry-run)",
                        "mesh": rpath,
                        "loc": [rx, ry, gz],
                        "yaw": yaw,
                        "cluster_id": int(cid),
                        "kind": "rock",
                    })
                    unreal.log("  rock (dry-run)              {:30s} loc=({:8.0f},{:8.0f},{:7.0f}) yaw={:5.1f} cluster={}".format(
                        rpath, rx, ry, gz, yaw, cid))
                rocks_here.append((rx, ry))
                rocks_by_cluster.setdefault(cid, []).append((rx, ry))
                n_rocks_placed += 1
                placed += 1
            if placed < n_rocks:
                unreal.log_warning("  cluster {:2d}: only placed {}/{} rocks after {} attempts".format(
                    cid, placed, n_rocks, per_cluster_attempts))
        unreal.log("Rock ring pass: placed {} rocks across {} clusters (target {}-{} per cluster).".format(
            n_rocks_placed, len(centroids), ROCK_COUNT_PER_CLUSTER[0], ROCK_COUNT_PER_CLUSTER[1]))

    # ---- PlayerStart per village ----
    # Place one untagged PlayerStart near each village centroid. The game mode
    # (AAuraGameModeBase::ChoosePlayerStart) picks randomly among ALL PlayerStarts
    # when no tag matches the active spawn tag — i.e. the fresh-game case — so the
    # player is born in a random village with no tagging required. (Tag matching
    # still handles checkpoint / portal respawns, which use a non-default tag.)
    n_player_starts = 0
    if USE_CLUSTERS and ADD_PLAYER_START_PER_CLUSTER and centroids:
        unreal.log("Placing PlayerStart near each of {} village centroids...".format(len(centroids)))
        placed_player_starts = []  # (cid, name, x, y, z, offset_from_centroid) for the summary
        for cid, (cx, cy) in enumerate(centroids):
            # search for a clear spot near the centroid
            spot = None
            spot_try = -1  # which candidate index cleared (0 = centroid itself)
            candidates = [(cx, cy)]
            for _ in range(PLAYER_START_SEARCH_TRIES):
                ang = random.uniform(0.0, 2.0 * math.pi)
                r = math.sqrt(random.random()) * PLAYER_START_MAX_OFFSET
                candidates.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
            for try_idx, (sx, sy) in enumerate(candidates):
                ok = True
                for hx, hy in houses_by_cluster.get(cid, []):
                    if math.hypot(sx - hx, sy - hy) < PLAYER_START_CLEAR_RADIUS:
                        ok = False; break
                if ok:
                    for rx, ry in rocks_by_cluster.get(cid, []):
                        if math.hypot(sx - rx, sy - ry) < PLAYER_START_CLEAR_RADIUS:
                            ok = False; break
                if ok:
                    spot = (sx, sy)
                    spot_try = try_idx
                    break
            if spot is None:
                spot = (cx, cy)  # fallback to the centroid
                spot_try = 0
            sx, sy = spot
            offset = math.hypot(sx - cx, sy - cy)
            gz = trace_ground_z(world, sx, sy, ignore)
            if gz is None:
                unreal.log_warning("  cluster {:2d}: PlayerStart trace missed at ({:.0f},{:.0f}), skipping".format(cid, sx, sy))
                continue
            pz = gz + PLAYER_START_Z_OFFSET
            rot = unreal.Rotator(0.0, 0.0, 0.0)
            if APPLY:
                try:
                    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                        unreal.PlayerStart.static_class(),
                        unreal.Vector(sx, sy, pz), rot, transient=False)
                except Exception as e:
                    unreal.log_error("  cluster {:2d}: PlayerStart spawn failed: {}".format(cid, e))
                    continue
                if actor is None:
                    unreal.log_error("  cluster {:2d}: PlayerStart spawn returned None".format(cid))
                    continue
                try:
                    ps_name = actor.get_name()
                except Exception:
                    ps_name = "PlayerStart_{:02d}".format(cid)
                try:
                    fl = actor.get_actor_location()
                    finalloc = [float(fl.x), float(fl.y), float(fl.z)]
                except Exception:
                    finalloc = [sx, sy, pz]
                manifest.append({
                    "actor": ps_name,
                    "mesh": "",
                    "loc": finalloc,
                    "yaw": 0.0,
                    "cluster_id": int(cid),
                    "kind": "player_start",
                })
                n_player_starts += 1
                placed_player_starts.append((cid, ps_name, finalloc[0], finalloc[1], finalloc[2], offset, spot_try))
                unreal.log("  PlayerStart {:24s} loc=({:8.0f},{:8.0f},{:7.0f}) cluster={:2d} offset={:5.0f}cm (try {})".format(
                    ps_name, sx, sy, pz, cid, offset, spot_try))
            else:
                manifest.append({
                    "actor": "(dry-run)",
                    "mesh": "",
                    "loc": [sx, sy, pz],
                    "yaw": 0.0,
                    "cluster_id": int(cid),
                    "kind": "player_start",
                })
                n_player_starts += 1
                placed_player_starts.append((cid, "(dry-run)", sx, sy, pz, offset, spot_try))
                unreal.log("  PlayerStart (dry-run)         loc=({:8.0f},{:8.0f},{:7.0f}) cluster={:2d} offset={:5.0f}cm (try {})".format(
                    sx, sy, pz, cid, offset, spot_try))
        unreal.log("PlayerStart pass: placed {} across {} clusters.".format(n_player_starts, len(centroids)))
        # Summary list — correlates 1:1 with the [PlayerStart] village[i] logs the
        # game mode prints at spawn time, so you can confirm which village the
        # player actually spawned in.
        if placed_player_starts:
            unreal.log("---- PlayerStart summary (matches runtime [PlayerStart] village logs) ----")
            for i, (cid, nm, fx, fy, fz, off, _try) in enumerate(placed_player_starts):
                unreal.log("  [{:2d}] cluster={:2d} {:24s} loc=({:8.0f},{:8.0f},{:7.0f}) offset={:5.0f}cm".format(
                    i, cid, nm, fx, fy, fz, off))
            unreal.log("---- PlayerStart summary: {} villages available for random spawn ----".format(
                len(placed_player_starts)))

    if APPLY:
        proj = unreal.SystemLibrary.get_project_directory()
        path = os.path.join(proj, MANIFEST_FILE_NAME)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(manifest, f, indent=2)
        n_houses = len(points)  # houses planned/sampled this run
        n_houses_in_manifest = sum(1 for e in manifest if e.get("kind") not in ("rock", "player_start"))
        n_player_starts_in_manifest = sum(1 for e in manifest if e.get("kind") == "player_start")
        unreal.log("Placed {} houses + {} rocks + {} player_starts. Manifest: {}".format(
            n_houses_in_manifest, n_rocks_placed, n_player_starts_in_manifest, path))
        unreal.log("Remember to SAVE THE LEVEL (Ctrl+S) to persist.")
    else:
        n_dry_houses = len(points)
        n_dry_player_starts = sum(1 for e in manifest if e.get("kind") == "player_start")
        unreal.log("DRY RUN — no actors spawned. ({} houses + {} rocks + {} player_starts planned) Set APPLY=True and re-run to place them.".format(
            n_dry_houses, n_rocks_placed, n_dry_player_starts))
    unreal.log("==== populate_desert_houses done ====")


main()
