"""Dedicated temporary GUI process: render BP_Xiaobeimen_AAA_V3, then exit unsaved.

The capture needs a live RHI, which a Python commandlet does not have. Launch with
`-unattended` or the editor stalls at plugin mounting.
"""
import runpy
import time
from pathlib import Path
import unreal

P = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
# Load the Blueprint and its materials before the tick delay so shaders and textures
# compile before the first capture (an immediate spawn renders checkerboards).
ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
preloaded = unreal.EditorAssetLibrary.load_asset(ROOT + '/BP_Xiaobeimen_AAA_V3')
for material in ('M_StoneWall', 'M_Plaque', 'M_AgedWood', 'M_Metal'):
    asset = unreal.EditorAssetLibrary.load_asset(ROOT + '/Materials/' + material)
    if asset:
        unreal.MaterialEditingLibrary.recompile_material(asset)
# Texture assets are named <Folder>_<Kind>_4K inside a folder of the same name.
for folder in ('StoneWall', 'Plaque', 'AgedWood', 'MetalFittings'):
    for suffix in ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO', 'Height'):
        tex = unreal.EditorAssetLibrary.load_asset(
            '{}/Textures/{}/{}_{}_4K'.format(ROOT, folder, folder, suffix))
        if tex:
            tex.set_force_mip_levels_to_be_resident(120.0)
start = time.monotonic()
handle = None


def tick(dt):
    global handle
    if time.monotonic() - start < 25:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        runpy.run_path(str(P / 'Scripts/CaptureXiaobeimenAAA.py'), run_name='__main__')
    except Exception:
        import traceback
        (P / 'Saved/Reports/Xiaobeimen/native-error.txt').write_text(
            traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        # Exit the review process; a leftover editor holds the project and blocks the
        # next isolated reimport commandlet.
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
