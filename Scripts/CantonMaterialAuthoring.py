"""Deterministic checked material graphs; used only by district authoring."""
import unreal
DEST='/Game/Canton/DistrictPrototype/Completion'
M=unreal.MaterialEditingLibrary;E=unreal.EditorAssetLibrary

def physical(family):
 path=DEST+'/Materials/PM_Canton_'+family
 material=E.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('PM_Canton_'+family,DEST+'/Materials',unreal.PhysicalMaterial,unreal.PhysicalMaterialFactoryNew())
 material.set_editor_property('friction',.55 if family=='Damp' else .8)
 material.set_editor_property('restitution',0.)
 assert E.save_loaded_asset(material)
 return material

def node(mat,kind,**props):
 n=M.create_material_expression(mat,kind)
 for k,v in props.items():n.set_editor_property(k,v)
 return n

def connect(a,output,b,pin):assert M.connect_material_expressions(a,output,b,pin),(a,b,pin)
def prop(a,output,target):assert M.connect_material_property(a,output,target)
def world_uv(mat):
 wp=node(mat,unreal.MaterialExpressionWorldPosition)
 mask=node(mat,unreal.MaterialExpressionComponentMask,r=True,g=True)
 connect(wp,'',mask,'')
 # Project vertical support walls in (X+Y,Z), horizontal roads in (X,Y).
 # Avoid stretching one texel column down the retaining wall.
 x=node(mat,unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);connect(wp,'',x,'')
 y=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);connect(wp,'',y,'')
 z=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);connect(wp,'',z,'')
 horizontal=node(mat,unreal.MaterialExpressionAdd);connect(x,'',horizontal,'A');connect(y,'',horizontal,'B')
 side=node(mat,unreal.MaterialExpressionAppendVector);connect(horizontal,'',side,'A');connect(z,'',side,'B')
 normal=node(mat,unreal.MaterialExpressionVertexNormalWS)
 nz=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);connect(normal,'',nz,'')
 weight=node(mat,unreal.MaterialExpressionAbs);connect(nz,'',weight,'')
 projection=node(mat,unreal.MaterialExpressionLinearInterpolate);connect(side,'',projection,'A');connect(mask,'',projection,'B');connect(weight,'',projection,'Alpha')
 uv=node(mat,unreal.MaterialExpressionMultiply,const_b=.01);connect(projection,'',uv,'A');return uv

def sample(mat,family,suffix,uv):
 sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
 n=node(mat,unreal.MaterialExpressionTextureSample,texture=E.load_asset(DEST+'/Textures/'+family+'_'+suffix),sampler_type=sampler)
 connect(uv,'',n,'');return n

def rebuild():
 for family,color in [('Stone',None),('Earth',None),('Pebble',None),('Grass',(.08,.13,.035)),('Leaf',(.14,.065,.018)),('Damp',None)]:
  mat=E.load_asset(DEST+'/Materials/M_Canton_'+family);M.delete_all_material_expressions(mat)
  mat.set_editor_property('phys_material',physical(family))
  uv=world_uv(mat)
  if color is None:
   texture_family='Earth' if family=='Damp' else family
   base=sample(mat,texture_family,'BaseColor',uv);out='RGB'
   normal=sample(mat,texture_family,'Normal',uv);prop(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
  else:base=node(mat,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*color,1));out=''
  tint=node(mat,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.48,.48,.48,1) if family=='Damp' else unreal.LinearColor(1,1,1,1))
  mul=node(mat,unreal.MaterialExpressionMultiply);connect(base,out,mul,'A');connect(tint,'',mul,'B');prop(mul,'',unreal.MaterialProperty.MP_BASE_COLOR)
  rough=node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Roughness',default_value=.65 if family=='Damp' else .87);prop(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
  spec=node(mat,unreal.MaterialExpressionScalarParameter,parameter_name='Specular',default_value=.10);prop(spec,'',unreal.MaterialProperty.MP_SPECULAR)
  M.recompile_material(mat);assert E.save_loaded_asset(mat)
 land=E.load_asset('/Game/Canton/DistrictPrototype/Terrain/M_Canton_Landscape_PROVISIONAL');M.delete_all_material_expressions(land)
 land.set_editor_property('phys_material',physical('Earth'))
 uv=world_uv(land);earth=sample(land,'Earth','BaseColor',uv);previous=earth;out='RGB'
 for name,family,tint in [('Soil','Earth',(1,1,1)),('Earth','Earth',(1.2,1.15,1.1)),('Pebble','Pebble',(1,1,1)),('Grass','Earth',(.7,.95,.6)),('Damp','Earth',(.5,.5,.5)),('Stone','Stone',(1,1,1))]:
  tex=sample(land,family,'BaseColor',uv);factor=node(land,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*tint,1))
  mul=node(land,unreal.MaterialExpressionMultiply);connect(tex,'RGB',mul,'A');connect(factor,'',mul,'B')
  layer=node(land,unreal.MaterialExpressionLandscapeLayerWeight,parameter_name=name,preview_weight=0)
  connect(previous,out,layer,'Base');connect(mul,'',layer,'Layer');previous,out=layer,''
 prop(previous,out,unreal.MaterialProperty.MP_BASE_COLOR)
 normal=sample(land,'Earth','Normal',uv);prop(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
 rough=node(land,unreal.MaterialExpressionConstant,r=.9);prop(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 M.recompile_material(land);assert E.save_loaded_asset(land)
