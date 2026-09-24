from dataclasses import dataclass, field
import pickle, math
from pathlib import Path
import numpy as np
import trimesh

@dataclass
class Group:
    vertices:list=field(default_factory=list); faces:list=field(default_factory=list); uvs:list=field(default_factory=list); names:list=field(default_factory=list)
class Builder:
    def __init__(self): self.groups={}

with open('/mnt/data/zhengximen_builders.pkl','rb') as f: builders=pickle.load(f)
ROOT=Path('/mnt/data/Zhengximen_GreatWestGate_UE5/Meshes')
mat_colors={'GrayBrick':(125,123,115),'StoneFoundation':(145,142,132),'AgedWood':(103,68,46),'DarkTimber':(68,45,33),'ClayRoofTile':(62,63,61),'LimePlaster':(191,188,176),'BlackIron':(38,38,37),'GatePlaque':(177,158,125)}

def write_array(f, arr, fmt='g', chunk=12000):
    a=np.asarray(arr).reshape(-1)
    for i in range(0,len(a),chunk):
        part=a[i:i+chunk]
        if fmt=='i': s=','.join(map(str,part.astype(np.int64).tolist()))
        else: s=','.join(f'{float(x):.6g}' for x in part)
        if i: f.write(',')
        f.write(s)

def write_one(label,bld):
    path=ROOT/f'SM_Zhengximen_{label}.fbx'
    next_id=100000
    matids={}
    with open(path,'w',encoding='utf-8',buffering=1024*1024) as f:
        f.write('; FBX 7.4.0 project file\nFBXHeaderExtension:  {\n    FBXHeaderVersion: 1003\n    FBXVersion: 7400\n}\n')
        f.write('GlobalSettings:  {\n    Version: 1000\n    Properties70:  {\n        P: "UpAxis", "int", "Integer", "",2\n        P: "UpAxisSign", "int", "Integer", "",1\n        P: "FrontAxis", "int", "Integer", "",1\n        P: "FrontAxisSign", "int", "Integer", "",-1\n        P: "CoordAxis", "int", "Integer", "",0\n        P: "CoordAxisSign", "int", "Integer", "",1\n        P: "UnitScaleFactor", "double", "Number", "",1\n        P: "OriginalUnitScaleFactor", "double", "Number", "",1\n    }\n}\nDefinitions:  { Version: 100 Count: 0 }\nObjects:  {\n')
        for mat,c in mat_colors.items():
            mid=next_id;next_id+=1;matids[mat]=mid; rgb=np.array(c)/255
            f.write(f'    Material: {mid}, "Material::M_{mat}", "" {{\n        Version: 102\n        ShadingModel: "phong"\n        MultiLayer: 0\n        Properties70:  {{ P: "DiffuseColor", "Color", "", "A",{rgb[0]:.6f},{rgb[1]:.6f},{rgb[2]:.6f} }}\n    }}\n')
        parent=next_id;next_id+=1
        f.write(f'    Model: {parent}, "Model::SM_Zhengximen_{label}", "Null" {{ Version: 232 Properties70:  {{ P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0 }} }}\n')
        con=[(parent,0)]
        total_faces=sum(len(g.faces) for g in bld.groups.values() if g.vertices)
        atlas_grid=int(math.ceil(math.sqrt(max(1,total_faces))))
        face_cursor=0
        for mat,g in bld.groups.items():
            if not g.vertices: continue
            gid=next_id;next_id+=1; mid=next_id;next_id+=1
            V=np.asarray(g.vertices,float)*100.0; F=np.asarray(g.faces,np.int64); UV=np.asarray(g.uvs,float)
            tm=trimesh.Trimesh(vertices=V,faces=F,process=False)
            N=tm.vertex_normals
            # polygon vertex index
            pidx=np.empty((len(F),3),dtype=np.int64); pidx[:,:2]=F[:,:2]; pidx[:,2]=-F[:,2]-1
            # Global non-overlapping lightmap UV atlas: one padded triangle chart per cell.
            ids=np.arange(face_cursor,face_cursor+len(F),dtype=np.int64); face_cursor += len(F)
            rows=ids//atlas_grid; cols=ids%atlas_grid; cell=1.0/atlas_grid; pad=0.16*cell
            u0=cols*cell+pad; v0=rows*cell+pad; u1=(cols+1)*cell-pad; v1=(rows+1)*cell-pad
            UV2=np.stack([np.stack([u0,v0],1),np.stack([u1,v0],1),np.stack([u0,v1],1)],1).reshape(-1,2)
            f.write(f'    Geometry: {gid}, "Geometry::SM_Zhengximen_{label}_{mat}", "Mesh" {{\n        Vertices: *{V.size} {{ a: ');write_array(f,V);f.write(' }\n')
            f.write(f'        PolygonVertexIndex: *{pidx.size} {{ a: ');write_array(f,pidx,'i');f.write(' }\n')
            f.write('        LayerElementNormal: 0 { Version: 101 Name: "" MappingInformationType: "ByVertice" ReferenceInformationType: "Direct"\n')
            f.write(f'            Normals: *{N.size} {{ a: ');write_array(f,N);f.write(' }\n        }\n')
            f.write('        LayerElementUV: 0 { Version: 101 Name: "UVChannel_1" MappingInformationType: "ByVertice" ReferenceInformationType: "Direct"\n')
            f.write(f'            UV: *{UV.size} {{ a: ');write_array(f,UV);f.write(' }\n        }\n')
            f.write('        LayerElementUV: 1 { Version: 101 Name: "LightMapUV" MappingInformationType: "ByPolygonVertex" ReferenceInformationType: "Direct"\n')
            f.write(f'            UV: *{UV2.size} {{ a: ');write_array(f,UV2);f.write(' }\n        }\n')
            f.write('        LayerElementMaterial: 0 { Version: 101 Name: "" MappingInformationType: "AllSame" ReferenceInformationType: "IndexToDirect" Materials: *1 { a: 0 } }\n')
            f.write('        Layer: 0 { Version: 100 LayerElement: { Type: "LayerElementNormal" TypedIndex: 0 } LayerElement: { Type: "LayerElementMaterial" TypedIndex: 0 } LayerElement: { Type: "LayerElementUV" TypedIndex: 0 } LayerElement: { Type: "LayerElementUV" TypedIndex: 1 } }\n    }\n')
            f.write(f'    Model: {mid}, "Model::SM_Zhengximen_{label}_{mat}", "Mesh" {{ Version: 232 Properties70:  {{ P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0 }} Shading: T Culling: "CullingOff" }}\n')
            con += [(gid,mid),(mid,parent),(matids[mat],mid)]
        if label=='LOD0':
            # UCX convex box collision proxies embedded in the LOD0 FBX. Units are cm.
            collision_boxes=[
                ((-737.5,0,390),(1025,1000,780)),((737.5,0,390),(1025,1000,780)),
                ((0,0,672),(450,1000,216)),((-1950,100,300),(1400,800,600)),((1950,100,300),(1400,800,600))]
            for ci,(cc,ss) in enumerate(collision_boxes):
                cx,cy,cz=cc; sx,sy,sz=ss; x0,x1=cx-sx/2,cx+sx/2; y0,y1=cy-sy/2,cy+sy/2; z0,z1=cz-sz/2,cz+sz/2
                Vc=np.array([(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)],float)
                Fc=np.array([[0,2,1],[0,3,2],[4,5,6],[4,6,7],[0,1,5],[0,5,4],[1,2,6],[1,6,5],[2,3,7],[2,7,6],[3,0,4],[3,4,7]],np.int64)
                pi=np.empty((len(Fc),3),dtype=np.int64); pi[:,:2]=Fc[:,:2]; pi[:,2]=-Fc[:,2]-1
                gid=next_id; next_id+=1; mid=next_id; next_id+=1; cname=f'UCX_SM_Zhengximen_LOD0_{ci:02d}'
                f.write(f'    Geometry: {gid}, "Geometry::{cname}", "Mesh" {{\n        Vertices: *{Vc.size} {{ a: ');write_array(f,Vc);f.write(' }\n')
                f.write(f'        PolygonVertexIndex: *{pi.size} {{ a: ');write_array(f,pi,'i');f.write(' }\n    }\n')
                f.write(f'    Model: {mid}, "Model::{cname}", "Mesh" {{ Version: 232 Properties70:  {{ P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0 }} Shading: T }}\n')
                con += [(gid,mid),(mid,parent)]
            for n,pos in [('SOCKET_GateCenter',(0,-530,180)),('SOCKET_PlayerEntry',(0,-650,90)),('SOCKET_LeftWall',(-1950,100,350)),('SOCKET_RightWall',(1950,100,350)),('SOCKET_RoofTop',(0,0,1630))]:
                sid=next_id;next_id+=1
                f.write(f'    Model: {sid}, "Model::{n}", "Null" {{ Version: 232 Properties70:  {{ P: "Lcl Translation", "Lcl Translation", "", "A",{pos[0]},{pos[1]},{pos[2]} }} }}\n')
                con.append((sid,0))
        f.write('}\nConnections:  {\n')
        for a,b in con:f.write(f'    C: "OO",{a},{b}\n')
        f.write('}\n')
    print(label, path.stat().st_size)

for i in range(5): write_one(f'LOD{i}',builders[f'LOD{i}'])
# remove obsolete combined fbx if present; individual files are intended for UE LOD import
allp=ROOT/'SM_Zhengximen_AllLODs.fbx'
if allp.exists(): allp.unlink()
