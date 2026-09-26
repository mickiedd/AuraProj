"""Fresh-process map-load validation and native terrain captures; no map save."""
import json
import struct
import time
import traceback
import zlib
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'QA/UE_Import_Screenshots'
MAP='/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL'
world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world
assert unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started=time.monotonic()
handle=None

def png(path,pixels,w,h):
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    raw=bytearray()
    for y in range(h):
        raw.append(0)
        for pixel in pixels[y*w:(y+1)*w]:raw.extend((pixel.r,pixel.g,pixel.b))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))


def tick(dt):
    if time.monotonic()-started<40:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        result=json.loads(unreal.CantonTerrainLibrary.validate_provisional_terrain(world,str(ROOT/'Export/Provisional/Canton_Modern_Context_SouthFirst.r16')))
        (OUT/'Reload_Validation.json').write_text(json.dumps(result,indent=2)+'\n')
        assert result['passed'],result
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        w,h=1440,1000
        rt=unreal.RenderingLibrary.create_render_target2d(world,w,h,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        views=[('overview',(-70000,-160000,340000),(201600,201600,1000)),('north-hills',(100000,190000,100000),(200000,330000,2500)),('south-gate',(150000,85000,18000),(180730,133180,1500))]
        records=[]
        for name,eye,aim in views:
            camera=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(*eye),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye),unreal.Vector(*aim)))
            c=camera.capture_component2d
            c.set_editor_property('texture_target',rt)
            c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            c.set_editor_property('fov_angle',65)
            settings=c.get_editor_property('post_process_settings')
            settings.set_editor_property('override_auto_exposure_method',True)
            settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property('override_auto_exposure_bias',True)
            settings.set_editor_property('auto_exposure_bias',11)
            c.set_editor_property('post_process_settings',settings)
            c.capture_scene()
            pixels=unreal.RenderingLibrary.read_render_target(world,rt,True)
            path=OUT/(name+'.png')
            png(path,pixels,w,h)
            records.append({'view':name,'camera_cm':eye,'target_cm':aim,'path':str(path.relative_to(ROOT))})
            actors.destroy_actor(camera)
        (OUT/'Capture_Settings.json').write_text(json.dumps(records,indent=2)+'\n')
    except Exception:
        (OUT/'Review_Error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
