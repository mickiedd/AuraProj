"""Ray-test visible enclosure, masonry spandrels and fitted door perimeter."""
import json
from pathlib import Path
import numpy as np
import trimesh
P=Path(__file__).resolve().parents[1]
OUT=P/'Saved/Reports/Zhengdongmen'

def load(path):
    scene=trimesh.load(path,force='scene',process=False)
    return {k.rsplit('_',1)[-1]:v for k,v in scene.geometry.items()}

def holes(mesh,origins,directions,max_distance):
    origins=np.array(origins); directions=np.array(directions)
    locations,ray,_=mesh.ray.intersects_location(origins,directions,multiple_hits=True)
    dist=np.linalg.norm(np.asarray(locations).reshape(-1,3)-origins[ray],axis=1)
    hit=set(ray[dist<max_distance].tolist())
    return {'samples':len(origins),'holes':len(origins)-len(hit)}

def check(meshes):
    result={}
    wood=trimesh.util.concatenate([meshes[g] for g in ('Wood','DoorWood','Plaster')])
    for name,hx,hy,z0,z1 in [('lower',11.9,5.55,9.71,12.42),('upper',9.58,3.95,13.69,17.45)]:
        for axis,span,face in [(1,hx,hy),(0,hy,hx)]:
            for sign in (-1,1):
                origins=[];directions=[]
                for a in np.linspace(-span+.03,span-.03,91):
                    for z in np.linspace(z0,z1,37):
                        pt=[a,sign*(face+.35),z] if axis==1 else [sign*(face+.35),a,z]
                        direction=[0,0,0];direction[axis]=-sign
                        origins.append(pt);directions.append(direction)
                result[f'{name}_{axis}_{sign}']=holes(wood,origins,directions,.75)
    origins=[]
    for x in np.linspace(-3.18,3.18,131):
        outer=3.4+np.sqrt(max(0,3.22**2-x*x))
        for z in np.linspace(outer+.025,6.65,17):
            origins.append([x,-8,z])
    result['spandrels']=holes(meshes['Stone'],origins,[[0,1,0]]*len(origins),1)
    origins=[]
    for t in np.linspace(.002,np.pi-.002,361):
        for inset in (.002,.01,.035):
            r=2.66-inset;origins.append([r*np.cos(t),2.0,3.4+r*np.sin(t)])
    for x in np.linspace(-2.65,2.65,81):
        for z in np.linspace(.02,3.39,45):origins.append([x,2.0,z])
    result['door_fit']=holes(meshes['DoorWood'],origins,[[0,1,0]]*len(origins),.4)
    # Masonry must not fill the intended semicircular passage in front of the door.
    origins=[]
    for x in np.linspace(-2.55,2.55,41):
        for z in np.linspace(.4,3.4+np.sqrt(2.66**2-x*x)-.08,41):origins.append([x,-8,z])
    passage=holes(meshes['Stone'],origins,[[0,1,0]]*len(origins),9.9)
    result['passage']={'samples':passage['samples'],'blocked':passage['samples']-passage['holes']}
    return result

report={stage:check(load(OUT/(stage+'.glb'))) for stage in ('before','after')}
report['passed']=all(v.get('holes',v.get('blocked'))==0 for v in report['after'].values())
(OUT/'closure-validation.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
assert report['passed']
