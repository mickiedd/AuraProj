"""Validate live instance and newly spawned Blueprint mesh references."""
import json
from pathlib import Path
import unreal
O=Path('C:/Git/AuraProj/Saved/GateDoorFix');changes=json.loads((O/'asset-changes.json').read_text())
s=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp=unreal.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3')
placed=[a for a in s.get_all_level_actors() if a.get_class()==bp.generated_class()]
assert placed
fresh=s.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,0,-100000))
try:
 for a in placed+[fresh]:
  meshes=[c.static_mesh.get_path_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
  for change in changes:assert change['after'] in meshes and change['before'] not in meshes,(a.get_actor_label(),change)
  assert len(meshes)==16,len(meshes)
 report={'passed':True,'placed_instances':len(placed),'fresh_blueprint_instance':True,'components_per_instance':16,'aligned_mesh_references':2}
 (O/'editor-validation.json').write_text(json.dumps(report,indent=2));print(report)
finally:s.destroy_actor(fresh)
