"""Rebuild GLB materials from external PBR maps; avoids invalid Interchange TextureObject nodes.
Only modifies material/texture assets in the three imported GLB landmark folders.
"""
import json,struct
from pathlib import Path
import unreal
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==Path('C:/Git/AuraProj').resolve()
ROOT=Path('C:/Works/Raw3DModels/Extracted');DEST='/Game/Assets/Environment/GuangzhouLandmarks'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
models={
 'GreatSouthGate':('Great_South_Gate_Textured.glb','Textures'),
 'GreatNorthGate':('GreatNorthGate_Guangzhou_MegaDetail_Textured.glb','../Textures'),
 'ZhenhaiTower':('Zhenhai_Tower_UE5_HighDetail_Textured_PBR.glb','PBR_Textures')}
for name,(filename,texdir) in models.items():
    source=next(ROOT.rglob(filename))
    with source.open('rb') as f:
        f.read(12);n,_=struct.unpack('<II',f.read(8));gltf=json.loads(f.read(n))
    folder=(source.parent/texdir).resolve()
    for desc in gltf['materials']:
        matname=desc['name'];mat=unreal.EditorAssetLibrary.load_asset(DEST+'/'+name+'/'+matname)
        assert isinstance(mat,unreal.Material),matname
        lib.delete_all_material_expressions(mat)
        mat.set_editor_property('used_with_nanite',True)
        if name=='GreatSouthGate':
            maps=[(folder/(matname+'_'+suffix+'.png'),prop,output,normal) for suffix,prop,output,normal in [
                ('Albedo',unreal.MaterialProperty.MP_BASE_COLOR,'RGB',False),('Normal',unreal.MaterialProperty.MP_NORMAL,'RGB',True),
                ('Roughness',unreal.MaterialProperty.MP_ROUGHNESS,'R',False),('Metallic',unreal.MaterialProperty.MP_METALLIC,'R',False)]]
        elif name=='GreatNorthGate':
            maps=[(folder/matname/('T_'+matname+'_'+suffix+'.png'),prop,output,normal) for suffix,prop,output,normal in [
                ('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR,'RGB',False),('NormalDX',unreal.MaterialProperty.MP_NORMAL,'RGB',True),
                ('ORM',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R',False),('ORM',unreal.MaterialProperty.MP_ROUGHNESS,'G',False),('ORM',unreal.MaterialProperty.MP_METALLIC,'B',False)]]
        elif matname=='Black_Sign':
            maps=[]
            pbr=desc.get('pbrMetallicRoughness',{});color=pbr.get('baseColorFactor',[0.02,0.02,0.02,1])
            node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,0)
            node.constant=unreal.LinearColor(*color)
            lib.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            maps=[(folder/(matname+'_'+suffix+'.png'),prop,output,normal) for suffix,prop,output,normal in [
                ('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR,'RGB',False),('Normal',unreal.MaterialProperty.MP_NORMAL,'RGB',True),
                ('MetallicRoughness',unreal.MaterialProperty.MP_ROUGHNESS,'G',False),('MetallicRoughness',unreal.MaterialProperty.MP_METALLIC,'B',False)]]
        nodes={}
        for row,(path,prop,output,normal) in enumerate(maps):
            if str(path) not in nodes:
                assert path.is_file(),path
                target=DEST+'/'+name+'/PBR_Textures/'+path.stem
                tex=unreal.EditorAssetLibrary.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else None
                if tex is None:
                    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=DEST+'/'+name+'/PBR_Textures'
                    task.automated=True;task.replace_existing=False;tools.import_asset_tasks([task]);tex=task.get_objects()[0]
                color=prop==unreal.MaterialProperty.MP_BASE_COLOR
                tex.set_editor_property('srgb',color)
                tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_DEFAULT if color else unreal.TextureCompressionSettings.TC_MASKS)
                if normal:tex.set_editor_property('flip_green_channel',name!='GreatNorthGate')
                node=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,row*180)
                node.texture=tex;node.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if color else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
                nodes[str(path)]=node
            assert lib.connect_material_property(nodes[str(path)],output,prop)
        lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_directory(DEST+'/'+name)
    print('PBR_REBUILT',name)
