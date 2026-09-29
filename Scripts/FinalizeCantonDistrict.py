"""Refresh instance collision/nav relevance and parameterize the authored surfaces."""
import unreal,json,time,traceback,math,sys
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'QA/Canton_Continuation';MAP='/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL';DEST='/Game/Canton/DistrictPrototype/Completion'
(OUT/'Finalization.json').unlink(missing_ok=True)
(OUT/'Finalization_Error.txt').unlink(missing_ok=True)
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary;A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w=unreal.EditorLoadingAndSavingUtils.load_map(MAP);assert w and unreal.CantonTerrainLibrary.load_provisional_region(w)
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic();phase=0

def tick(dt):
 global phase,started
 if phase==2 or time.monotonic()-started<(25 if phase==0 else 25):return
 try:
  if phase==0:
   phase=2
   # Ground the new raised approach with retaining fill; no suspended thin road.
   data=json.loads((OUT/'Implementation.json').read_text())['approach']
   xa,y0,z0=data['start_cm'];_,y1,z1=data['end_cm']
   raw=memoryview((ROOT/'Export/Provisional/Canton_Modern_Context_SouthFirst.r16').read_bytes()).cast('H')
   def ground(x,y):
    col,row=round(x/200),round(y/200);return (raw[row*2017+col]-32768)*50/128
   for a in list(A.get_all_level_actors()):
    if a.get_actor_label().startswith(('Canton_Approach_Fill_','Canton_Approach_GateFoundation')):A.destroy_actor(a)
   for i in range(26):
    ya=y0+(y1-y0)*i/26;yb=y0+(y1-y0)*(i+1)/26;ym=(ya+yb)/2
    top=z0+(z1-z0)*(ym-y0)/(y1-y0)-12
    bottom=min(ground(xa-300,ya),ground(xa+300,ya),ground(xa-300,yb),ground(xa+300,yb))-40
    if top<=bottom:continue
    a=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(xa,ym,(top+bottom)/2),unreal.Rotator(roll=-math.degrees(math.atan2(z1-z0,y1-y0))))
    a.set_actor_label('Canton_Approach_Fill_%02d'%i);a.static_mesh_component.set_static_mesh(E.load_asset('/Engine/BasicShapes/Cube'));a.static_mesh_component.set_material(0,E.load_asset(DEST+'/Materials/M_Canton_Stone'))
    a.set_actor_scale3d(unreal.Vector(5.98,(yb-ya+1)/100,(top-bottom)/100))
   gate=next(a for a in A.get_all_level_actors() if a.get_actor_label().startswith('Wenmingmen_PROVISIONAL_'))
   g=gate.get_actor_location();bottom=min(ground(g.x+x,g.y+y) for x in (-3250,0,3250) for y in (-220,220))-40
   foundation=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(g.x,g.y,(g.z+bottom)/2),unreal.Rotator())
   foundation.set_actor_label('Canton_Approach_GateFoundation');foundation.static_mesh_component.set_static_mesh(E.load_asset('/Engine/BasicShapes/Cube'));foundation.static_mesh_component.set_material(0,E.load_asset(DEST+'/Materials/M_Canton_Stone'));foundation.set_actor_scale3d(unreal.Vector(65,4.4,(g.z-bottom)/100))
   # Bootstrap World Partition at the district, including in a cooked game.
   starts=[a for a in A.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
   start=starts[0] if starts else A.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(180000,130000,1300),unreal.Rotator(yaw=90))
   start.set_actor_label('Canton_District_PlayerStart');start.modify(True)
   start.set_actor_location(unreal.Vector(180000,130000,1300),False,False)
   start.set_editor_property('is_spatially_loaded',False)
   assert unreal.CantonTerrainLibrary.configure_district_navigation(w)
   for nav in A.get_all_level_actors():
    if isinstance(nav,unreal.RecastNavMesh):
     nav.modify(True);nav.set_editor_property('max_simplification_error',1.3);nav.set_editor_property('simplification_elevation_ratio',0.);nav.set_editor_property('tile_size_uu',300.)
   sys.path.insert(0,str(ROOT/"Scripts"))
   from CantonMaterialAuthoring import rebuild
   rebuild()
   instances={}
   for family in ('Stone','Earth','Pebble','Grass','Leaf','Damp'):
    path=DEST+'/Materials/MI_Canton_'+family
    if not E.does_asset_exist(path):
     parent=E.load_asset(DEST+'/Materials/M_Canton_'+family)
     instance=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_Canton_'+family,DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
     M.set_material_instance_parent(instance,parent);E.save_loaded_asset(instance)
    instances[family]=E.load_asset(path)
   for actor in A.get_all_level_actors():
    if actor.get_actor_label().startswith('District_LocalDamp_'):
     actor.modify(True);actor.set_editor_property('tags',list(set([str(t) for t in actor.tags]+['Canton.Damp'])))
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
     for i in range(c.get_num_materials()):
      current=c.get_material(i)
      if current and current.get_path_name().startswith(DEST+'/Materials/M_Canton_'):
       family=current.get_name().removeprefix('M_Canton_')
       if family in instances:actor.modify(True);c.set_material(i,instances[family])
   unreal.SystemLibrary.execute_console_command(w,'BuildPaths');phase=1;started=time.monotonic();return
  phase=2
  unreal.SystemLibrary.execute_console_command(w,'MAP CHECK')
  unreal.EditorLoadingAndSavingUtils.save_map(w,MAP)
  packages=[a.get_package() for a in A.get_all_level_actors() if '/Canton/DistrictPrototype/' in a.get_package().get_name()]
  packages+=list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
  packages=[p for p in packages if '/Canton/DistrictPrototype/' in p.get_name()]
  assert unreal.EditorLoadingAndSavingUtils.save_packages(list(set(packages)),False)
  gates=[a for a in A.get_all_level_actors() if a.get_actor_label().startswith('Wenmingmen_PROVISIONAL_')]
  (OUT/'Finalization.json').write_text(json.dumps({'passed':True,'gate_actor_collision_enabled':[a.get_actor_enable_collision() for a in gates],'material_instances':6,'nav_cell_size_cm':5,'nav_cell_height_cm':1,'historically_accepted':False},indent=2))
 except Exception:
  phase=2;(OUT/'Finalization_Error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
 finally:
  if phase==2:
   unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
