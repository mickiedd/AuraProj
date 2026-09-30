"""Reload and validate the adaptive wall modules and foundation edit layer."""
from __future__ import annotations

import hashlib
import json
import math
import struct
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL"
PROFILE = ROOT / "Data/Canton_WalledCity_Provisional_Wall_Foundation_Profile.json"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
OUT = ROOT / "QA/Canton_Continuation/Walled_Wall_Terrain_Fit_Validation.json"
MAX_MODULE_CM = 250.0
WALL_HEIGHT_CM = 1000.0


def load_ground():
    heights = memoryview(RAW.read_bytes()).cast("H")

    def ground(x, y):
        fx = max(0.0, min(2016.0, float(x) / 200.0))
        fy = max(0.0, min(2016.0, float(y) / 200.0))
        x0, y0 = int(math.floor(fx)), int(math.floor(fy))
        x1, y1 = min(x0 + 1, 2016), min(y0 + 1, 2016)
        u, v = fx - x0, fy - y0

        def h(ix, iy):
            return (heights[iy * 2017 + ix] - 32768) * 50.0 / 128.0

        return ((1 - u) * (1 - v) * h(x0, y0) + u * (1 - v) * h(x1, y0) +
                (1 - u) * v * h(x0, y1) + u * v * h(x1, y1))

    return ground


def segments_from_profile(profile):
    return [((float(row["a_cm"][0]), float(row["a_cm"][1])),
             (float(row["b_cm"][0]), float(row["b_cm"][1])))
            for row in profile["segments"]]


def fitted_ground(x, y, raw, segments, half_width, blend):
    best = None
    for a, b in segments:
        ax, ay, bx, by = a[0], a[1], b[0], b[1]
        dx, dy = bx - ax, by - ay
        denom = dx * dx + dy * dy
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / denom)) if denom else 0.0
        point = (ax + dx * t, ay + dy * t)
        distance = math.hypot(x - point[0], y - point[1])
        if best is None or distance < best[0]:
            best = (distance, point)
    distance, point = best
    base = raw(x, y)
    if distance > half_width + blend:
        return base
    weight = 1.0
    if distance > half_width:
        weight = 0.5 * (1.0 + math.cos(math.pi * (distance - half_width) / blend))
    return base + max(-300.0, min(300.0, (raw(*point) - base) * weight))


def module_endpoints(actor):
    location = actor.get_actor_location()
    scale = actor.get_actor_scale3d()
    rotation = actor.get_actor_rotation()
    length = abs(float(scale.x)) * 100.0
    half = length * 0.5
    yaw = math.radians(float(rotation.yaw))
    pitch = math.radians(float(rotation.pitch))
    dx, dy = math.cos(yaw), math.sin(yaw)
    dz = math.sin(pitch) * half
    bottom_offset = math.cos(pitch) * WALL_HEIGHT_CM * 0.5
    return ((location.x - dx * half, location.y - dy * half, location.z - dz - bottom_offset),
            (location.x + dx * half, location.y + dy * half, location.z + dz - bottom_offset))


def run():
    profile = json.loads(PROFILE.read_text())
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    native = json.loads(unreal.CantonTerrainLibrary.validate_provisional_wall_foundation(world))
    assert native["passed"], native
    modules = [a for a in actors if a.get_actor_label().startswith("PROVISIONAL_WallAdaptive_")]
    old = [a for a in actors if a.get_actor_label().startswith("PROVISIONAL_Wall_") or
           a.get_actor_label().startswith("PROVISIONAL_WallFit_")]
    gates = [a for a in actors if a.get_actor_label().startswith("Canton_Provisional_")]
    assert modules and not old
    raw = load_ground()
    segments = segments_from_profile(profile)
    half_width = float(profile["corridor_half_width_cm"])
    blend = float(profile["blend_width_cm"])
    max_gap = -1e9
    max_penetration = 0.0
    gaps = 0
    collision_missing = 0
    lengths = []
    for actor in modules:
        length = abs(float(actor.get_actor_scale3d().x)) * 100.0
        lengths.append(length)
        if length > MAX_MODULE_CM + 0.01:
            raise AssertionError((actor.get_actor_label(), length))
        if abs(abs(float(actor.get_actor_scale3d().z)) * 100.0 - WALL_HEIGHT_CM) > 0.01:
            raise AssertionError((actor.get_actor_label(), actor.get_actor_scale3d().z))
        if not actor.get_actor_enable_collision():
            collision_missing += 1
        for x, y, bottom in module_endpoints(actor):
            terrain = fitted_ground(x, y, raw, segments, half_width, blend)
            gap = float(bottom) - terrain
            max_gap = max(max_gap, gap)
            max_penetration = max(max_penetration, -gap)
            if gap > 2.0:
                gaps += 1
    report = {
        "passed": gaps == 0 and collision_missing == 0 and max_gap <= 2.0,
        "map": MAP,
        "historically_accepted": False,
        "landscape_edit_layers": ["Base_Imported", "Wall_Foundation_Adjustment"],
        "native_reload_check": native,
        "adaptive_module_count": len(modules),
        "old_coarse_wall_count": len(old),
        "gate_label_count": len(gates),
        "max_module_length_cm": max(lengths),
        "wall_height_cm": WALL_HEIGHT_CM,
        "max_bottom_gap_cm": round(max_gap, 3),
        "max_buried_penetration_cm": round(max_penetration, 3),
        "positive_gap_endpoint_count": gaps,
        "collision_missing_count": collision_missing,
        "raw_sha256": hashlib.sha256(RAW.read_bytes()).hexdigest(),
    }
    OUT.write_text(json.dumps(report, indent=2) + "\n")
    assert report["passed"], report
    unreal.log("CANTON_WALL_TERRAIN_FIT_VALIDATION " + json.dumps(report))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({"passed": False, "error": traceback.format_exc()}, indent=2) + "\n")
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
