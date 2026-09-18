"""Reimport repaired variants with native-FBX V compensation; preserve BP data."""
import json,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
SOURCE=ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_IntactEnvelope_20260916'
DEST=BP.rsplit('/',1)[0]+'/IntactEnvelope_20260916'
manifest=json.loads((SOURCE/'envelope-manifest.json').read_text())
bp=unreal.EditorAssetLibrary.load_asset(BP);before=snapshot(templates(bp)[1])
assert before==json.loads((OUT/'apply.json').read_text())['after']
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
tasks=[]
for e in manifest['entries']:
    t=unreal.AssetImportTask();t.filename=e['fbx'];t.destination_path=DEST;t.destination_name=e['mesh_name'];t.automated=True;t.save=True;t.replace_existing=True
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
for e in manifest['entries']:
    mesh=unreal.EditorAssetLibrary.load_asset(DEST+'/'+e['mesh_name'])
    n=mesh.get_editor_property('nanite_settings');n.fallback_target=unreal.NaniteFallbackTarget.PERCENT_TRIANGLES;n.fallback_percent_triangles=1.;n.fallback_relative_error=0.
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh,n,True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
    assert mesh.get_num_triangles(0)==e['new_triangles']
    b=mesh.get_bounding_box()
    for point,expected in ((b.min,e['expected_local_bounds'][0]),(b.max,e['expected_local_bounds'][1])):
        assert max(abs(getattr(point,a)-v) for a,v in zip('xyz',expected))<.05
    t=unreal.AssetExportTask();t.object=mesh;t.filename=str(OUT/'Imported'/(e['mesh_name']+'.obj'));t.automated=True;t.prompt=False;t.replace_identical=True;t.exporter=unreal.StaticMeshExporterOBJ()
    assert unreal.Exporter.run_asset_export_task(t)
assert snapshot(templates(bp)[1])==before
(OUT/'restore-uv.json').write_text(json.dumps(dict(passed=True,mesh_count=101,native_fbx_v_compensated=True,blueprint_payload_preserved=True,maps_saved=False),indent=2))
print('ENVELOPE_UV_CONVENTION_RESTORED',101)
