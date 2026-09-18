"""Author a flat, closed, outward-wound stone prototype and aligned courses.

Keep the original four-wall cell mask and arch. Local prototype is 100cm;
existing V2 roll maps local Y to world height. Joints are 1.2cm, relief is
confined to a small geometric bevel and the existing stone PBR maps.
"""
import hashlib,itertools,json,math,struct
from pathlib import Path
import numpy as np
from scipy.spatial import ConvexHull
import trimesh
from GenerateGuangzhouLandmarkDeepGeometry import Builder,write_fbx

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RawModelImport/ZhengnanmenFlatMasonry'
SOURCE=ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_FlatMasonry_20260916'
GLB=Path('C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package/GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb')

def main():
    SOURCE.mkdir(parents=True,exist_ok=True)
    with GLB.open('rb') as f:
        f.read(12);length,kind=struct.unpack('<II',f.read(8));scene=json.loads(f.read(length))
        length,kind=struct.unpack('<II',f.read(8));binary=f.read(length)
    # Confirm the reported defect independently from source positions/indices.
    p=scene['meshes'][0]['primitives'][0]
    def accessor(index):
        a=scene['accessors'][index];v=scene['bufferViews'][a['bufferView']]
        return np.frombuffer(binary,dtype={5126:np.float32,5125:np.uint32,5123:np.uint16}[a['componentType']],count=a['count']*(3 if a['type']=='VEC3' else 1),offset=v.get('byteOffset',0)+a.get('byteOffset',0))
    pos=accessor(p['attributes']['POSITION']).reshape(-1,3)
    tri=pos[accessor(p['indices']).reshape(-1,3)]
    dots=np.einsum('ij,ij->i',np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),tri.mean(1))
    assert len(dots)==120000 and np.all(dots<0),'Unexpected stone source topology'
    # Correct convex bevel topology, not the legacy box helper's edge strips.
    b=.6;verts=[]
    for axis in range(3):
        for signs in itertools.product((-1,1),repeat=3):
            verts.append(tuple(signs[i]*(50 if i==axis else 50-b) for i in range(3)))
    verts=np.array(verts);hull=ConvexHull(verts);faces=hull.simplices.copy()
    for i,face in enumerate(faces):
        a,c,d=verts[face]
        if np.dot(np.cross(c-a,d-a),(a+c+d)/3)<0:faces[i]=face[::-1]
    mesh=trimesh.Trimesh(vertices=verts,faces=faces,process=False)
    assert mesh.is_watertight and mesh.is_winding_consistent and mesh.volume>0
    assert len(faces)==44
    groups=Builder(('M_Stone_Flat',));vs=[];fs=[];uv=[]
    for face in faces:
        pts=verts[face];normal=np.cross(pts[1]-pts[0],pts[2]-pts[0]);axis=int(np.argmax(abs(normal)))
        axes=[i for i in range(3) if i!=axis]
        start=len(vs);vs.extend(map(tuple,pts));fs.append((start,start+1,start+2))
        uv.extend(tuple((p[i]+50)/100 for i in axes) for p in pts)
    groups.groups['M_Stone_Flat'].add(vs,fs,uv,'flat_dressed_stone')
    fbx=SOURCE/'SM_Zhengnanmen_FlatDressedStone.fbx';write_fbx(fbx,groups)
    # Map the packed instance order back to named source cells; no guessing
    # from the sorted HISM index or approximate z coordinates.
    source_nodes={}
    for node in scene['nodes']:
        name=node.get('name','')
        if not name.startswith(('Stone_FB_','Stone_LR_')):continue
        m=node['matrix'];key=tuple(round(v,4) for v in (m[12]*100,-m[13]*100,m[14]*100))
        assert key not in source_nodes;source_nodes[key]=name
    before=json.loads((OUT/'before-stones.json').read_text());assert len(before)==1
    cells=[];removed=[]
    for r in before[0]['instances']:
        key=tuple(round(v,4) for v in r['location']);name=source_nodes[key]
        _,wall,sign,row,col=name.split('_');sign=int(sign);row=int(row);col=int(col)
        z=19+row*33.5;height=32.3;width=41.3
        if wall=='FB':
            lateral=-1258+col*42.5
            # Protect the arch using the course's lowest edge, not its center.
            bottom=z-height/2
            half=215 if bottom<=345 else math.sqrt(max(0,215**2-(bottom-345)**2)) if bottom<560 else 0
            if half:
                lo=lateral-width/2;hi=lateral+width/2
                if lateral<0:hi=min(hi,-half-.6)
                else:lo=max(lo,half+.6)
                if hi<=lo:
                    removed.append(dict(original_index=r['index'],cell=name,reason='Entire cell lies inside the arched opening'))
                    continue
                width=hi-lo;lateral=(lo+hi)/2
            loc=[lateral,-sign*795,z];scale=[width/100,height/100,.10]
        else:
            lateral=-768+col*42.5
            loc=[sign*1285,-lateral,z];scale=[.10,height/100,width/100]
        cells.append(dict(index=r['index'],cell=name,wall=wall,sign=sign,row=row,col=col,location=loc,scale=scale,rotation=r['rotation']))
    assert len(cells)+len(removed)==len(source_nodes)==4154
    # Exactly coplanar outer surfaces at y=±800 and x=±1290.
    for c in cells:
        axis=1 if c['wall']=='FB' else 0
        assert abs(abs(c['location'][axis])+5-(800 if axis==1 else 1290))<1e-8
    data=dict(mesh_source=str(fbx),sha256=hashlib.sha256(fbx.read_bytes()).hexdigest(),source_glb=str(GLB),source_inward_triangles=int(np.sum(dots<0)),prototype_triangles=len(faces),prototype_volume=float(mesh.volume),closed=True,outward_winding=True,major_faces_planar=True,unit_bounds=[[-50]*3,[50]*3],bevel_cm=b,
        component=before[0]['name'],original_mesh=before[0]['mesh'],joint_cm=1.2,course_pitch_cm=33.5,lateral_pitch_cm=42.5,front_back_outer_plane_cm=800,side_outer_plane_cm=1290,cells=cells,removed_cells=removed)
    (SOURCE/'masonry-manifest.json').write_text(json.dumps(data,indent=2))
    print('FLAT_MASONRY_SOURCE_VALIDATED',len(cells),len(faces),'inward source triangles',data['source_inward_triangles'])

if __name__=='__main__':main()
