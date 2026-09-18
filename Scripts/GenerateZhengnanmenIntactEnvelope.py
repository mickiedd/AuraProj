"""Repair inverted source solids and close thin shells for the exact V2 gate.

Keep source vertices/UVs and the existing HISM transforms. Native FBX maps
(x,y,z) to (x,-y,z); emit (source_x,-source_z,source_y) to reproduce the
Interchange prototype basis (source_x,source_z,source_y) used by this actor.
"""
import collections,hashlib,json,struct
from pathlib import Path
import numpy as np
import trimesh
from GenerateGuangzhouLandmarkDeepGeometry import Builder,write_fbx

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
SOURCE=ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_IntactEnvelope_20260916'
GLB=Path('C:/Works/Raw3DModels/V2/GreatSouthGate_Zhengnanmen_UE5_Complete_Package/GreatSouthGate_Zhengnanmen_UE5_HighFidelity.glb')

def geometry(v,f):
    m=trimesh.Trimesh(vertices=v,faces=f,process=False)
    m.merge_vertices(merge_tex=True,merge_norm=True,digits_vertex=6)
    return m

def close_sheet(v,f,uv,delta):
    # Count edges by position across UV seams, but retain original UV indices.
    _,inv=np.unique(np.round(v,6),axis=0,return_inverse=True)
    edges=collections.defaultdict(list)
    for face in f:
        for a,b in zip(face,np.roll(face,-1)):
            key=tuple(sorted((int(inv[a]),int(inv[b]))))
            if key[0]!=key[1]:edges[key].append((int(a),int(b)))
    assert all(len(e)<=2 for e in edges.values()),'Nonmanifold source sheet'
    n=len(v);newf=[tuple(t) for t in f]+[tuple(int(i)+n for i in t[::-1]) for t in f]
    for e in edges.values():
        if len(e)==1:
            a,b=e[0];newf.extend(((b,a,a+n),(b,a+n,b+n)))
    return np.vstack((v,v+delta)),np.array(newf),np.vstack((uv,uv))

def main():
    SOURCE.mkdir(parents=True,exist_ok=True)
    with GLB.open('rb') as file:
        file.read(12);n,t=struct.unpack('<II',file.read(8));scene=json.loads(file.read(n));n,t=struct.unpack('<II',file.read(8));binary=file.read(n)
    def acc(index):
        a=scene['accessors'][index];b=scene['bufferViews'][a['bufferView']];arity={'VEC3':3,'VEC2':2,'SCALAR':1}[a['type']]
        return np.frombuffer(binary,dtype={5126:np.float32,5125:np.uint32,5123:np.uint16}[a['componentType']],count=a['count']*arity,offset=b.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,arity)
    nodes={n.get('name','').replace('.','_'):n for n in scene['nodes']}
    before=json.loads((OUT/'before-instances.json').read_text());entries=[]
    for component in before:
        original=component['mesh'];name=original.rsplit('/',1)[-1].split('.')[0]
        if name not in nodes:continue # Engine cube and the already-flat masonry.
        source_mesh=scene['meshes'][nodes[name]['mesh']];p=source_mesh['primitives'][0]
        v=acc(p['attributes']['POSITION']).astype(float)*100;f=acc(p['indices']).reshape(-1,3).astype(int)
        had_uv='TEXCOORD_0' in p['attributes']
        uv=acc(p['attributes']['TEXCOORD_0']).astype(float) if had_uv else np.zeros((len(v),2))
        old=geometry(v,f);reason=None
        if old.is_watertight and old.volume<0:
            f=f[:,::-1];reason='Reverse inward-wound closed solid'
        elif not old.is_watertight:
            role=source_mesh['name'];delta=np.zeros_like(v)
            if role.startswith('RoofShell_'):delta[:,2]=-3;reason='Close roof underside and all perimeter edges, 3cm thickness'
            elif role.startswith('GEO_RoofTile_'):delta[:,2]=-.6;reason='Close tile underside and end faces, 0.6cm thickness'
            elif role=='TunnelArch':
                radial=v[:,(0,2)]-np.array([0,345]);radial/=np.linalg.norm(radial,axis=1)[:,None]
                delta[:,(0,2)]=radial*3;reason='Close arched soffit radially outward, preserve clear 215cm radius'
            else:
                axis=int(np.argmin(np.ptp(v,axis=0)));assert np.ptp(v,axis=0)[axis]<.001,role
                delta[:,axis]=.3 if role=='SignBoardFace' else 2;reason='Give visual plane a closed backing and edge faces'
            v,f,uv=close_sheet(v,f,uv,delta)
            if geometry(v,f).volume<0:f=f[:,::-1]
        if reason is None:continue
        m=geometry(v,f)
        assert m.is_watertight and m.is_winding_consistent and m.volume>0,(name,'Invalid repair')
        if not had_uv:
            # Source planes/boxes without UV0 need per-face planar coordinates.
            points=[];triangles=[];coords=[];lo=v.min(0);span=np.maximum(np.ptp(v,axis=0),1.)
            for face in f:
                pts=v[face];axis=int(np.argmax(abs(np.cross(pts[1]-pts[0],pts[2]-pts[0]))));axes=[i for i in range(3) if i!=axis]
                start=len(points);points.extend(pts);triangles.append((start,start+1,start+2));coords.extend((p[axes]-lo[axes])/span[axes] for p in pts)
            v=np.array(points);f=np.array(triangles);uv=np.array(coords)
        meshname='SM_Intact_'+name.replace('-','Neg')
        # glTF UVs use the texture's top-left convention. Native FBX import
        # flips V; pre-flip it here to retain the original UE UV0 orientation.
        fbx_uv=uv.copy();fbx_uv[:,1]=1-fbx_uv[:,1]
        b=Builder(('M_Original',));b.groups['M_Original'].add([(x,-z,y) for x,y,z in v],f.tolist(),fbx_uv.tolist(),'repaired_envelope')
        path=SOURCE/(meshname+'.fbx');write_fbx(path,b)
        # Keep geometry for reproducible coverage/ray checks and source QA.
        npz=SOURCE/(meshname+'.npz');np.savez_compressed(npz,vertices_cm=v,faces=f,uv=uv)
        local=v[:,(0,2,1)]
        entries.append(dict(component=component['name'],original_mesh=original,source_role=source_mesh['name'],mesh_name=meshname,fbx=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),geometry=str(npz),reason=reason,original_uv0_present=had_uv,old_triangles=len(old.faces),new_triangles=len(f),old_closed=bool(old.is_watertight),old_volume_cm3=float(old.volume),new_volume_cm3=float(m.volume),closed=True,outward=True,expected_local_bounds=[local.min(0).tolist(),local.max(0).tolist()]))
    assert len(entries)==101,len(entries)
    data=dict(source_glb=str(GLB),preserve_original_vertices_uvs=True,preserve_instance_transforms=True,entries=entries,inverted_solids=sum(e['old_closed'] for e in entries),closed_sheets=sum(not e['old_closed'] for e in entries),roof_shell_count=3)
    (SOURCE/'envelope-manifest.json').write_text(json.dumps(data,indent=2))
    print('INTACT_ENVELOPE_SOURCE_VALIDATED',len(entries),data['inverted_solids'],data['closed_sheets'])

if __name__=='__main__':main()
