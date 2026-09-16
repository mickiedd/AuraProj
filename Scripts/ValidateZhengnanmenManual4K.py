"""Read the saved Blueprint/materials in live UE or a fresh NullRHI process."""
import hashlib
import json
from pathlib import Path
import unreal

PROJECT=Path(__file__).resolve().parents[1]
OUT=PROJECT/"Saved/RawModelImport/ZhengnanmenManual4K"
before=json.loads((OUT/"before.json").read_text());applied=json.loads((OUT/"apply.json").read_text())
source=json.loads(Path(applied['source_manifest']).read_text())
bp=unreal.EditorAssetLibrary.load_asset(applied['blueprint']);assert bp and bp.generated_class()
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
expected={r['name']:r for r in before['components']};seen=set();records=[]
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
    d=ss.k2_find_subobject_data_from_handle(h)
    if not lib.is_component(d):continue
    c=lib.get_object(d)
    if not isinstance(c,unreal.StaticMeshComponent) or not c.static_mesh or c.get_path_name() in seen:continue
    seen.add(c.get_path_name());r=expected[c.get_name()]
    assert c.static_mesh.get_path_name()==r['mesh'],c.get_name()
    instances=c.get_instance_count() if isinstance(c,unreal.InstancedStaticMeshComponent) else 1
    assert instances==r['instances'],c.get_name()
    transform={key:[float(getattr(c.get_editor_property(key),axis)) for axis in axes] for key,axes in (("relative_location","xyz"),("relative_rotation",("pitch","yaw","roll")),("relative_scale3d","xyz"))}
    assert transform==r['transform'],c.get_name()
    assert str(c.get_collision_enabled())==r['collision'],c.get_name()
    mesh_path=r['mesh'].split('.')[0]
    path=PROJECT/'Content'/(mesh_path[6:]+'.uasset') if mesh_path.startswith('/Game/') else Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_content_dir()))/(mesh_path[8:]+'.uasset')
    assert hashlib.sha256(path.read_bytes()).hexdigest()==r['mesh_sha256'],path
    mats=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]
    assert all(m in applied['materials'].values() for m in mats),(c.get_name(),mats)
    # Verify each material role, not merely membership in the new material set.
    for old,new in zip(r['materials'],mats):
        families={'M_Stone_Aged':'Stone','M_GlazedTile_Green':'Glaze','M_Wood_RedLacquer':'RedTimber','M_Wood_DarkAged':'DarkTimber','M_Gold_RidgeOrnament':'BronzeGold','M_DarkInterior':'Interior','M_Signboard_Zhengnanmen':'Plaque'}
        kind=next(v for k,v in families.items() if old.rsplit('/',1)[-1].startswith(k+'_'))
        assert new==applied['materials'][kind],(old,new)
    records.append({'name':c.get_name(),'instances':instances,'materials':mats})
assert len(records)==len(expected)==115
assert sum(r['instances'] for r in records)==12388
texture_records={}
mode=globals().get('VALIDATION_MODE','live')
for kind,mpath in applied['materials'].items():
    m=unreal.EditorAssetLibrary.load_asset(mpath);assert m
    assert m.get_editor_property('used_with_instanced_static_meshes')
    assert not m.get_editor_property('two_sided')
    # Plaque deliberately has no PerInstanceRandom: 7 expressions. Tiling
    # surfaces add random/lerp/two scalar/multiply nodes for exactly 12.
    assert unreal.MaterialEditingLibrary.get_num_material_expressions(m)==(7 if kind=='Plaque' else 12),mpath
    # NullRHI has no compiled shader resource, so get_used_textures is empty.
    # Walk the actual serialized property input graph in *both* environments.
    graph_tex=set();visited=set();roots={}
    for p in (unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_NORMAL,unreal.MaterialProperty.MP_ROUGHNESS,unreal.MaterialProperty.MP_METALLIC,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION):
        node=unreal.MaterialEditingLibrary.get_material_property_input_node(m,p);assert node,(kind,p)
        roots[str(p)]=node
        stack=[node]
        while stack:
            n=stack.pop()
            if not n or n.get_path_name() in visited:continue
            visited.add(n.get_path_name())
            if isinstance(n,unreal.MaterialExpressionTextureSample):graph_tex.add(n.get_editor_property('texture').get_path_name().split('.')[0])
            stack.extend(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(m,n))
    assert graph_tex==set(applied['textures'][kind].values()),(kind,graph_tex)
    for p,key,channel in ((unreal.MaterialProperty.MP_NORMAL,'Normal_DX','RGB'),(unreal.MaterialProperty.MP_ROUGHNESS,'ORM','G'),(unreal.MaterialProperty.MP_METALLIC,'ORM','B'),(unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'ORM','R')):
        node=roots[str(p)]
        assert isinstance(node,unreal.MaterialExpressionTextureSample),(kind,p)
        assert node.get_editor_property('texture').get_path_name().split('.')[0]==applied['textures'][kind][key],(kind,p)
        if channel!='RGB':assert unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(m,p)==channel,(kind,p,channel)
    if mode=='live':
        used={t.get_path_name().split('.')[0] for t in unreal.MaterialEditingLibrary.get_used_textures(m)}
        assert used==graph_tex,(kind,used)
    texture_records[kind]={}
    for key,path in applied['textures'][kind].items():
        t=unreal.EditorAssetLibrary.load_asset(path);r=source['materials'][kind]['maps'][key]
        assert (t.blueprint_get_size_x(),t.blueprint_get_size_y())==(r['width'],r['height']),path
        assert t.get_editor_property('srgb')==(key=='BaseColor'),path
        assert t.get_editor_property('max_texture_size')==4096 and t.get_editor_property('lod_bias')==0,path
        expected_comp={'BaseColor':unreal.TextureCompressionSettings.TC_DEFAULT,'Normal_DX':unreal.TextureCompressionSettings.TC_NORMALMAP,'ORM':unreal.TextureCompressionSettings.TC_MASKS,'Height':unreal.TextureCompressionSettings.TC_HALF_FLOAT}[key]
        assert t.get_editor_property('compression_settings')==expected_comp,path
        texture_records[kind][key]={'path':path,'size':[r['width'],r['height']],'srgb':t.get_editor_property('srgb'),'compression':str(expected_comp)}
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# NullRHI PythonScript commandlet has no showcase world. Name output explicitly
# via a small launcher rather than inferring validation provenance from claims.
mode=globals().get('VALIDATION_MODE','live')
result={'passed':True,'mode':mode,'blueprint':applied['blueprint'],'components':115,'instances':12388,'material_count':7,'imported_texture_count':28,'shared_mesh_bytes_unchanged':True,'component_transforms_unchanged':True,'collision_unchanged':True,'world':world.get_path_name() if world else None,'maps_saved':False,'textures':texture_records,'component_records':records}
(OUT/(mode+'-validation.json')).write_text(json.dumps(result,indent=2))
print('ZNM_MANUAL4K_VALIDATION_PASSED',json.dumps({k:v for k,v in result.items() if k not in ('textures','component_records')}))
