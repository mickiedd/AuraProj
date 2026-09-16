"""Capture one placed V3 Blueprint at close range in the showcase level."""
from pathlib import Path
import unreal

LEVEL='/Game/Scifi_desert_city/Level/L_showcase_level'
ROOT=Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
TAG='ImportedGuangzhouLandmarkV3'
NAMES=['Xiaobeimen_AAA_V3','Xiaobeimen_Production_V3','Zhengnanmen_AAA_V3']

def capture(index):
    name=NAMES[index]
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    actor=next(a for a in actors if TAG in [str(t) for t in a.tags] and a.get_actor_label()=='GuangzhouLandmark_'+name)
    origin,extent=actor.get_actor_bounds(False)
    radius=max(extent.x,extent.y,extent.z)
    target=unreal.Vector(origin.x,origin.y,origin.z*.9)
    camera=target+unreal.Vector(radius*1.2,radius*2.15,radius*.85)
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,target))
    world=editor.get_editor_world()
    for command in ('r.TextureStreaming 0','r.ScreenPercentage 100','ShowFlag.Grid 0','ShowFlag.SelectionOutline 0'):
        unreal.SystemLibrary.execute_console_command(world,command)
    out=ROOT/('v3-level-placement-'+name+'.png')
    unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(out),None,False,False,
        unreal.ComparisonTolerance.LOW,'V3 placed Blueprint closeup '+name,2.0,True)
    print('V3_PLACEMENT_CLOSEUP_REQUESTED',out)

if __name__=='__main__': capture(0)
