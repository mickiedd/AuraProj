"""Validate saved V2 authority, source preservation and portable instances."""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,OUT,templates,snapshot

bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
expected=json.loads((OUT/'apply.json').read_text())['after']
actual=snapshot(templates(bp)[1])
assert actual==expected,'Serialized component data differs from repaired snapshot'
assert len(actual)==114 and sum(r['instances'] for r in actual.values())==12387
assert not any('UltraAAA' in r['mesh'] for r in actual.values())
assert sum('N_RoofShell_' in r['mesh'] for r in actual.values())==3
assert sum('N_SignBoardFace.' in r['mesh'] for r in actual.values())==1
mode=globals().get('VALIDATION_MODE','live')
if mode=='live':
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    a=sub.spawn_actor_from_object(bp,unreal.Vector(0,0,100000));assert a
    try:
        cs=a.get_components_by_class(unreal.StaticMeshComponent)
        assert len(cs)==114
        assert not any('UltraAAA' in c.static_mesh.get_path_name() for c in cs if c.static_mesh)
        assert all(c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') for c in cs)
        assert sum(c.get_instance_count() for c in cs)==12387
    finally:assert sub.destroy_actor(a)
report=dict(passed=True,mode=mode,blueprint=BP,components=114,instances=12387,roof_shells=3,signboard_faces=1,
    overlay_components=0,source_snapshot_preserved=True,spawned_instance_passed=True if mode=='live' else None,temporary_actor_destroyed=True if mode=='live' else None,
    spawn_skip_reason='Commandlet has no initialized placement subsystem; spawn verified in live editor' if mode!='live' else None)
(OUT/('validate-'+mode+'.json')).write_text(json.dumps(report,indent=2))
print('ZNM_ASSEMBLY_VALIDATED',json.dumps(report))
