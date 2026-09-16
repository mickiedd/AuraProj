"""Validate saved/reloaded V3 Blueprint placements in the Scifi Desert level."""
import json
from pathlib import Path
import unreal

LEVEL_PATH='/Game/Scifi_desert_city/Level/L_showcase_level'
ROOT=Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
MANIFEST=ROOT/'v3-level-placement.json'
OUT=ROOT/'v3-level-placement-validation.json'
TAG='ImportedGuangzhouLandmarkV3'

def bounds(actor):
    o,e=actor.get_actor_bounds(False)
    return [o.x-e.x,o.y-e.y,o.z-e.z,o.x+e.x,o.y+e.y,o.z+e.z]

def overlap(a,b,padding=1.0):
    return a[0]-padding < b[3] and a[3]+padding > b[0] and a[1]-padding < b[4] and a[4]+padding > b[1]

def ignored(cls):
    return cls.startswith(('Landscape','RecastNavMesh','NavMeshBoundsVolume','InstancedFoliageActor',
        'DirectionalLight','SkyLight','SkyAtmosphere','PostProcessVolume','ExponentialHeightFog',
        'BlockingVolume','WorldSettings'))

def main():
    manifest=json.loads(MANIFEST.read_text())
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH), LEVEL_PATH
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    tagged=[a for a in actors if TAG in [str(t) for t in a.tags]]
    assert len(tagged)==3, len(tagged)
    expected={e['label']:e for e in manifest['actors']}
    assert set(expected)=={a.get_actor_label() for a in tagged}
    rows=[]
    for actor in tagged:
        entry=expected[actor.get_actor_label()]
        bp=unreal.EditorAssetLibrary.load_asset(entry['blueprint'])
        assert actor.get_class().get_path_name()==bp.generated_class().get_path_name(), actor.get_actor_label()
        location=actor.get_actor_location()
        saved=entry['location']
        assert max(abs(location.x-saved[0]),abs(location.y-saved[1]),abs(location.z-saved[2])) <= 2.0, actor.get_actor_label()
        actual=bounds(actor)
        assert abs(actual[2]-entry['ground_z']) <= 2.0, (actor.get_actor_label(),actual,entry['ground_z'])
        comps=actor.get_components_by_class(unreal.StaticMeshComponent)
        visible=sum(1 for c in comps if c.get_editor_property('visible'))
        hidden=len(comps)-visible
        assert visible==entry['visible_components'] and hidden==entry['collision_components'], actor.get_actor_label()
        rows.append({'label':actor.get_actor_label(),'actor':actor.get_name(),'blueprint':entry['blueprint'],
                     'location':[location.x,location.y,location.z],'bounds':actual,
                     'size_cm':[actual[3]-actual[0],actual[4]-actual[1],actual[5]-actual[2]],
                     'visible_components':visible,'collision_components':hidden,'ground_z':entry['ground_z']})
    for i,left in enumerate(tagged):
        for right in tagged[i+1:]:
            assert not overlap(bounds(left),bounds(right),100.0), (left.get_actor_label(),right.get_actor_label())
    existing=[]
    for actor in actors:
        if actor in tagged or ignored(actor.get_class().get_name()):
            continue
        cls=actor.get_class().get_name()
        if not cls.startswith('StaticMeshActor'):
            try:
                if not actor.get_components_by_class(unreal.StaticMeshComponent):
                    continue
            except Exception:
                continue
        b=bounds(actor)
        if b[3]-b[0]>10 and b[4]-b[1]>10:
            existing.append((actor.get_actor_label(),b))
    conflicts=[]
    for actor in tagged:
        for label,b in existing:
            if overlap(bounds(actor),b,1.0):
                conflicts.append((actor.get_actor_label(),label))
    assert not conflicts, conflicts
    result={'passed':True,'level':LEVEL_PATH,'tag':TAG,'actors':rows,'existing_geometry_checked':len(existing)}
    OUT.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('V3_LEVEL_PLACEMENT_VALIDATED',json.dumps(result))

if __name__=='__main__': main()
