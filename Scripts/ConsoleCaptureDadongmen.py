import json
from pathlib import Path
import unreal
root=Path("C:/Git/AuraProj/Saved/RawModelImport/V4"); report=json.loads((root/"Dadongmen_V4-import.json").read_text()); assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report["preview_level"])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); b=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=="Preview_Dadongmen_V4"); o,e=b.get_actor_bounds(False); r=max(e.x,e.y,e.z); t=unreal.Vector(o.x,o.y,o.z*.9); cam=t+unreal.Vector(r*2.0,r*2.4,r*1.0); ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); ed.set_level_viewport_camera_info(cam,unreal.MathLibrary.find_look_at_rotation(cam,t)); world=ed.get_editor_world(); unreal.SystemLibrary.execute_console_command(world,"HighResShot 1600x1000"); print("CAM",cam)
