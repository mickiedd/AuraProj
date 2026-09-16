"""Capture the three placed V3 Blueprints together in the Scifi Desert level."""
import json
from pathlib import Path
import unreal

LEVEL_PATH='/Game/Scifi_desert_city/Level/L_showcase_level'
ROOT=Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
TAG='ImportedGuangzhouLandmarkV3'
OUT=ROOT/'v3-level-placement-overview.png'

def main():
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    tagged=[a for a in actors if TAG in [str(t) for t in a.tags]]
    assert len(tagged)==3, len(tagged)
    boxes=[]
    for actor in tagged:
        origin,extent=actor.get_actor_bounds(False)
        boxes.append((origin,extent))
    minx=min(o.x-e.x for o,e in boxes); maxx=max(o.x+e.x for o,e in boxes)
    miny=min(o.y-e.y for o,e in boxes); maxy=max(o.y+e.y for o,e in boxes)
    minz=min(o.z-e.z for o,e in boxes); maxz=max(o.z+e.z for o,e in boxes)
    center=unreal.Vector((minx+maxx)/2.0,(miny+maxy)/2.0,(minz+maxz)/2.0)
    radius=max(maxx-minx,maxy-miny,maxz-minz)
    camera=center+unreal.Vector(radius*0.95,radius*1.05,radius*0.72)
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,center))
    world=editor.get_editor_world()
    for command in ('r.TextureStreaming 0','r.ScreenPercentage 100','ShowFlag.Grid 0','ShowFlag.SelectionOutline 0'):
        unreal.SystemLibrary.execute_console_command(world,command)
    unreal.AutomationLibrary.take_high_res_screenshot(1800,1000,str(OUT),None,False,False,
        unreal.ComparisonTolerance.LOW,'V3 Scifi Desert placement overview',2.0,True)
    print('V3_PLACEMENT_CAPTURE_REQUESTED',OUT)

if __name__=='__main__': main()
