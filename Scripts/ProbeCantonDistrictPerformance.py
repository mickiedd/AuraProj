"""Editor-only 1080p fixed-route performance probe; not a packaged-build acceptance test."""
import json
import math
import os
import resource
import statistics
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
heights = memoryview(RAW.read_bytes()).cast("H")
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
started = time.monotonic()
warmup_seconds, sample_seconds = 60, 180
samples = []
camera = None


def z_at(x, y):
    fx, fy = x / 200, y / 200
    col, row = math.floor(fx), math.floor(fy)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u)*(1-v)*sample(col,row) + u*(1-v)*sample(col+1,row) +
            (1-u)*v*sample(col,row+1) + u*v*sample(col+1,row+1))


def percentile(values, fraction):
    ordered = sorted(values)
    rank = (len(ordered)-1) * fraction
    low = math.floor(rank)
    high = math.ceil(rank)
    return ordered[low] + (ordered[high]-ordered[low]) * (rank-low)


def tick(delta_seconds):
    global camera
    try:
        elapsed = time.monotonic() - started
        if camera is None:
            target = unreal.RenderingLibrary.create_render_target2d(
                world, 1920, 1080, unreal.TextureRenderTargetFormat.RTF_RGBA8)
            camera = actor_api.spawn_actor_from_class(unreal.SceneCapture2D,
                unreal.Vector(180000, 123500, z_at(180000,123500)+180), unreal.Rotator())
            camera.capture_component2d.set_editor_property("texture_target", target)
            camera.capture_component2d.set_editor_property("capture_every_frame", True)
            camera.capture_component2d.set_editor_property("fov_angle", 65)
            for group in ("ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination",
                          "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"):
                unreal.SystemLibrary.execute_console_command(world, "sg.%sQuality 2" % group)
        progress = (elapsed / warmup_seconds if elapsed < warmup_seconds else
                    min(1.0, (elapsed - warmup_seconds) / sample_seconds))
        y = 123500 + progress * (138000-123500)
        eye = unreal.Vector(180000, y, z_at(180000,y) + 180)
        aim = unreal.Vector(180000, min(139000, y+1200), z_at(180000,min(139000,y+1200)) + 150)
        camera.set_actor_location(eye, False, True)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, aim), True)
        if warmup_seconds <= elapsed < warmup_seconds + sample_seconds and delta_seconds > 0:
            samples.append(delta_seconds * 1000)
        if elapsed < warmup_seconds + sample_seconds:
            return
        rss_bytes = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        result = {
            "map": MAP,
            "mode": "UE 5.5.4 editor SceneCapture2D proxy; NOT Development or packaged acceptance",
            "target_hardware": "Apple M4 Mac mini 16 GiB unified memory",
            "render_target": [1920, 1080], "scalability": "High (sg.*Quality=2)",
            "route_cm": [[180000,123500],[180000,138000]],
            "warmup_seconds": warmup_seconds, "capture_seconds": sample_seconds,
            "sample_count": len(samples),
            "p50_ms": percentile(samples,.50), "p95_ms": percentile(samples,.95),
            "p99_ms": percentile(samples,.99), "max_ms": max(samples),
            "mean_ms": statistics.mean(samples),
            "process_peak_rss_bytes": rss_bytes,
            "acceptance_gate_evaluated": False,
            "reason": "Editor capture and viewport overhead differ from a Development or packaged fixed route",
        }
        (OUT / "Performance_Probe.json").write_text(json.dumps(result, indent=2) + "\n")
        actor_api.destroy_actor(camera)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    except Exception:
        (OUT / "Performance_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
