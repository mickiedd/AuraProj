"""Capture exterior views of every provisional gate/wall joint before or after fitting."""
import json
import math
import os
import struct
import time
import traceback
import zlib
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'QA/Canton_Continuation/Wall_Gate_Fit'
OUT.mkdir(parents=True, exist_ok=True)
PHASE = os.environ.get('CANTON_WALL_GATE_CAPTURE_PHASE', 'Before')
assert PHASE in ('Before', 'After')
DATA = json.loads((ROOT / 'Data/Canton_WalledCity_Provisional_Landmark_Placements.json').read_text())
WORLD = unreal.EditorLoadingAndSavingUtils.load_map(DATA['target_map'])
assert WORLD and unreal.CantonTerrainLibrary.load_provisional_region(WORLD)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
START = time.monotonic()
HANDLE = None


def save_png(path, pixels, width, height):
    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))
    scanlines = bytearray()
    for row in range(height):
        scanlines.append(0)
        for p in pixels[row * width:(row + 1) * width]:
            scanlines.extend((p.r, p.g, p.b))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def tick(dt):
    global HANDLE
    if time.monotonic() - START < 25:
        return
    unreal.unregister_slate_post_tick_callback(HANDLE)
    try:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        by_label = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
        width, height = 1280, 800
        target = unreal.RenderingLibrary.create_render_target2d(
            WORLD, width, height, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        records = []
        for row in DATA['landmarks']:
            if row['kind'] != 'gate':
                continue
            gate = by_label['PROVISIONAL_Landmark_' + row['key']]
            p = gate.get_actor_location()
            yaw = math.radians(row['label_yaw_deg'])
            outward = (-math.sin(yaw), math.cos(yaw))
            eye = unreal.Vector(p.x + 7000 * outward[0], p.y + 7000 * outward[1],
                                p.z + 1900)
            aim = unreal.Vector(p.x, p.y, p.z + 750)
            camera = actors.spawn_actor_from_class(
                unreal.SceneCapture2D, eye,
                unreal.MathLibrary.find_look_at_rotation(eye, aim))
            assert camera
            capture = camera.capture_component2d
            capture.set_editor_property('texture_target', target)
            capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            capture.set_editor_property('fov_angle', 80.0)
            settings = capture.get_editor_property('post_process_settings')
            settings.set_editor_property('override_auto_exposure_method', True)
            settings.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property('override_auto_exposure_bias', True)
            settings.set_editor_property('auto_exposure_bias', 11.0)
            capture.set_editor_property('post_process_settings', settings)
            capture.capture_scene()
            pixels = unreal.RenderingLibrary.read_render_target(WORLD, target, True)
            path = OUT / (PHASE + '_' + row['key'] + '.png')
            save_png(path, pixels, width, height)
            records.append({'key': row['key'], 'image': str(path.relative_to(ROOT)),
                            'eye_cm': [eye.x, eye.y, eye.z],
                            'aim_cm': [aim.x, aim.y, aim.z]})
            actors.destroy_actor(camera)
        assert len(records) == 8
        (OUT / (PHASE + '_Capture.json')).write_text(json.dumps({
            'phase': PHASE, 'historically_accepted': False,
            'views': records}, indent=2) + '\n')
        unreal.log('CANTON_WALL_GATE_CAPTURE ' + json.dumps({'phase': PHASE,
                                                               'views': len(records)}))
    except Exception:
        (OUT / (PHASE + '_Capture_Error.txt')).write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
