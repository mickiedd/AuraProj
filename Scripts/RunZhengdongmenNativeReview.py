"""Dedicated temporary GUI process: render repaired Blueprint, then exit unsaved."""
import runpy
import time
from pathlib import Path
import unreal
P=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
# Load the Blueprint/materials before the tick delay so shaders and textures
# compile before the first capture (an immediate spawn renders checkerboards).
preloaded=unreal.EditorAssetLibrary.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/BP_Zhengdongmen')
master=unreal.EditorAssetLibrary.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/Materials/M_ZDM_Master')
unreal.MaterialEditingLibrary.recompile_material(master)
for group in ('Stone','Wood','RoofTile','Plaster','Iron','DoorWood','Sign'):
    for suffix in ('BaseColor','Normal','Roughness','Metallic','AO'):
        tex=unreal.EditorAssetLibrary.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/Textures/T_ZDM_'+group+'_'+suffix)
        tex.set_force_mip_levels_to_be_resident(120.0)
start=time.monotonic()
handle=None

def tick(dt):
    global handle
    if time.monotonic()-start<25:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        runpy.run_path(str(P/'Scripts/CaptureZhengdongmenReference.py'),run_name='__main__')
    except Exception:
        import traceback
        (P/'Saved/Reports/Zhengdongmen/native-error.txt').write_text(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        # Exit the review process. The previous run left its editor open, which then
        # blocked the next isolated reimport for an hour, so quitting is part of the
        # job rather than something the operator has to remember.
        unreal.SystemLibrary.quit_editor()

handle=unreal.register_slate_post_tick_callback(tick)
