"""Check actual imported stone instances for planarity, joints and arch clearance."""
import collections,json,math,sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CorrectZhengnanmenAssembly import BP,templates,snapshot

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenFlatMasonry'
applied=json.loads((OUT/'apply.json').read_text());manifest=json.loads(Path(applied['manifest']).read_text())
bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
intact_report=OUT.parent/'ZhengnanmenIntactPerspective/apply.json'
expected=json.loads(intact_report.read_text())['after'] if intact_report.exists() else applied['after']
rows=templates(bp)[1];assert snapshot(rows)==expected
c=next(c for h,c in rows if c.get_name()==applied['component'])
assert c.static_mesh.get_num_triangles(0)==44
assert c.get_instance_count()==4150
groups=collections.defaultdict(list);depths=collections.defaultdict(list)
for i,cell in enumerate(manifest['cells']):
    t=c.get_instance_transform(i,False)
    loc=[t.translation.x,t.translation.y,t.translation.z];scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z]
    assert max(abs(a-b) for a,b in zip(loc,cell['location']))<.001
    assert max(abs(a-b) for a,b in zip(scale,cell['scale']))<.00001
    assert abs(t.rotation.x-math.sqrt(.5))<.00001 and abs(t.rotation.w-math.sqrt(.5))<.00001
    wall=cell['wall'];sign=cell['sign'];axis=1 if wall=='FB' else 0
    depth=scale[2]*100 if wall=='FB' else scale[0]*100
    outer=abs(loc[axis])+depth/2
    assert abs(outer-(800 if wall=='FB' else 1290))<.001
    depths[(wall,sign)].append(outer)
    lateral=loc[0] if wall=='FB' else loc[1]
    width=(scale[0] if wall=='FB' else scale[2])*100
    height=scale[1]*100
    bottom=loc[2]-height/2;top=loc[2]+height/2
    groups[(wall,sign,cell['row'])].append((lateral-width/2,lateral+width/2,bottom,top))
    if wall=='FB' and bottom<560:
        half=215 if bottom<=345 else math.sqrt(max(0,215**2-(bottom-345)**2))
        assert abs(lateral)-width/2>=half+.59,'Brick crosses clear arch opening'
assert len(depths)==4 and len(groups)==92
for face in depths:assert max(depths[face])-min(depths[face])<.001
for face in depths:
    courses=[]
    for row in range(23):
        blocks=sorted(groups[(*face,row)])
        assert max(b[2] for b in blocks)-min(b[2] for b in blocks)<.001
        assert max(b[3] for b in blocks)-min(b[3] for b in blocks)<.001
        assert all(b[0]-a[1]>=1.199 for a,b in zip(blocks,blocks[1:])), 'Overlapping bricks or closed joint'
        courses.append((blocks[0][2],blocks[0][3]))
    assert all(abs(b[0]-a[1]-1.2)<.001 for a,b in zip(courses,courses[1:])), 'Uneven bed joints'
assert len(rows)==114 and sum(r['instances'] for r in applied['after'].values())==12383
assert not any('UltraAAA_HeroGeometry' in r for r in applied['after'])
mode=globals().get('VALIDATION_MODE','live')
report=dict(passed=True,mode=mode,wall_instances=4150,total_instances=12383,components=114,planar_wall_faces=4,level_courses_per_face=23,joints_cm=1.2,overlap_count=0,arch_intruding_cells=0,non_wall_components_preserved=113,prototype_triangles=44)
(OUT/('validate-'+mode+'.json')).write_text(json.dumps(report,indent=2))
print('ZNM_FLAT_MASONRY_VALIDATED',json.dumps(report))
