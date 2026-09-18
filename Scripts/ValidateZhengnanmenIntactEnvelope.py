"""Verify saved closed variants and all original V2 component instance payloads."""
import json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
applied=json.loads((OUT/'apply.json').read_text());manifest=json.loads(Path(applied['manifest']).read_text())
before={r['name']:r for r in json.loads((OUT/'before-instances.json').read_text())}
bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
rows=templates(bp)[1];assert snapshot(rows)==applied['after']
checked=0;exports=[];(OUT/'Unchanged').mkdir(exist_ok=True)
changed={e['component']:e for e in manifest['entries']}
detail=json.loads((OUT/'detail-fallback-applied.json').read_text())['entries'] if (OUT/'detail-fallback-applied.json').exists() else []
details={e['component']:e for e in detail}
mode=globals().get('VALIDATION_MODE','live')
for h,c in rows:
    original=before[c.get_name()]['instances']
    assert c.get_instance_count()==len(original)
    for i,r in enumerate(original):
        t=c.get_instance_transform(i,False)
        for actual,expected in (([t.translation.x,t.translation.y,t.translation.z],r['location']),([t.scale3d.x,t.scale3d.y,t.scale3d.z],r['scale']),([t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],r['rotation'])):
            assert max(abs(a-b) for a,b in zip(actual,expected))<.000001,(c.get_name(),i)
        checked+=1
    if c.get_name() in changed:
        e=changed[c.get_name()];nanite=c.static_mesh.get_editor_property('nanite_settings')
        assert nanite.enabled and nanite.fallback_target==unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
        assert nanite.fallback_percent_triangles==1 and nanite.fallback_relative_error==0
        if mode=='live':assert c.static_mesh.get_num_triangles(0)==e['new_triangles']
    elif mode=='live':
        if c.get_name() in details:
            nanite=c.static_mesh.get_editor_property('nanite_settings')
            assert nanite.fallback_target==unreal.NaniteFallbackTarget.PERCENT_TRIANGLES and nanite.fallback_percent_triangles==1
            assert c.static_mesh.get_num_triangles(0)==details[c.get_name()]['new_triangles']
        t=unreal.AssetExportTask();t.object=c.static_mesh;t.filename=str(OUT/'Unchanged'/(c.get_name()+'.obj'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterOBJ()
        assert unreal.Exporter.run_asset_export_task(t);exports.append(t.filename)
assert checked==12383 and len(rows)==114 and len(changed)==101
assert not any('UltraAAA_HeroGeometry' in c.get_name() for h,c in rows)
report=dict(passed=True,mode=mode,components=114,mesh_variants=101,detail_fallback_copies=len(detail),instance_transforms_preserved=checked,full_fallback_geometry=True,unchanged_mesh_exports=exports,maps_saved=False)
(OUT/('validate-'+mode+'.json')).write_text(json.dumps(report,indent=2))
print('INTACT_ENVELOPE_VALIDATED',json.dumps({k:v for k,v in report.items() if k!='unchanged_mesh_exports'}))
