"""Apply source-facing compatibility settings to the two affected V4 packages."""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[1]/'Saved/RawModelImport/V4'
def main():
 changed=[]
 for name in ('Guidemen','Zhengximen'):
  for p in unreal.EditorAssetLibrary.list_assets('/Game/Assets/Environment/GuangzhouLandmarks/V4/'+name,True,False):
   obj=unreal.load_asset(p)
   if isinstance(obj,unreal.Material):
    obj.set_editor_property('two_sided',True)
    unreal.MaterialEditingLibrary.recompile_material(obj)
   elif isinstance(obj,unreal.StaticMesh):
    obj.get_editor_property('body_setup').set_editor_property('double_sided_geometry',True)
   else:continue
   assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
   changed.append(p)
 (ROOT/'facing-compatibility.json').write_text(json.dumps({'render_and_collision_two_sided':changed},indent=2))
if __name__=='__main__':main()
