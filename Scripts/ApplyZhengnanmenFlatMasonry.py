"""Bind flat outward stone and replace only the V2 wall stone instances."""
import hashlib,json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RawModelImport/ZhengnanmenFlatMasonry'
SOURCE=ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_FlatMasonry_20260916'
DEST=BP.rsplit('/',1)[0]+'/FlatMasonry_20260916'
MESH=DEST+'/SM_Zhengnanmen_FlatDressedStone'

def main():
    data=json.loads((SOURCE/'masonry-manifest.json').read_text())
    assert hashlib.sha256(Path(data['mesh_source']).read_bytes()).hexdigest()==data['sha256']
    assert not (OUT/'apply.json').exists(),'Repair already applied; preserve original rollback evidence'
    bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
    rows=templates(bp)[1];before=snapshot(rows)
    c=next(c for h,c in rows if c.get_name()==data['component'])
    assert c.static_mesh.get_path_name()==data['original_mesh'] and c.get_instance_count()==4154
    path=ROOT/'Content'/(BP[6:]+'.uasset')
    (OUT/'before-BP.uasset').write_bytes(path.read_bytes())
    (OUT/'before-components.json').write_text(json.dumps(before,indent=2))
    mat=c.get_material(0)
    mesh=unreal.EditorAssetLibrary.load_asset(MESH)
    if not mesh:
        task=unreal.AssetImportTask();task.filename=data['mesh_source'];task.destination_path=DEST
        task.destination_name='SM_Zhengnanmen_FlatDressedStone';task.automated=True;task.save=True;task.replace_existing=False
        ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False
        ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;ui.automated_import_should_detect_type=False
        ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.generate_lightmap_u_vs=False
        ui.static_mesh_import_data.auto_generate_collision=True;ui.static_mesh_import_data.build_nanite=True
        ui.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        task.options=ui
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        old=unreal.SystemLibrary.get_console_variable_bool_value('Interchange.FeatureFlags.Import.FBX')
        try:
            unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        finally:unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX '+('1' if old else '0'))
        mesh=unreal.EditorAssetLibrary.load_asset(MESH)
    assert mesh,MESH
    b=mesh.get_bounding_box()
    assert all(abs(getattr(b.min,a)+50)<.01 and abs(getattr(b.max,a)-50)<.01 for a in 'xyz'),str(b)
    assert mesh.get_num_triangles(0)==44
    ts=[unreal.Transform(location=unreal.Vector(*r['location']),rotation=unreal.Rotator(roll=-90),scale=unreal.Vector(*r['scale'])) for r in data['cells']]
    c.set_static_mesh(mesh);c.set_material(0,mat)
    c.clear_instances()
    c.add_instances(ts,False,False,False)
    assert c.get_instance_count()==len(ts)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    after=snapshot(templates(bp)[1])
    assert {k:v for k,v in before.items() if k!=c.get_name()}=={k:v for k,v in after.items() if k!=c.get_name()}
    for key in ('materials','collision','transform','visible','hidden'):assert before[c.get_name()][key]==after[c.get_name()][key]
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    report=dict(passed=True,blueprint=BP,manifest=str(SOURCE/'masonry-manifest.json'),new_mesh=MESH,component=data['component'],before=before,after=after,
        wall_instances=len(ts),removed_arch_intruding_instances=len(data['removed_cells']),total_instances=sum(r['instances'] for r in after.values()),component_count=len(after),maps_saved=False)
    (OUT/'apply.json').write_text(json.dumps(report,indent=2))
    print('ZNM_FLAT_MASONRY_APPLIED',len(ts),'arch intrusions removed',len(data['removed_cells']))

if __name__=='__main__':main()
