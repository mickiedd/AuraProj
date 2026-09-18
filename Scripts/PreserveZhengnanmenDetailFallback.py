"""Keep column/rail caps closed in fallback rendering using private mesh copies.

Two stages: export copies; validate topology on the host; then rebind only the
two HISM prototypes. Shared original meshes and all instance data stay intact.
"""
import hashlib,json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
DEST=BP.rsplit('/',1)[0]+'/IntactEnvelope_20260916'
applied=json.loads((OUT/'apply.json').read_text())
assert not (OUT/'detail-fallback-applied.json').exists(),'Detail repair already applied'
bp=unreal.EditorAssetLibrary.load_asset(BP);rows=templates(bp)[1]
assert snapshot(rows)==applied['after']
selected={'HierarchicalInstancedStaticMesh110_GEN_VARIABLE','HierarchicalInstancedStaticMesh3_GEN_VARIABLE'}
entries=[];copies={}
for h,c in rows:
    if c.get_name() not in selected:continue
    original=c.static_mesh;name='SM_IntactFallback_'+original.get_name().replace('-','Neg')
    asset=DEST+'/'+name
    mesh=unreal.EditorAssetLibrary.load_asset(asset) if unreal.EditorAssetLibrary.does_asset_exist(asset) else unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(),asset)
    assert mesh
    nanite=mesh.get_editor_property('nanite_settings')
    nanite.fallback_target=unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
    nanite.fallback_percent_triangles=1.;nanite.fallback_relative_error=0.
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh,nanite,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
    path=OUT/'Unchanged'/(c.get_name()+'.obj')
    t=unreal.AssetExportTask();t.object=mesh;t.filename=str(path);t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterOBJ()
    assert unreal.Exporter.run_asset_export_task(t)
    entries.append(dict(component=c.get_name(),original_mesh=original.get_path_name(),mesh=mesh.get_path_name(),export=str(path),new_triangles=mesh.get_num_triangles(0),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    copies[c.get_name()]=mesh
assert len(entries)==2
(OUT/'detail-fallback-imported.json').write_text(json.dumps(dict(entries=entries),indent=2))
verified=OUT/'validate-complete-topology.json'
if not verified.exists() or not json.loads(verified.read_text())['passed']:
    print('DETAIL_FALLBACK_READY_FOR_TOPOLOGY_CHECK');quit()
topology=json.loads(verified.read_text());hashes={r['path']:r['sha256'] for r in topology['unchanged']}
for e in entries:assert hashes[e['export']]==e['sha256']
before=snapshot(rows)
for h,c in rows:
    if c.get_name() in copies:
        mats=[c.get_material(i) for i in range(c.get_num_materials())]
        c.set_static_mesh(copies[c.get_name()])
        for i,m in enumerate(mats):c.set_material(i,m)
after=snapshot(templates(bp)[1])
for name,r in before.items():
    for k in ('instances','materials','collision','transform','visible','hidden'):assert r[k]==after[name][k],(name,k)
    if name not in selected:assert r==after[name]
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
(OUT/'detail-fallback-applied.json').write_text(json.dumps(dict(passed=True,entries=entries,before=before,after=after,original_meshes_unchanged=True,maps_saved=False),indent=2))
applied['primary_envelope_after']=applied['after'];applied['after']=after
applied['changed_mesh_components']=103;applied['detail_fallback_report']=str(OUT/'detail-fallback-applied.json')
(OUT/'apply.json').write_text(json.dumps(applied,indent=2))
print('DETAIL_FALLBACK_APPLIED',2)
