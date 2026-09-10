"""Import the prepared Xiaobeimen OBJ and explicitly wire its supplied PBR maps.
Run PrepareXiaobeimenObj.py first, then execute this file inside Unreal Editor.
"""
import json
from pathlib import Path
import unreal
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"
ROOT=Path('C:/Works/Raw3DModels/Extracted/Xiaobeimen_SmallNorthGate_Unreal_v3_Textured/Xiaobeimen_SmallNorthGate_Unreal_v3_Textured')
DEST='/Game/Assets/Environment/GuangzhouLandmarks/Xiaobeimen'
assert not unreal.EditorAssetLibrary.does_directory_exist(DEST), 'Refusing to overwrite existing content'
tools=unreal.AssetToolsHelpers.get_asset_tools()
task=unreal.AssetImportTask()
task.filename=str(ROOT/'SM_Xiaobeimen_Combined.obj')
task.destination_path=DEST
task.destination_name='SM_Xiaobeimen'
task.automated=True;task.save=True;task.replace_existing=False
opts=unreal.FbxImportUI()
opts.import_mesh=True;opts.import_materials=False;opts.import_textures=False
opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
opts.automated_import_should_detect_type=False
opts.static_mesh_import_data.combine_meshes=True
opts.static_mesh_import_data.auto_generate_collision=False
opts.static_mesh_import_data.generate_lightmap_u_vs=False
opts.static_mesh_import_data.import_uniform_scale=1.0
opts.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
opts.static_mesh_import_data.build_nanite=True
task.options=opts
tools.import_asset_tasks([task])
meshes=[o for o in task.get_objects() if isinstance(o,unreal.StaticMesh)]
assert len(meshes)==1
mesh=meshes[0]
mapping=json.loads((ROOT/'model_stats.json').read_text())['texture_sets']
textures={}
for p in sorted((ROOT/'Textures').glob('*.png')):
    t=unreal.AssetImportTask();t.filename=str(p);t.destination_path=DEST+'/Textures'
    t.automated=True;t.save=False;t.replace_existing=False
    tools.import_asset_tasks([t])
    tex=t.get_objects()[0]
    normal=p.stem.endswith('_Normal');color=p.stem.endswith('_BaseColor')
    tex.set_editor_property('srgb',color)
    if normal:tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif not color:tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    textures[p.stem]=tex
materials={}
for index,slot in enumerate(mesh.static_materials):
    name=str(slot.material_slot_name)
    assert name in mapping, name
    prefix=mapping[name]
    if prefix not in materials:
        mat=tools.create_asset('M_'+prefix[2:],DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        mat.set_editor_property('used_with_nanite',True)
        for row,(suffix,prop) in enumerate([
            ('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR),('Normal',unreal.MaterialProperty.MP_NORMAL),
            ('Roughness',unreal.MaterialProperty.MP_ROUGHNESS),('AO',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]):
            node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,row*220)
            node.texture=textures[prefix+'_'+suffix]
            node.sampler_type=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix=='Normal' else
                               unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if suffix=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            assert unreal.MaterialEditingLibrary.connect_material_property(node,'RGB' if suffix in ('BaseColor','Normal') else 'R',prop)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        materials[prefix]=mat
    mesh.set_material(index,materials[prefix])
assert unreal.EditorAssetLibrary.save_directory(DEST)
b=mesh.get_bounds()
report={'mesh':mesh.get_path_name(),'size_cm':[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2],
        'materials':[str(s.material_slot_name) for s in mesh.static_materials], 'textures':len(textures),
        'nanite':mesh.get_editor_property('nanite_settings').enabled}
Path('C:/Git/AuraProj/Saved/RawModelImport/Xiaobeimen.json').write_text(json.dumps(report,indent=2))
print('IMPORT_VALIDATED',report)
