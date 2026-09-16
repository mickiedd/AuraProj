"""Place the three V3 building Blueprints in the Scifi Desert showcase level."""
import json
from pathlib import Path
import unreal

LEVEL_PATH = '/Game/Scifi_desert_city/Level/L_showcase_level'
ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
MANIFEST = ROOT / 'v3-level-placement.json'
ACTOR_TAG = 'ImportedGuangzhouLandmarkV3'
PARK_TAG = 'GuangzhouLandmarkPark'
PADDING = 1200.0
GRID_STEP = 5000.0
EDGE_MARGIN = 12000.0

NAMES = ['Xiaobeimen_AAA_V3', 'Xiaobeimen_Production_V3', 'Zhengnanmen_AAA_V3']

def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x-extent.x, origin.y-extent.y, origin.z-extent.z,
            origin.x+extent.x, origin.y+extent.y, origin.z+extent.z]

def overlap(a, b, padding=PADDING):
    return (a[0]-padding < b[3] and a[3]+padding > b[0] and
            a[1]-padding < b[4] and a[4]+padding > b[1])

def load_level():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert subsystem.load_level(LEVEL_PATH), LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world, LEVEL_PATH
    return world

def ignored_class(cls):
    return cls.startswith(('Landscape', 'RecastNavMesh', 'NavMeshBoundsVolume',
        'InstancedFoliageActor', 'DirectionalLight', 'SkyLight', 'SkyAtmosphere',
        'PostProcessVolume', 'ExponentialHeightFog', 'BlockingVolume', 'WorldSettings'))

def scene_geometry(all_actors, landscapes):
    result = []
    for actor in all_actors:
        cls = actor.get_class().get_name()
        if actor in landscapes or ignored_class(cls):
            continue
        if not cls.startswith('StaticMeshActor'):
            try:
                if not actor.get_components_by_class(unreal.StaticMeshComponent):
                    continue
            except Exception:
                continue
        b = bounds(actor)
        if (b[3]-b[0]) > 10 and (b[4]-b[1]) > 10:
            result.append({'label': actor.get_actor_label(), 'bbox': b})
    return result

def probe_blueprint(actors, name):
    report = json.loads((ROOT / (name + '-import.json')).read_text())
    bp = unreal.EditorAssetLibrary.load_asset(report['blueprint'])
    assert bp and bp.generated_class(), report['blueprint']
    probe = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0,0,0), unreal.Rotator())
    assert probe, name
    b = bounds(probe)
    actors.destroy_actor(probe)
    return {'name': name, 'blueprint': report['blueprint'], 'class': bp.generated_class().get_path_name(),
            'local_bounds': b, 'half': [(b[3]-b[0])/2.0, (b[4]-b[1])/2.0],
            'height': b[5]-b[2]}

def ground_z(world, x, y, ignore):
    query = getattr(unreal.ObjectTypeQuery, 'OBJECT_TYPE_QUERY1', None)
    draw = getattr(unreal.DrawDebugTrace, 'NONE', None)
    if query is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(query)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -5000.0),
        types, True, ignore, draw, False)
    data = hit.to_tuple()
    return float(data[4].z) if data and data[0] is True else 0.0

def main():
    world = load_level()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    existing = [a for a in all_actors if ACTOR_TAG in [str(t) for t in a.tags]]
    assert not existing, 'V3 placement already exists: ' + ', '.join(a.get_actor_label() for a in existing)
    landscapes = [a for a in all_actors if a.get_class().get_name().startswith('Landscape')]
    assert landscapes, 'No Landscape actors found'
    terrain = [bounds(a) for a in landscapes]
    rect = (min(b[0] for b in terrain)+EDGE_MARGIN, min(b[1] for b in terrain)+EDGE_MARGIN,
            max(b[3] for b in terrain)-EDGE_MARGIN, max(b[4] for b in terrain)-EDGE_MARGIN)
    geometry = scene_geometry(all_actors, landscapes)
    probes = [probe_blueprint(actors, name) for name in NAMES]
    candidates=[]
    x=rect[0]
    while x <= rect[2]:
        y=rect[3]
        while y >= rect[1]:
            candidates.append((x,y))
            y -= GRID_STEP
        x += GRID_STEP
    chosen=[]
    for probe in probes:
        hx,hy=probe['half']
        found=None
        for cx,cy in candidates:
            b=[cx-hx,cy-hy,0.0,cx+hx,cy+hy,probe['height']]
            if any(overlap(b, item['bbox']) for item in geometry):
                continue
            if any(overlap(b, item['bbox'], padding=2500.0) for item in chosen):
                continue
            found=(cx,cy,b)
            break
        assert found, 'No clear placement slot for '+probe['name']
        probe['center_xy']=[found[0],found[1]]
        probe['candidate_bbox_xy']=found[2][0:2]+found[2][3:5]
        probe['bbox_probe']=found[2]
        chosen.append({'name':probe['name'], 'bbox':found[2]})
    ignore=unreal.Array(unreal.Actor)
    for actor in all_actors:
        if not actor.get_class().get_name().startswith('Landscape'):
            ignore.append(actor)
    manifest={'level':LEVEL_PATH,'tag':ACTOR_TAG,'park_tag':PARK_TAG,'terrain_rect':list(rect),'actors':[]}
    for probe in probes:
        b=probe['local_bounds']
        cx,cy=probe['center_xy']
        gz=ground_z(world,cx,cy,ignore)
        loc=unreal.Vector(cx-(b[0]+b[3])/2.0, cy-(b[1]+b[4])/2.0, gz-b[2])
        report=json.loads((ROOT/(probe['name']+'-import.json')).read_text())
        actor=actors.spawn_actor_from_class(unreal.EditorAssetLibrary.load_asset(report['blueprint']).generated_class(),loc,unreal.Rotator())
        assert actor, probe['name']
        label='GuangzhouLandmark_'+probe['name']
        actor.set_actor_label(label)
        actor.tags=[ACTOR_TAG,PARK_TAG,'V3Building']
        actual=bounds(actor)
        assert abs(actual[2]-gz) <= 2.0, (probe['name'],actual,gz)
        entry={'name':probe['name'],'label':label,'actor':actor.get_name(),'blueprint':report['blueprint'],
               'class':probe['class'],'location':[actor.get_actor_location().x,actor.get_actor_location().y,actor.get_actor_location().z],
               'rotation':[actor.get_actor_rotation().pitch,actor.get_actor_rotation().yaw,actor.get_actor_rotation().roll],
               'center_xy':[cx,cy],'ground_z':gz,'bounds':actual,'size_cm':[actual[3]-actual[0],actual[4]-actual[1],actual[5]-actual[2]],
               'visible_components':report['visible_components'],'collision_components':report['collision_components']}
        manifest['actors'].append(entry)
        print('V3_PLACED',json.dumps(entry))
    placed=[bounds(a) for a in actors.get_all_level_actors() if ACTOR_TAG in [str(t) for t in a.tags]]
    assert len(placed)==3
    for i,left in enumerate(placed):
        for right in placed[i+1:]:
            assert not overlap(left,right,padding=100.0), (left,right)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,LEVEL_PATH), 'Level save failed'
    manifest['passed']=True
    MANIFEST.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('V3_LEVEL_PLACEMENT_COMPLETE',MANIFEST)

if __name__=='__main__':
    main()
