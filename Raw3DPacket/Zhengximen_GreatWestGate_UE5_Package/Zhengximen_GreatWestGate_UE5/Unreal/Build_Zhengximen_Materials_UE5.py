# Build master material + eight material instances and assign to Zhengximen mesh.
import unreal
BASE='/Game/Zhengximen'; T=BASE+'/Textures'; M=BASE+'/Materials'; SM=BASE+'/Meshes'
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()

def load(path): return unreal.EditorAssetLibrary.load_asset(path)

def make_master():
    path=M+'/M_Zhengximen_Master'
    old=load(path)
    if old: return old
    mat=asset_tools.create_asset('M_Zhengximen_Master',M,unreal.Material,unreal.MaterialFactoryNew())
    mel=unreal.MaterialEditingLibrary
    params=[
      ('BaseColorTex',unreal.MaterialProperty.MP_BASE_COLOR,'RGB',-600,-240,None),
      ('NormalTex',unreal.MaterialProperty.MP_NORMAL,'RGB',-600,-80,'normal'),
      ('RoughnessTex',unreal.MaterialProperty.MP_ROUGHNESS,'R',-600,80,None),
      ('MetallicTex',unreal.MaterialProperty.MP_METALLIC,'R',-600,240,None),
      ('AOTex',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R',-600,400,None)]
    for pn,prop,out,x,y,kind in params:
        e=mel.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,x,y)
        e.set_editor_property('parameter_name',pn)
        if kind=='normal':
            try: e.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            except: pass
        mel.connect_material_property(e,out,prop)
    mel.recompile_material(mat); unreal.EditorAssetLibrary.save_loaded_asset(mat); return mat

master=make_master(); mel=unreal.MaterialEditingLibrary
names=['GrayBrick','StoneFoundation','AgedWood','DarkTimber','ClayRoofTile','LimePlaster','BlackIron','GatePlaque']
instances={}
for n in names:
    ip=M+'/MI_'+n; mi=load(ip)
    if not mi:
        mi=asset_tools.create_asset('MI_'+n,M,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi,master)
    for par,suf in [('BaseColorTex','BaseColor'),('NormalTex','Normal'),('RoughnessTex','Roughness'),('MetallicTex','Metallic'),('AOTex','AO')]:
        tex=load(T+'/T_'+n+'_'+suf)
        if tex: mel.set_material_instance_texture_parameter_value(mi,par,tex)
    unreal.EditorAssetLibrary.save_loaded_asset(mi); instances[n]=mi
# Try to assign by material slot name to any Zhengximen static mesh under /Meshes.
for asset_path in unreal.EditorAssetLibrary.list_assets(SM,recursive=True,include_folder=False):
    sm=load(asset_path)
    if not isinstance(sm,unreal.StaticMesh): continue
    try:
        slots=sm.get_editor_property('static_materials')
        for i,s in enumerate(slots):
            slot=str(s.material_slot_name)
            key=slot.replace('M_','').replace('MI_','')
            if key in instances: sm.set_material(i,instances[key])
        unreal.EditorAssetLibrary.save_loaded_asset(sm)
    except Exception as e: unreal.log_warning('Material assignment: '+str(e))
unreal.log('Zhengximen materials built. Verify texture color-space settings and slot assignment in Static Mesh Editor.')
