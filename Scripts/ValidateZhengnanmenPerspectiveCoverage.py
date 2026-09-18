"""Host-side front-face ray coverage on actual UE exported envelope meshes.

Checks six faces of floor/foundation/door solids, both sides of all roofs, and
the inside of the arched soffit. Uses preserved HISM transforms, not a screenshot
or the generated FBX, so flipped faces and perspective holes can fail this gate.
"""
import json
from pathlib import Path
import numpy as np
import trimesh
from scipy.spatial.transform import Rotation
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
manifest=json.loads((ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_IntactEnvelope_20260916/envelope-manifest.json').read_text())
instances={r['name']:r['instances'] for r in json.loads((OUT/'before-instances.json').read_text())}

def world_mesh(e):
    m=trimesh.load(OUT/'Imported'/(e['mesh_name']+'.obj'),force='mesh',process=False)
    t=instances[e['component']];assert len(t)==1;t=t[0]
    # Unreal OBJ exporter swaps Y/Z. Undo that reflection, including winding.
    v=Rotation.from_quat(t['rotation']).apply(m.vertices[:,(0,2,1)]*t['scale'])+t['location']
    m=trimesh.Trimesh(vertices=v,faces=m.faces[:,::-1],process=False)
    assert m.volume>0
    return m

def ray(m,origin,direction):
    tri=m.triangles;a=tri[:,0];e1=tri[:,1]-a;e2=tri[:,2]-a
    p=np.cross(np.broadcast_to(direction,e2.shape),e2);det=np.einsum('ij,ij->i',e1,p)
    valid=abs(det)>1e-10;inv=np.zeros_like(det);inv[valid]=1/det[valid]
    s=origin-a;u=np.einsum('ij,ij->i',s,p)*inv;q=np.cross(s,e1)
    v=q@direction*inv;distance=np.einsum('ij,ij->i',e2,q)*inv
    hit=valid&(u>=-1e-7)&(v>=-1e-7)&(u+v<=1+1e-7)&(distance>1e-5)
    assert hit.any(),'No geometry along coverage ray'
    near=np.min(distance[hit]);indices=hit&(abs(distance-near)<.001)
    assert np.any(m.face_normals[indices]@direction<-.001),'First surface faces away from external viewer'
    return float(near)

rows=[]
for e in manifest['entries']:
    role=e['source_role']
    if not (role.startswith(('FloorCore_','Base_','RoofShell_')) or role.startswith('Door_') or role=='TunnelArch'):continue
    m=world_mesh(e);lo,hi=m.bounds;count=0
    if role.startswith(('FloorCore_','Base_','Door_')):
        for axis in range(3):
            other=[i for i in range(3) if i!=axis]
            for sign in (-1,1):
                for u in np.linspace(.08,.92,9):
                    for v in np.linspace(.08,.92,7):
                        o=(lo+hi)/2;o[other]=(lo+(hi-lo)*np.array([u if i==other[0] else v for i in range(3)]))[other]
                        o[axis]=(hi[axis]+20) if sign==1 else (lo[axis]-20)
                        d=np.zeros(3);d[axis]=-sign;ray(m,o,d);count+=1
    elif role.startswith('RoofShell_'):
        for u in np.linspace(.12,.88,11):
            for v in np.linspace(.12,.88,9):
                for sign in (-1,1):
                    o=np.array([lo[0]+u*(hi[0]-lo[0]),lo[1]+v*(hi[1]-lo[1]),hi[2]+100 if sign==1 else lo[2]-100])
                    ray(m,o,np.array([0,0,-sign]));count+=1
    else:
        # Source arch clear center Z345cm and radius215cm are retained.
        for y in np.linspace(lo[1]+.1*(hi[1]-lo[1]),hi[1]-.1*(hi[1]-lo[1]),9):
            for angle in np.linspace(.05*np.pi,.95*np.pi,31):
                distance=ray(m,np.array([0,y,345]),np.array([np.cos(angle),0,np.sin(angle)]))
                assert 214.8<distance<215.1,(role,distance)
                count+=1
    rows.append(dict(role=role,front_face_rays=count,passed=True))
assert len(rows)==12,len(rows)
report=dict(passed=True,envelope_prototypes=len(rows),front_face_rays=sum(r['front_face_rays'] for r in rows),rows=rows,scope='Solid foundations, three floor cores, three roof undersides/tops, two doors, arched soffit; decorative gaps and passage are intentional')
(OUT/'validate-perspective-coverage.json').write_text(json.dumps(report,indent=2))
print('PERSPECTIVE_FRONT_FACE_COVERAGE_VALIDATED',report['front_face_rays'])
