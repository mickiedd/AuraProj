"""Fresh-map SceneCapture views of the nine provisional city landmarks."""
import json
import struct
import time
import traceback
import zlib
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'QA/Canton_Continuation'
MAP = '/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL'
WORLD = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert WORLD and unreal.CantonTerrainLibrary.load_provisional_region(WORLD)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
STARTED = time.monotonic()
HANDLE = None


def save_png(path, pixels, width, height):
    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))
    scanlines = bytearray()
    for row in range(height):
        scanlines.append(0)
        for pixel in pixels[row * width:(row + 1) * width]:
            scanlines.extend((pixel.r, pixel.g, pixel.b))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def tick(dt):
    global HANDLE
    if time.monotonic() - STARTED < 25:
        return
    unreal.unregister_slate_post_tick_callback(HANDLE)
    try:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        assert sum('Canton.ProvisionalLandmarkPlacement' in [str(t) for t in a.tags]
                   for a in actors.get_all_level_actors()) == 9
        width, height = 1440, 1000
        target = unreal.RenderingLibrary.create_render_target2d(
            WORLD, width, height, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        views = [
            ('Walled_Landmarks_Citywide', (30000, -110000, 245000), (205000, 220000, 1700)),
            ('Walled_Landmarks_North', (150000, 175000, 118000), (210000, 340000, 3000)),
            ('Walled_Landmarks_South', (108000, -20000, 85000), (190000, 145000, 1100)),
        ]
        records = []
        for name, eye, aim in views:
            camera = actors.spawn_actor_from_class(
                unreal.SceneCapture2D, unreal.Vector(*eye),
                unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*aim)))
            assert camera
            capture = camera.capture_component2d
            capture.set_editor_property('texture_target', target)
            capture.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            capture.set_editor_property('fov_angle', 65.0)
            settings = capture.get_editor_property('post_process_settings')
            settings.set_editor_property('override_auto_exposure_method', True)
            settings.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property('override_auto_exposure_bias', True)
            settings.set_editor_property('auto_exposure_bias', 11.0)
            capture.set_editor_property('post_process_settings', settings)
            capture.capture_scene()
            pixels = unreal.RenderingLibrary.read_render_target(WORLD, target, True)
            path = OUT / (name + '.png')
            save_png(path, pixels, width, height)
            records.append({'view': name, 'camera_cm': eye, 'aim_cm': aim,
                            'image': str(path.relative_to(ROOT))})
            actors.destroy_actor(camera)
        (OUT / 'Walled_Landmark_Capture_Settings.json').write_text(
            json.dumps({'map': MAP, 'historically_accepted': False,
                        'views': records}, indent=2) + '\n')
        unreal.log('CANTON_WALLED_LANDMARK_CAPTURE ' + json.dumps({'views': len(records)}))
    except Exception:
        (OUT / 'Walled_Landmark_Capture_Error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
