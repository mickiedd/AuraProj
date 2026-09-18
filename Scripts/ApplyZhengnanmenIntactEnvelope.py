"""Import verified closed variants, then rebind only the V2 ActorAsset.

Never edit shared source meshes/materials or load/save the user's maps. Imports
are resumable; Blueprint rebinding waits until every source variant is ready.
"""
import hashlib,json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
SOURCE=ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_IntactEnvelope_20260916'
DEST=BP.rsplit('/',1)[0]+'/IntactEnvelope_20260916'

def main():
    data=json.loads((SOURCE/'envelope-manifest.json').read_text())
    assert not (OUT/'apply.json').exists(),'Already applied; preserve immutable baseline evidence'
    bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
    assert snapshot(templates(bp)[1])==json.loads((OUT/'before-components.json').read_text())
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    tasks=[]
    for e in data['entries']:
        assert hashlib.sha256(Path(e['fbx']).read_bytes()).hexdigest()==e['sha256']
        path=DEST+'/'+e['mesh_name']
        if unreal.EditorAssetLibrary.does_asset_exist(path):continue
        t=unreal.AssetImportTask();t.filename=e['fbx'];t.destination_path=DEST;t.destination_name=e['mesh_name'];t.automated=True;t.save=True;t.replace_existing=False
        ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False
        ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;ui.automated_import_should_detect_type=False
        ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.generate_lightmap_u_vs=False
        ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.build_nanite=True
        ui.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        t.options=ui;tasks.append(t)
    old=unreal.SystemLibrary.get_console_variable_bool_value('Interchange.FeatureFlags.Import.FBX')
    try:
        unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    finally:unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX '+('1' if old else '0'))
    meshes={};exported=[];(OUT/'Imported').mkdir(exist_ok=True)
    for e in data['entries']:
        mesh=unreal.EditorAssetLibrary.load_asset(DEST+'/'+e['mesh_name']);assert mesh,e['mesh_name']
        # Perspective SceneCapture/ray paths can use Nanite's fallback. Keep
        # complete closure there too, rather than auto-simplifying thin edges.
        nanite=mesh.get_editor_property('nanite_settings')
        nanite.set_editor_property('fallback_target',unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        nanite.set_editor_property('fallback_percent_triangles',1.)
        nanite.set_editor_property('fallback_relative_error',0.)
        mesh.set_editor_property('nanite_settings',nanite)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
        b=mesh.get_bounding_box()
        for point,expected in ((b.min,e['expected_local_bounds'][0]),(b.max,e['expected_local_bounds'][1])):
            assert max(abs(getattr(point,a)-v) for a,v in zip('xyz',expected))<.05,(e['mesh_name'],str(b))
        assert mesh.get_num_triangles(0)==e['new_triangles'],(e['mesh_name'],mesh.get_num_triangles(0),e['new_triangles'])
        meshes[e['component']]=mesh
        task=unreal.AssetExportTask();task.object=mesh;task.filename=str(OUT/'Imported'/(e['mesh_name']+'.obj'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.StaticMeshExporterOBJ()
        assert unreal.Exporter.run_asset_export_task(task)
        exported.append(task.filename)
    (OUT/'imported.json').write_text(json.dumps(dict(passed=True,mesh_count=len(meshes),exports=exported,manifest=str(SOURCE/'envelope-manifest.json')),indent=2))
    # Geometry topology of exported UE meshes is checked by a separate host
    # script before this operation is allowed to touch the Blueprint.
    if not (OUT/'validate-imported.json').exists():
        print('INTACT_IMPORTS_READY_FOR_TOPOLOGY_CHECK',len(meshes));return
    verified=json.loads((OUT/'validate-imported.json').read_text());assert verified['passed'] and verified['mesh_count']==101
    for e in data['entries']:
        path=OUT/'Imported'/(e['mesh_name']+'.obj')
        assert hashlib.sha256(path.read_bytes()).hexdigest()==verified['hashes'][str(path)]
    rows=templates(bp)[1];before=snapshot(rows)
    for h,c in rows:
        if c.get_name() not in meshes:continue
        mats=[c.get_material(i) for i in range(c.get_num_materials())]
        c.set_static_mesh(meshes[c.get_name()])
        for i,mat in enumerate(mats):c.set_material(i,mat)
    after=snapshot(templates(bp)[1])
    assert len(after)==len(before)==114
    for name,r in before.items():
        for key in ('instances','materials','collision','transform','visible','hidden'):assert r[key]==after[name][key],(name,key)
        if name not in meshes:assert r==after[name]
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    report=dict(passed=True,blueprint=BP,manifest=str(SOURCE/'envelope-manifest.json'),changed_mesh_components=len(meshes),before=before,after=after,total_instances=sum(r['instances'] for r in after.values()),component_count=114,maps_saved=False,original_materials_instance_payloads_preserved=True)
    (OUT/'apply.json').write_text(json.dumps(report,indent=2))
    print('INTACT_ENVELOPE_APPLIED',len(meshes),report['total_instances'])

if __name__=='__main__':main()
