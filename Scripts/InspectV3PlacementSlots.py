"""Find clear terrain slots for the three V3 Blueprint buildings."""
import json
from pathlib import Path
import unreal

LEVEL_PATH = '/Game/Scifi_desert_city/Level/L_showcase_level'
ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
PADDING = 1200.0
GRID = 5000.0
EDGE = 12000.0

def box(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x-extent.x, origin.y-extent.y, origin.z-extent.z,
            origin.x+extent.x, origin.y+extent.y, origin.z+extent.z]

def overlap(a, b, pad=PADDING):
    return (a[0]-pad < b[3] and a[3]+pad > b[0] and
            a[1]-pad < b[4] and a[4]+pad > b[1])

def main():
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    landscapes = [a for a in all_actors if a.get_class().get_name().startswith('Landscape')]
    assert landscapes, 'No landscape actors'
    terrain = [box(a) for a in landscapes]
    rect = (min(b[0] for b in terrain)+EDGE, min(b[1] for b in terrain)+EDGE,
            max(b[3] for b in terrain)-EDGE, max(b[4] for b in terrain)-EDGE)
    existing = []
    ignored_classes = ('Landscape', 'RecastNavMesh', 'NavMeshBoundsVolume', 'InstancedFoliageActor',
                       'DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'PostProcessVolume',
                       'ExponentialHeightFog', 'BlockingVolume', 'WorldSettings')
    for actor in all_actors:
        cls = actor.get_class().get_name()
        if actor in landscapes or cls.startswith(ignored_classes):
            continue
        if not cls.startswith('StaticMeshActor'):
            try:
                if not actor.get_components_by_class(unreal.StaticMeshComponent):
                    continue
            except Exception:
                continue
        b = box(actor)
        if (b[3]-b[0]) > 10 and (b[4]-b[1]) > 10:
            existing.append((actor.get_actor_label(), b))
    names = ['Xiaobeimen_AAA_V3', 'Xiaobeimen_Production_V3', 'Zhengnanmen_AAA_V3']
    sizes = {}
    for name in names:
        report = json.loads((ROOT / (name+'-import.json')).read_text())
        bp = unreal.EditorAssetLibrary.load_asset(report['blueprint'])
        probe = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0,0,0), unreal.Rotator())
        assert probe, name
        b = box(probe)
        sizes[name] = {'half': [(b[3]-b[0])/2, (b[4]-b[1])/2], 'height': b[5]-b[2], 'local_origin': [b[0],b[1],b[2]]}
        actors.destroy_actor(probe)
    candidates=[]
    min_x,min_y,max_x,max_y=rect
    x=min_x
    while x <= max_x:
        y=max_y
        while y >= min_y:
            candidates.append((x,y))
            y -= GRID
        x += GRID
    chosen=[]
    diagnostics=[]
    for name in names:
        found=[]
        hx,hy=sizes[name]['half']
        for cx,cy in candidates:
            cand=[cx-hx,cy-hy,0,cx+hx,cy+hy,sizes[name]['height']]
            if any(overlap(cand,b) for _,b in existing):
                continue
            if any(overlap(cand,b,pad=2500.0) for _,b in chosen):
                continue
            found.append({'center_xy':[cx,cy], 'bbox_xy':[cand[0],cand[1],cand[3],cand[4]], 'size_xy':[hx*2,hy*2]})
            if len(found)>=8: break
        diagnostics.append({'name':name, 'options':found})
        if found:
            chosen.append((name, [found[0]['bbox_xy'][0], found[0]['bbox_xy'][1], 0, found[0]['bbox_xy'][2], found[0]['bbox_xy'][3], sizes[name]['height']]))
    largest = sorted(existing, key=lambda item: (item[1][3]-item[1][0])*(item[1][4]-item[1][1]), reverse=True)[:40]
    report={'terrain_rect':rect, 'sizes':sizes, 'existing_count':len(existing), 'existing_sample':existing[:30], 'largest':largest, 'diagnostics':diagnostics}
    (ROOT / 'v3-placement-slot-inspect.json').write_text(json.dumps(report, indent=2))
    unreal.log('V3_PLACEMENT_SLOT_INSPECT ' + json.dumps({'terrain_rect':rect,'sizes':sizes,'existing_count':len(existing),'option_counts':{d['name']:len(d['options']) for d in diagnostics}}))

if __name__ == '__main__':
    main()
