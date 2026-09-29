"""Idempotent engineering continuation on the existing district only.
Preserves the locked historical base and shared gate assets. Source-derived gate
collision uses the local interpretive package, not surveyed historical geometry.
"""
import unreal,json,math,time,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'QA/Canton_Continuation';OUT.mkdir(exist_ok=True)
(OUT/'Implementation.json').unlink(missing_ok=True)
DEST='/Game/Canton/DistrictPrototype/Completion'
MAP='/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL'
SOURCE=ROOT/'ContentSource/CantonDistrict'
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary
report={'historically_accepted':False,'source':'engineering interpretation; source gate dimensions preserved','meshes':{}}
# Asset-only scene import on a transient empty map, then remove all import actors.
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
for name in ('SM_Canton_Paving','SM_Canton_GrassTuft','SM_Canton_Leaf','SM_Canton_DampPatch'):
 target=DEST+'/Meshes/'+name
 if True:  # deterministic in-place reimport of our own generated modules
  pipe=unreal.InterchangeGenericAssetsPipeline();pipe.import_offset_rotation=unreal.Rotator()
  pipe.common_meshes_properties.bake_meshes=False;pipe.common_meshes_properties.bake_pivot_meshes=False
  pipe.mesh_pipeline.build_nanite=False;pipe.mesh_pipeline.set_editor_property('collision',False)
  pipe.mesh_pipeline.generate_lightmap_u_vs=False;pipe.material_pipeline.import_materials=False;pipe.material_pipeline.texture_pipeline.import_textures=False
  scene=unreal.InterchangeGenericLevelPipeline();scene.scene_hierarchy_type=unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
  params=unreal.ImportAssetParameters();params.is_automated=True;params.replace_existing=True
  params.import_level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
  params.override_pipelines=[unreal.SoftObjectPath(pipe.get_path_name()),unreal.SoftObjectPath(scene.get_path_name())]
  manager=unreal.InterchangeManager.get_interchange_manager_scripted()
  assert manager.import_scene(DEST+'/Meshes',manager.create_source_data(str(SOURCE/(name+'.glb'))),params)
  for actor in list(A.get_all_level_actors()):
   if actor.get_component_by_class(unreal.StaticMeshComponent):A.destroy_actor(actor)
 mesh=E.load_asset(target);assert mesh,target
 if name=='SM_Canton_Paving':
  # Simple box follows the 100 cm module envelope; decorative joints are sub-capsule details.
  sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
  sub.remove_collisions(mesh);sub.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
 E.save_loaded_asset(mesh)
 report['meshes'][name]=str(mesh.get_bounding_box())
textures={}
for path in sorted(SOURCE.glob('*.png')):
 target=DEST+'/Textures/'+path.stem
 if not E.does_asset_exist(target):
  task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=DEST+'/Textures';task.destination_name=path.stem;task.automated=True;task.save=True
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 tex=E.load_asset(target);assert tex,target
 normal=path.stem.endswith('Normal');rough=path.stem.endswith('Roughness')
 tex.set_editor_property('srgb',not(normal or rough))
 tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if rough else unreal.TextureCompressionSettings.TC_DEFAULT)
 if normal:tex.set_editor_property('flip_green_channel',True)
 E.save_loaded_asset(tex);textures[path.stem]=tex

def material(name,family=None,color=None,roughness=.85,two=False):
 path=DEST+'/Materials/'+name
 mat=E.load_asset(path) if E.does_asset_exist(path) else None
 if mat:return mat
 mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 mat.set_editor_property('two_sided',two)
 if family:
  for suffix,prop,sampler,out in [('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,'RGB'),('Normal',unreal.MaterialProperty.MP_NORMAL,unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,'RGB'),('Roughness',unreal.MaterialProperty.MP_ROUGHNESS,unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,'R')]:
   node=M.create_material_expression(mat,unreal.MaterialExpressionTextureSample);node.set_editor_property('texture',textures[family+'_'+suffix]);node.set_editor_property('sampler_type',sampler);assert M.connect_material_property(node,out,prop)
 else:
  node=M.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);node.set_editor_property('constant',unreal.LinearColor(*color,1));M.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
  node=M.create_material_expression(mat,unreal.MaterialExpressionConstant);node.set_editor_property('r',roughness);M.connect_material_property(node,'',unreal.MaterialProperty.MP_ROUGHNESS)
 M.recompile_material(mat);E.save_loaded_asset(mat);return mat
mats={f:material('M_Canton_'+f,f) for f in ('Stone','Earth','Pebble')}
mats['Grass']=material('M_Canton_Grass',color=(.08,.13,.035),roughness=.96,two=True)
mats['Leaf']=material('M_Canton_Leaf',color=(.14,.065,.018),roughness=.95,two=True)
mats['Damp']=material('M_Canton_Damp',color=(.055,.044,.029),roughness=.24,two=True)
meshes={n:E.load_asset(DEST+'/Meshes/'+n) for n in report['meshes']}
w=unreal.EditorLoadingAndSavingUtils.load_map(MAP);assert w and unreal.CantonTerrainLibrary.load_provisional_region(w)
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic();phase=0
heights=memoryview((ROOT/'Export/Provisional/Canton_Modern_Context_SouthFirst.r16').read_bytes()).cast('H')
def z_at(x,y):
 fx,fy=x/200,y/200;i,j=math.floor(fx),math.floor(fy);u,v=fx-i,fy-j
 z=lambda a,b:(heights[b*2017+a]-32768)*50/128
 return (1-u)*(1-v)*z(i,j)+u*(1-v)*z(i+1,j)+(1-u)*v*z(i,j+1)+u*v*z(i+1,j+1)

def box(label,center,size,material=None,hidden=False,rotation=None):
 actor=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*center),rotation or unreal.Rotator());actor.set_actor_label(label)
 actor.static_mesh_component.set_static_mesh(E.load_asset('/Engine/BasicShapes/Cube'));actor.set_actor_scale3d(unreal.Vector(*(n/100 for n in size)))
 if material:actor.static_mesh_component.set_material(0,material)
 actor.set_actor_hidden_in_game(hidden);actor.static_mesh_component.set_visibility(not hidden)
 actor.static_mesh_component.set_cast_shadow(not hidden)
 actor.set_editor_property('tags',['Canton.Continuation.EngineeringOnly']);return actor

def finish(dt):
 global phase,started
 if time.monotonic()-started<(25 if phase==0 else 35):return
 if phase==2:return
 try:
  if phase==0:
   phase=2
   actors=list(A.get_all_level_actors())
   for a in actors:
    if a.get_actor_label().startswith(('Canton_Proxy_','Canton_Approach_','Canton_Edge_','Canton_Debris_')):A.destroy_actor(a)
   gate=next(a for a in actors if a.get_actor_label().startswith('Wenmingmen_PROVISIONAL_'))
   g=gate.get_actor_location();report['gate_origin_cm']=[g.x,g.y,g.z]
   for c in gate.get_components_by_class(unreal.StaticMeshComponent):
    c.modify(True);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_editor_property('can_ever_affect_navigation',False)
   gate.modify(True);gate.set_actor_enable_collision(False)
   # Dimensions taken from the local gate generator; source +Y maps to UE -Y.
   # Two wall wings plus a closed central door. No collision is removed to permit transit.
   for side in (-1,1):
    box('Canton_Proxy_Wall_'+str(side),(g.x+side*1775,g.y,g.z+480),(2950,420,960),hidden=True)
   box('Canton_Proxy_ClosedDoor',(g.x,g.y+187,g.z+290),(520,25,580),hidden=True)
   box('Canton_Proxy_ArchHeader',(g.x,g.y,g.z+800),(600,420,340),hidden=True)
   # Walkable collision under the source bridge deck, rather than its high-poly faces.
   def deck(y):return g.z+15+150*math.sin(math.pi*max(0,min(1,(2350-y)/2045)))**1.3+12
   for i in range(41):
    y0=305+i*2045/41;y1=305+(i+1)*2045/41;z0,z1=deck(y0),deck(y1)
    box('Canton_Proxy_Bridge_%02d'%i,(g.x,g.y+(y0+y1)/2,(z0+z1)/2-10),(950,math.hypot(y1-y0,z1-z0),20),hidden=True,rotation=unreal.Rotator(roll=-math.degrees(math.atan2(z1-z0,y1-y0))))
   for side in (-1,1):box('Canton_Proxy_BridgeRail_'+str(side),(g.x+side*455,g.y+1327,g.z+150),(35,2045,300),hidden=True)
   # Landing outside the closed door and new approach beyond the raised bridge.
   def ramp(label,y0,y1,z0,z1,width):
    return box(label,(g.x,(y0+y1)/2,(z0+z1)/2-12),(width,math.hypot(y1-y0,z1-z0),24),mats['Stone'],rotation=unreal.Rotator(roll=-math.degrees(math.atan2(z1-z0,y1-y0))))
   ramp('Canton_Approach_Threshold',g.y+205,g.y+305,g.z+9,deck(305),520)
   y0=g.y+2350;y1=129000;z0=deck(2350);z1=z_at(g.x,y1)+19
   ramp('Canton_Approach_Outer',y0,y1,z0,z1,600)
   report['approach']={'start_cm':[g.x,y0,z0],'end_cm':[g.x,y1,z1],'grade_percent':abs((z1-z0)/(y1-y0))*100,'threshold_endpoint_cm':[g.x,g.y+250,g.z+18],'historical_ground_correction':False}
   assert report['approach']['grade_percent']<8
   for a in actors:
    label=a.get_actor_label();c=a.get_component_by_class(unreal.StaticMeshComponent)
    if not c:continue
    p=a.get_actor_location()
    if label.startswith('District_Main_Stone_'):
     c.set_static_mesh(meshes['SM_Canton_Paving']);c.set_material(0,mats['Stone'])
     dx=(z_at(p.x+300,p.y)-z_at(p.x-300,p.y))/600
     dy=(z_at(p.x,p.y+100)-z_at(p.x,p.y-100))/200
     a.set_actor_rotation(unreal.Rotator(pitch=math.degrees(math.atan(dx)),roll=-math.degrees(math.atan(dy))),True)
     # Grounded skirts leave the original measured road-top contract unchanged.
     for side in (-1,1):box('Canton_Edge_%s_%d'%(label,side),(p.x+side*292,p.y,z_at(p.x+side*292,p.y)+2),(16,200,34),mats['Stone'],rotation=a.get_actor_rotation())
    elif label.startswith(('District_Main_EarthShoulder_','District_MixedLane_','District_CoveredGutter_')):
     c.set_material(0,mats['Pebble' if label.startswith('District_MixedLane_') else 'Earth'])
    elif label.startswith('District_Weed_'):
     c.set_static_mesh(meshes['SM_Canton_GrassTuft']);c.set_material(0,mats['Grass']);a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_rotation(unreal.Rotator(),True);a.set_actor_location(unreal.Vector(p.x,p.y,z_at(p.x,p.y)),False,True);a.set_actor_enable_collision(False)
    elif label.startswith('District_LocalDamp_'):
     c.set_static_mesh(meshes['SM_Canton_DampPatch']);c.set_material(0,mats['Damp']);a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_rotation(unreal.Rotator(),True);a.set_actor_location(unreal.Vector(p.x,p.y,z_at(p.x,p.y)+.6),False,True);a.set_actor_enable_collision(False)
    else:continue
    a.modify(True)
   # Sparse leaf litter at road edges, outside paved/gutter and mixed-lane buffers.
   for i in range(64):
    x=180000+(-1 if i%2 else 1)*(820+(i%7)*43);y=124200+i*205
    if abs(y-132000)<350:continue
    a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z_at(x,y)+1),unreal.Rotator(yaw=i*137.5));a.set_actor_label('Canton_Debris_%02d'%i)
    a.static_mesh_component.set_static_mesh(meshes['SM_Canton_Leaf']);a.static_mesh_component.set_material(0,mats['Leaf']);a.set_actor_enable_collision(False)
   # Reuse six paint targets; add real scale-aware soil texture to the existing unpainted base.
   land=E.load_asset('/Game/Canton/DistrictPrototype/Terrain/M_Canton_Landscape_PROVISIONAL')
   # Keep editable layer colour graph; surface detail adds normal and roughness texture.
   if not E.does_asset_exist(DEST+'/Materials/M_LandscapeDetail_Applied'):
    for suffix,prop,sampler,out in [('Normal',unreal.MaterialProperty.MP_NORMAL,unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,'RGB'),('Roughness',unreal.MaterialProperty.MP_ROUGHNESS,unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,'R')]:
     node=M.create_material_expression(land,unreal.MaterialExpressionTextureSample);node.set_editor_property('texture',textures['Earth_'+suffix]);node.set_editor_property('sampler_type',sampler);M.connect_material_property(node,out,prop)
    M.recompile_material(land);E.save_loaded_asset(land)
    E.duplicate_asset(DEST+'/Materials/M_Canton_Earth',DEST+'/Materials/M_LandscapeDetail_Applied')
   assert unreal.CantonTerrainLibrary.configure_district_navigation(w)
   for a in A.get_all_level_actors():
    if isinstance(a,unreal.RecastNavMesh):
     a.set_editor_property('agent_radius',35);a.set_editor_property('agent_height',180);a.set_editor_property('agent_max_slope',35);a.set_editor_property('runtime_generation',unreal.RuntimeGenerationType.DYNAMIC)
   w.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/Aura.CantonDistrictGameMode'))
   unreal.SystemLibrary.execute_console_command(w,'BuildPaths')
   started=time.monotonic();phase=1;return
  phase=2
  unreal.SystemLibrary.execute_console_command(w,'MAP CHECK')
  assert unreal.EditorLoadingAndSavingUtils.save_map(w,MAP)
  packages=[a.get_package() for a in A.get_all_level_actors() if '/Canton/DistrictPrototype/' in a.get_package().get_name()]
  packages+=list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
  packages=[p for p in packages if '/Canton/DistrictPrototype/' in p.get_name()]
  assert unreal.EditorLoadingAndSavingUtils.save_packages(list(set(packages)),False)
  report['passed']=True;report['saved_packages']=len(packages)
  (OUT/'Implementation.json').write_text(json.dumps(report,indent=2)+'\n')
 except Exception:
  phase=2;report['passed']=False;report['error']=traceback.format_exc();(OUT/'Implementation.json').write_text(json.dumps(report,indent=2));unreal.log_error(report['error'])
 finally:
  if phase==2:
   unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(finish)
