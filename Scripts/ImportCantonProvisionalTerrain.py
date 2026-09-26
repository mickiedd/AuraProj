"""Run in a fresh full UE editor; creates only Canton/Provisional packages."""
import hashlib
import json
import math
import struct
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT=ROOT/'QA/UE_Import_Screenshots'
OUT.mkdir(parents=True,exist_ok=True)
CONTRACT=json.loads((ROOT/'Data/Canton_Prototype_Contract.json').read_text())
MAP=CONTRACT['map']
RAW=ROOT/CONTRACT['raw_height_path']
assert hashlib.sha256(RAW.read_bytes()).hexdigest()==CONTRACT['raw_sha256']
assert CONTRACT['historically_accepted'] is False and CONTRACT['historical_terrain_zero_m'] is None
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lib=unreal.EditorAssetLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=None


def fail():
    (OUT/'Import_Error.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc())
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def material(name,color):
    folder='/Game/Canton/Provisional/Terrain'
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    node=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',.85)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(m)
    lib.save_loaded_asset(m)
    return m


def start():
    global world, heights, started, handle
    world=unreal.CantonTerrainLibrary.create_provisional_world()
    assert world, 'Map exists or WP map creation failed; refusing overwrite'
    assert unreal.CantonTerrainLibrary.import_provisional_terrain(world,str(RAW)), 'Native import failed'
    heights=memoryview(RAW.read_bytes()).cast('H')
    ground=material('M_Diagnostic_PROVISIONAL',(.34,.38,.31))
    instance=unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_Diagnostic_PROVISIONAL','/Game/Canton/Provisional/Terrain',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance,ground)
    lib.save_loaded_asset(instance)
    for a in actors.get_all_level_actors():
        if isinstance(a,unreal.LandscapeProxy):
            a.set_editor_property('landscape_material',instance)
    wallmat=material('M_WallMarkers_PROVISIONAL',(.7,.46,.12))
    gatemat=material('M_GateMarkers_PROVISIONAL',(.1,.55,.8))
    cube=lib.load_asset('/Engine/BasicShapes/Cube')
    def z_at(x,y):
        col=max(0,min(2016,round(x/200)));row=max(0,min(2016,round(y/200)))
        return (heights[row*2017+col]-32768)*50/128
    def marker(label,x,y,sx,sy,sz,mat,yaw=0):
        a=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z_at(x,y)+sz*50),unreal.Rotator(yaw=yaw))
        a.set_actor_label(label)
        a.static_mesh_component.set_static_mesh(cube)
        a.static_mesh_component.set_material(0,mat)
        a.set_actor_scale3d(unreal.Vector(sx,sy,sz))
        a.set_actor_enable_collision(False)
        a.set_editor_property('tags',['Canton.Provisional.MapOverlay'])
        return a
    def local(p): return ((p[0]-729400)*100,(p[1]-2557300)*100)
    overlay=json.loads((ROOT/'Export/Provisional/UE_Overlay_Metres.json').read_text())
    walls=overlay['layers']['Wall']
    for fi,f in enumerate(walls['features']):
        geom=f['geometry'];lines=geom['coordinates'] if geom['type']=='Polygon' else [geom['coordinates']]
        for line in lines:
            for si,(p,q) in enumerate(zip(line,line[1:])):
                x,y=local(p);xx,yy=local(q)
                distance=math.hypot(xx-x,yy-y)
                # Split long segments so the coarse trace follows the modern hills.
                count=max(1,math.ceil(distance/3000))
                for k in range(count):
                    t=(k+.5)/count
                    marker(f'PROVISIONAL_Wall_{fi}_{si}_{k}',x+(xx-x)*t,y+(yy-y)*t,distance/count/100,4,5,wallmat,math.degrees(math.atan2(yy-y,xx-x)))
    gates=overlay['layers']['Gates']
    for i,f in enumerate(gates['features']):
        x,y=local(f['geometry']['coordinates'])
        marker(f'PROVISIONAL_Gate_{i}',x,y,12,12,30,gatemat)
    sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,30000),unreal.Rotator(pitch=-40,yaw=-35))
    sun.light_component.set_intensity(4)
    fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,30000),unreal.Rotator(pitch=-55,yaw=140))
    fill.light_component.set_intensity(1.5)
    fill.light_component.set_editor_property('cast_shadows',False)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-90000,-90000,290000),unreal.Rotator(pitch=-35,yaw=45))
    started=time.monotonic()
    handle=unreal.register_slate_post_tick_callback(finish)


def finish(dt):
    global handle
    if time.monotonic()-started<45:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        result=json.loads(unreal.CantonTerrainLibrary.validate_provisional_terrain(world,str(RAW)))
        (OUT/'Creation_Validation.json').write_text(json.dumps(result,indent=2)+'\n')
        assert result['passed'],result
        assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
        packages=list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        packages=[p for p in packages if '/Canton/Provisional/' in p.get_name()]
        if packages:assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,True)
        (OUT/'Import_Settings.json').write_text(json.dumps(CONTRACT,indent=2)+'\n')
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    except Exception:fail()

try:start()
except Exception:fail()
