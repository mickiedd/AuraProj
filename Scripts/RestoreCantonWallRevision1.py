"""Restore the Revision 1 provisional wall: the adaptive box-module chain.

Revision 2 replaced the box chain with one continuous mesh.  This script puts
the boxes back: it re-applies the original foundation corridor profile and
rebuilds the same modules from the source endpoints the Revision 1 run recorded
in ``QA/Canton_Continuation/Walled_Wall_Terrain_Fit.json``.

The module placement below is the Revision 1 recipe, including the later height
doubling: a module is created at 500 cm and then grown upward by
``250 * cos(pitch)`` while keeping its buried base, so the world bottom stays at
``base - 140``.  That is folded into the single expression used here.
"""
from __future__ import annotations

import hashlib
import json
import math
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL"
PROFILE = ROOT / "Data/Canton_WalledCity_Provisional_Wall_Foundation_Profile.json"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
SOURCE = ROOT / "QA/Canton_Continuation/Walled_Wall_Terrain_Fit.json"
OUT = ROOT / "QA/Canton_Continuation/Walled_Restore_Revision1.json"

MAX_MODULE_CM = 250.0
WALL_WIDTH_CM = 400.0
WALL_HEIGHT_CM = 1000.0
WALL_EMBED_CM = 140.0
CONTINUOUS_LABEL = "Canton_ContinuousCityWall"


def raw_height_loader():
    data = RAW.read_bytes()
    heights = memoryview(data).cast("H")

    def sample(x, y):
        fx = max(0.0, min(2016.0, float(x) / 200.0))
        fy = max(0.0, min(2016.0, float(y) / 200.0))
        x0, y0 = int(math.floor(fx)), int(math.floor(fy))
        x1, y1 = min(x0 + 1, 2016), min(y0 + 1, 2016)
        u, v = fx - x0, fy - y0

        def h(ix, iy):
            return (heights[iy * 2017 + ix] - 32768) * 50.0 / 128.0

        return ((1.0 - u) * (1.0 - v) * h(x0, y0) + u * (1.0 - v) * h(x1, y0) +
                (1.0 - u) * v * h(x0, y1) + u * v * h(x1, y1))

    return sample


def profile_segments(profile):
    return [((float(row["a_cm"][0]), float(row["a_cm"][1])),
             (float(row["b_cm"][0]), float(row["b_cm"][1])))
            for row in profile["segments"]]


def closest(point, segments):
    px, py = point
    best = None
    for ax, ay, bx, by in ((a[0], a[1], b[0], b[1]) for a, b in segments):
        dx, dy = bx - ax, by - ay
        length_sq = dx * dx + dy * dy
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq)) if length_sq else 0.0
        cx, cy = ax + dx * t, ay + dy * t
        dsq = (px - cx) ** 2 + (py - cy) ** 2
        if best is None or dsq < best[0]:
            best = (dsq, (cx, cy))
    return math.sqrt(best[0]), best[1]


def fitted_height(x, y, raw_ground, segments, half_width, blend):
    distance, nearest = closest((x, y), segments)
    if distance > half_width + blend:
        return raw_ground(x, y)
    centerline = raw_ground(*nearest)
    base = raw_ground(x, y)
    weight = 1.0
    if distance > half_width:
        t = max(0.0, min(1.0, (distance - half_width) / blend))
        weight = 0.5 * (1.0 + math.cos(math.pi * t))
    return base + max(-300.0, min(300.0, (centerline - base) * weight))


def configure_module(actor, label, a, b, raw_ground, segments, half_width, blend, cube, material):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    middle = ((ax + bx) * 0.5, (ay + by) * 0.5)
    center_ground = fitted_height(*middle, raw_ground, segments, half_width, blend)
    endpoint_ground = [fitted_height(ax, ay, raw_ground, segments, half_width, blend),
                       fitted_height(bx, by, raw_ground, segments, half_width, blend)]
    pitch = math.degrees(math.atan2(endpoint_ground[1] - endpoint_ground[0], max(length, 1.0)))
    normal = (-dy / max(length, 1.0), dx / max(length, 1.0))
    cross_ground = [fitted_height(middle[0] + normal[0] * offset,
                                  middle[1] + normal[1] * offset,
                                  raw_ground, segments, half_width, blend)
                    for offset in (-WALL_WIDTH_CM * 0.5, 0.0, WALL_WIDTH_CM * 0.5)]
    base = max([center_ground, endpoint_ground[0], endpoint_ground[1]] + cross_ground)
    # Revision 1 built this at 500 cm, then the height-doubling pass raised the
    # actor by half the old height along its own axis and rescaled Z to 1000 cm.
    # Both steps folded together, base preserved:
    z = base + (500.0 * 0.5) - WALL_EMBED_CM + (500.0 * 0.5) * math.cos(math.radians(pitch))
    actor.set_actor_label(label)
    actor.set_actor_location(unreal.Vector(middle[0], middle[1], z), False, False)
    actor.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0), False)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, WALL_WIDTH_CM / 100.0,
                                          WALL_HEIGHT_CM / 100.0))
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_enable_collision(True)
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    actor.set_editor_property("is_spatially_loaded", True)
    actor.tags = ["Canton.ProvisionalWallAdaptive", "Canton.HistoricalXY.Unverified"]
    actor.set_folder_path("Canton/PROVISIONAL_Wall_Terrain_Fit")
    return {"label": label, "length_cm": round(length, 3),
            "bottom_world_z_cm": round(z - (WALL_HEIGHT_CM * 0.5) * math.cos(math.radians(pitch)), 3),
            "pitch_deg": round(pitch, 4)}


def run():
    assert PROFILE.exists() and RAW.exists() and SOURCE.exists(), (PROFILE, RAW, SOURCE)
    profile = json.loads(PROFILE.read_text())
    assert profile["historically_accepted"] is False
    assert hashlib.sha256(RAW.read_bytes()).hexdigest() == json.loads(
        (ROOT / "Data/Canton_Prototype_Contract.json").read_text())["raw_sha256"]
    source_records = json.loads(SOURCE.read_text())["source_actor_endpoints"]
    assert len(source_records) == 474, len(source_records)

    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)

    foundation = json.loads(unreal.CantonTerrainLibrary.apply_provisional_wall_foundation(
        world, str(PROFILE)))
    assert foundation["passed"], foundation

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    continuous = [a for a in all_actors
                  if a.get_actor_label() == CONTINUOUS_LABEL or
                  "Canton.ProvisionalWallContinuous" in [str(t) for t in a.tags]]
    existing = [a for a in all_actors if a.get_actor_label().startswith("PROVISIONAL_WallAdaptive_")]
    for actor in existing + continuous:
        assert actors.destroy_actor(actor)

    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    material = unreal.EditorAssetLibrary.load_asset("/Game/Canton/Provisional/Terrain/M_WallMarkers_PROVISIONAL")
    assert cube and material
    raw_ground = raw_height_loader()
    segments = profile_segments(profile)
    half_width = float(profile["corridor_half_width_cm"])
    blend = float(profile["blend_width_cm"])

    placed = []
    module_index = 0
    for source in source_records:
        ax, ay = source["a_cm"]
        bx, by = source["b_cm"]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy)
        count = max(1, int(math.ceil(length / MAX_MODULE_CM)))
        for part in range(count):
            t0, t1 = part / count, (part + 1) / count
            a = (ax + dx * t0, ay + dy * t0)
            b = (ax + dx * t1, ay + dy * t1)
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                                  unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(),
                                                  transient=False)
            assert actor
            label = f"PROVISIONAL_WallAdaptive_{module_index:05d}"
            placed.append(configure_module(actor, label, a, b, raw_ground, segments,
                                           half_width, blend, cube, material))
            module_index += 1

    assert placed and all(item["length_cm"] <= MAX_MODULE_CM + 0.01 for item in placed)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    packages = [p for p in set(packages) if "/Canton/Provisional/" in p.get_name()]
    if packages:
        unreal.EditorLoadingAndSavingUtils.save_packages(packages, False)

    validation = json.loads(unreal.CantonTerrainLibrary.validate_provisional_wall_foundation(world))
    assert validation["passed"], validation
    report = {
        "passed": True,
        "map": MAP,
        "historically_accepted": False,
        "foundation": foundation,
        "destroyed_continuous_actors": len(continuous),
        "replaced_adaptive_actors": len(existing),
        "adaptive_module_count": len(placed),
        "max_module_length_cm": MAX_MODULE_CM,
        "wall_width_cm": WALL_WIDTH_CM,
        "wall_height_cm": WALL_HEIGHT_CM,
        "wall_embed_cm": WALL_EMBED_CM,
        "native_validation": validation,
        "modules": placed,
    }
    OUT.write_text(json.dumps(report, indent=2) + "\n")
    unreal.log("CANTON_WALL_RESTORE_REV1 " + json.dumps({
        "passed": True, "adaptive_modules": len(placed),
        "destroyed_continuous": len(continuous)}))


try:
    run()
except Exception:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({"passed": False, "error": traceback.format_exc()}, indent=2) + "\n")
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
