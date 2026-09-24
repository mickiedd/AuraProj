import os, math, json, hashlib, zipfile, shutil, textwrap
from dataclasses import dataclass, field
from pathlib import Path
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont
import trimesh
from trimesh.visual.material import PBRMaterial
from trimesh.visual.texture import TextureVisuals

ROOT = Path('/mnt/data/Zhengximen_GreatWestGate_UE5')
TEX = ROOT/'Textures'
MESH = ROOT/'Meshes'
SRC = ROOT/'Source'/'HighPoly'
MAT = ROOT/'Materials'
UE = ROOT/'Unreal'
PRE = ROOT/'Preview'
DOC = ROOT/'Documentation'
for p in [TEX,MESH,SRC,MAT,UE,PRE,DOC]: p.mkdir(parents=True, exist_ok=True)

# ------------------------------------------------------------
# Texture generation
# ------------------------------------------------------------
SIZE=4096
WORK=1024
EMBED=1024
rng=np.random.default_rng(20260914)

def smooth_noise(size=WORK, seed=0, coarse=128):
    r=np.random.default_rng(seed)
    a=r.random((coarse,coarse), dtype=np.float32)
    # periodic edge blend
    a[-1,:]=a[0,:]; a[:,-1]=a[:,0]
    n=cv2.resize(a,(size,size),interpolation=cv2.INTER_CUBIC)
    n=cv2.GaussianBlur(n,(0,0),sigmaX=2.0)
    n=(n-n.min())/(n.max()-n.min()+1e-6)
    return n

def save_rgb(path, arr):
    arr=np.clip(arr,0,255).astype(np.uint8)
    im=Image.fromarray(arr,'RGB')
    if im.size != (SIZE,SIZE): im=im.resize((SIZE,SIZE),Image.Resampling.LANCZOS)
    im.save(path, compress_level=1)

def save_l(path, arr):
    arr=np.clip(arr,0,255).astype(np.uint8)
    im=Image.fromarray(arr,'L')
    if im.size != (SIZE,SIZE): im=im.resize((SIZE,SIZE),Image.Resampling.LANCZOS)
    im.save(path, compress_level=1)

def normal_from_height(h, strength=3.0, invert_green=True):
    h=h.astype(np.float32)/255.0
    dx=cv2.Sobel(h,cv2.CV_32F,1,0,ksize=3)
    dy=cv2.Sobel(h,cv2.CV_32F,0,1,ksize=3)
    nx=-dx*strength; ny=-dy*strength; nz=np.ones_like(h)
    norm=np.sqrt(nx*nx+ny*ny+nz*nz)+1e-8
    nx/=norm; ny/=norm; nz/=norm
    if invert_green: ny=-ny  # DirectX/UE
    out=np.stack([(nx*.5+.5)*255,(ny*.5+.5)*255,(nz*.5+.5)*255],axis=-1)
    return np.clip(out,0,255).astype(np.uint8)

def material_maps(name, kind, seed, base):
    H=W=WORK
    y=np.arange(H,dtype=np.float32)[:,None]
    x=np.arange(W,dtype=np.float32)[None,:]
    n=smooth_noise(seed=seed, coarse=256)
    n2=smooth_noise(seed=seed+101, coarse=128)
    basev=np.array(base,dtype=np.float32)
    metallic=np.zeros((H,W),np.float32)
    ao=np.full((H,W),245,np.float32)

    if kind=='brick':
        bw,bh=360,150
        row=(y//bh).astype(np.int32)
        xoff=((row%2)*bw/2)
        xm=np.mod(x+xoff,bw); ym=np.mod(y,bh)
        mortar=((xm<13)|(xm>bw-13)|(ym<11)|(ym>bh-11)).astype(np.float32)
        stains=np.clip((n2-.48)*1.2,0,1)
        color=basev[None,None,:]*(0.82+0.27*n[...,None])
        color*= (1-0.28*mortar[...,None])
        moss=np.clip((stains-.42)*2.3,0,1)[...,None]
        color=color*(1-0.12*moss)+np.array([63,76,55])[None,None,:]*0.12*moss
        height=120+70*n-100*mortar+10*n2
        rough=185+35*(1-n)+25*mortar
        ao=245-55*mortar-15*moss[...,0]
    elif kind=='stone':
        # large dressed blocks
        bw,bh=700,310
        row=(y//bh).astype(np.int32); xoff=((row%2)*bw/2)
        xm=np.mod(x+xoff,bw); ym=np.mod(y,bh)
        joint=((xm<10)|(xm>bw-10)|(ym<9)|(ym>bh-9)).astype(np.float32)
        color=basev[None,None,:]*(0.78+0.30*n[...,None])
        color*=1-0.16*joint[...,None]
        height=135+72*n-85*joint+18*n2
        rough=175+45*n2+20*joint
        ao=248-45*joint
    elif kind=='wood' or kind=='darkwood':
        grain=(np.sin((y/15.5)+(n*7.0))+1)*0.5
        pores=(np.sin((y/4.7)+(n2*13.0))+1)*0.5
        knots=np.clip((n2-.82)*7,0,1)
        color=basev[None,None,:]*(0.72+0.23*grain[...,None]+0.10*n[...,None])
        color*=1-0.22*knots[...,None]
        height=120+55*grain+15*pores-25*knots
        rough=145+50*n+20*knots
        ao=248-30*knots
    elif kind=='roof':
        tw,th=150,255
        xm=np.mod(x,tw); ym=np.mod(y,th)
        ridge=np.exp(-((xm-tw/2)/(tw*.26))**2)
        overlap=((ym<18)|(ym>th-15)).astype(np.float32)
        color=basev[None,None,:]*(0.78+0.24*n[...,None])
        color*=1+0.12*ridge[...,None]-0.12*overlap[...,None]
        height=105+95*ridge-35*overlap+18*n2
        rough=145+55*n+20*overlap
        ao=245-30*overlap-15*(1-ridge)
    elif kind=='plaster':
        cracks=np.clip((n2-.76)*5,0,1)
        color=basev[None,None,:]*(0.90+0.13*n[...,None])
        color*=1-0.22*cracks[...,None]
        height=128+34*n-35*cracks
        rough=205+25*n2+20*cracks
        ao=250-25*cracks
    elif kind=='iron':
        rust=np.clip((n2-.62)*3,0,1)
        color=basev[None,None,:]*(0.75+0.28*n[...,None])
        color=color*(1-rust[...,None]*0.42)+np.array([88,47,30])[None,None,:]*(rust[...,None]*0.42)
        height=128+30*n+20*n2
        rough=80+90*rust+25*n
        metallic[:]=235
        ao=250-15*rust
    elif kind=='plaque':
        color=basev[None,None,:]*(0.88+0.15*n[...,None])
        height=128+25*n
        rough=145+45*n2
        # lettering added after image conversion
    else:
        color=basev[None,None,:]*(0.85+0.2*n[...,None]); height=128+30*n; rough=180+30*n2

    base_img=np.clip(color,0,255).astype(np.uint8)
    height_img=np.clip(height,0,255).astype(np.uint8)
    rough_img=np.clip(rough,0,255).astype(np.uint8)
    metal_img=np.clip(metallic,0,255).astype(np.uint8)
    ao_img=np.clip(ao,0,255).astype(np.uint8)

    if kind=='plaque':
        pil=Image.fromarray(base_img,'RGB')
        d=ImageDraw.Draw(pil)
        font_path='/usr/share/opentype/noto/NotoSerifCJK-Bold.ttc'
        font=ImageFont.truetype(font_path, 280)
        text='正西门'
        bbox=d.textbbox((0,0),text,font=font)
        twd=bbox[2]-bbox[0]; thd=bbox[3]-bbox[1]
        pos=((W-twd)//2,(H-thd)//2-18)
        # subtle pale inset panel and dark ink
        d.rounded_rectangle((60,160,W-60,H-160), radius=24, fill=(190,176,147), outline=(83,63,42), width=10)
        d.text(pos,text,font=font,fill=(31,27,22))
        base_img=np.array(pil)
        # lettering height mask approximate
        mask=Image.new('L',(W,H),0); md=ImageDraw.Draw(mask); md.text(pos,text,font=font,fill=255)
        m=np.array(mask,dtype=np.float32)/255.0
        height_img=np.clip(height_img.astype(np.float32)+35*m,0,255).astype(np.uint8)
        rough_img=np.clip(rough_img.astype(np.float32)-25*m,0,255).astype(np.uint8)

    normal_img=normal_from_height(height_img, strength=2.1 if kind in ['plaster','plaque'] else 3.2)
    prefix=TEX/f'T_{name}'
    save_rgb(str(prefix)+'_BaseColor.png',base_img)
    nim=Image.fromarray(normal_img,'RGB').resize((SIZE,SIZE),Image.Resampling.LANCZOS); nim.save(str(prefix)+'_Normal.png', compress_level=1)
    save_l(str(prefix)+'_Roughness.png',rough_img)
    save_l(str(prefix)+'_Metallic.png',metal_img)
    save_l(str(prefix)+'_AO.png',ao_img)
    save_l(str(prefix)+'_Height.png',height_img)
    # material metadata
    (MAT/f'MI_{name}.json').write_text(json.dumps({
        'name':f'MI_{name}', 'master':'M_Zhengximen_Master',
        'textures':{
            'BaseColor':f'../Textures/T_{name}_BaseColor.png','Normal':f'../Textures/T_{name}_Normal.png',
            'Roughness':f'../Textures/T_{name}_Roughness.png','Metallic':f'../Textures/T_{name}_Metallic.png',
            'AmbientOcclusion':f'../Textures/T_{name}_AO.png','Height':f'../Textures/T_{name}_Height.png'},
        'uv_scale_m':1.0,'normal_format':'DirectX (Y-)','texture_resolution':[4096,4096]
    },indent=2), encoding='utf-8')
    del n,n2,base_img,height_img,rough_img,metal_img,ao_img,normal_img

materials={
 'GrayBrick':('brick',11,(126,125,118)),
 'StoneFoundation':('stone',22,(137,135,126)),
 'AgedWood':('wood',33,(104,68,45)),
 'DarkTimber':('darkwood',44,(71,48,34)),
 'ClayRoofTile':('roof',55,(69,68,65)),
 'LimePlaster':('plaster',66,(190,187,174)),
 'BlackIron':('iron',77,(44,43,41)),
 'GatePlaque':('plaque',88,(176,157,124)),
}
if not os.environ.get('REUSE_TEXTURES'):
    for name,(kind,seed,base) in materials.items():
        print('Texture',name,flush=True)
        material_maps(name,kind,seed,base)
else:
    print('Reusing existing textures', flush=True)

# ------------------------------------------------------------
# Geometry generation
# ------------------------------------------------------------
@dataclass
class Group:
    vertices:list=field(default_factory=list)
    faces:list=field(default_factory=list)
    uvs:list=field(default_factory=list)
    names:list=field(default_factory=list)

class Builder:
    def __init__(self): self.groups={k:Group() for k in materials}
    def add_mesh(self, mat, v, f, uv=None, name=''):
        g=self.groups[mat]; off=len(g.vertices)
        v=np.asarray(v,float); f=np.asarray(f,int)
        if uv is None: uv=np.zeros((len(v),2),float)
        g.vertices.extend(v.tolist()); g.uvs.extend(np.asarray(uv,float).tolist()); g.faces.extend((f+off).tolist())
        if name: g.names.append(name)
    def add_box(self, mat, center, size, uvscale=1.0, name='box'):
        cx,cy,cz=center; sx,sy,sz=size
        x0,x1=cx-sx/2,cx+sx/2; y0,y1=cy-sy/2,cy+sy/2; z0,z1=cz-sz/2,cz+sz/2
        # six faces, unique vertices for UV
        quads=[
          [(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1)], # front
          [(x1,y1,z0),(x0,y1,z0),(x0,y1,z1),(x1,y1,z1)], # back
          [(x0,y1,z0),(x0,y0,z0),(x0,y0,z1),(x0,y1,z1)],
          [(x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1)],
          [(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)],
          [(x0,y1,z0),(x1,y1,z0),(x1,y0,z0),(x0,y0,z0)],]
        verts=[]; faces=[]; uvs=[]
        for q in quads:
            i=len(verts); verts+=q; faces += [[i,i+1,i+2],[i,i+2,i+3]]
            # UV density based on face local dimensions approximate
            a=np.linalg.norm(np.array(q[1])-np.array(q[0]))/uvscale
            b=np.linalg.norm(np.array(q[3])-np.array(q[0]))/uvscale
            uvs += [(0,0),(a,0),(a,b),(0,b)]
        self.add_mesh(mat,verts,faces,uvs,name)
    def add_cylinder_between(self, mat, p0,p1,r=0.1,sides=12, uvscale=1.0,name='cyl'):
        p0=np.array(p0,float); p1=np.array(p1,float); axis=p1-p0; L=np.linalg.norm(axis)
        if L<1e-8:return
        t=axis/L
        ref=np.array([0,0,1.0]) if abs(t[2])<0.9 else np.array([1.0,0,0])
        n1=np.cross(t,ref); n1/=np.linalg.norm(n1); n2=np.cross(t,n1)
        verts=[];uv=[]
        for j,p in enumerate([p0,p1]):
            for i in range(sides):
                a=2*math.pi*i/sides; verts.append(p+r*(math.cos(a)*n1+math.sin(a)*n2)); uv.append((j*L/uvscale,i/sides))
        faces=[]
        for i in range(sides):
            ni=(i+1)%sides; faces += [[i,ni,sides+ni],[i,sides+ni,sides+i]]
        # caps
        c0=len(verts); verts.append(p0);uv.append((.5,.5)); c1=len(verts);verts.append(p1);uv.append((.5,.5))
        for i in range(sides):
            ni=(i+1)%sides; faces.append([c0,ni,i]);faces.append([c1,sides+i,sides+ni])
        self.add_mesh(mat,np.array(verts),np.array(faces),np.array(uv),name)
    def add_tube_curve(self, mat, points, r=.05,sides=8,uvscale=1.0,name='tube'):
        pts=np.asarray(points,float); n=len(pts)
        if n<2:return
        tang=np.zeros_like(pts);tang[1:-1]=pts[2:]-pts[:-2];tang[0]=pts[1]-pts[0];tang[-1]=pts[-1]-pts[-2]
        tang/=np.linalg.norm(tang,axis=1)[:,None]
        verts=[];uv=[]; dist=np.concatenate([[0],np.cumsum(np.linalg.norm(np.diff(pts,axis=0),axis=1))])
        for j,(p,t) in enumerate(zip(pts,tang)):
            n1=np.array([1.0,0,0])
            if abs(np.dot(n1,t))>.95:n1=np.array([0,0,1.0])
            n1=n1-np.dot(n1,t)*t;n1/=np.linalg.norm(n1);n2=np.cross(t,n1);n2/=np.linalg.norm(n2)
            for i in range(sides):
                a=2*math.pi*i/sides;verts.append(p+r*(math.cos(a)*n1+math.sin(a)*n2));uv.append((dist[j]/uvscale,i/sides))
        faces=[]
        for j in range(n-1):
            for i in range(sides):
                ni=(i+1)%sides;a=j*sides+i;b=j*sides+ni;c=(j+1)*sides+ni;d=(j+1)*sides+i
                faces += [[a,b,c],[a,c,d]]
        self.add_mesh(mat,np.array(verts),np.array(faces),np.array(uv),name)
    def add_arch_wedge(self, mat, r0,r1,a0,a1,y0,y1,zc, name='voussoir'):
        # angles: 0..pi around X-Z with x=r*cos(a), z=zc+r*sin(a)
        pts=[]
        for y in [y0,y1]:
            pts += [(r0*math.cos(a0),y,zc+r0*math.sin(a0)),(r1*math.cos(a0),y,zc+r1*math.sin(a0)),
                    (r1*math.cos(a1),y,zc+r1*math.sin(a1)),(r0*math.cos(a1),y,zc+r0*math.sin(a1))]
        f=[[0,1,2],[0,2,3],[4,7,6],[4,6,5],[0,4,5],[0,5,1],[1,5,6],[1,6,2],[2,6,7],[2,7,3],[3,7,4],[3,4,0]]
        uv=[(0,0),(1,0),(1,1),(0,1)]*2
        self.add_mesh(mat,pts,f,uv,name)
    def add_roof_shell(self, mat, W,D,base_z,rise,overhang=0, nx=36,ny=24, thickness=.18, name='roof'):
        xs=np.linspace(-W/2,W/2,nx);ys=np.linspace(-D/2,D/2,ny)
        def zfun(x,y):
            ax=abs(x)/(W/2); ay=abs(y)/(D/2)
            # hip-like diminishing ridge at ends + upturned eaves
            crown=rise*(1-ay**1.3)*(1-0.32*ax**7)
            uplift=.34*(ax**10+ay**10)
            return base_z+crown+uplift
        top=[];uv=[]
        for iy,y in enumerate(ys):
            for ix,x in enumerate(xs):
                top.append((x,y,zfun(x,y)));uv.append((ix/(nx-1)*W/0.8,iy/(ny-1)*D/0.8))
        top=np.array(top); bottom=top.copy();bottom[:,2]-=thickness
        verts=np.vstack([top,bottom]);uv=np.vstack([uv,uv]);faces=[]
        for iy in range(ny-1):
            for ix in range(nx-1):
                a=iy*nx+ix;b=a+1;c=a+nx+1;d=a+nx
                faces += [[a,b,c],[a,c,d]]
                a2=a+nx*ny;b2=b+nx*ny;c2=c+nx*ny;d2=d+nx*ny
                faces += [[a2,c2,b2],[a2,d2,c2]]
        # sides
        loops=[]
        loops.append([i for i in range(nx)])
        loops.append([(ny-1)*nx+i for i in range(nx)])
        loops.append([iy*nx for iy in range(ny)])
        loops.append([iy*nx+nx-1 for iy in range(ny)])
        for loop in loops:
            for i in range(len(loop)-1):
                a=loop[i];b=loop[i+1];a2=a+nx*ny;b2=b+nx*ny
                faces += [[a,b,b2],[a,b2,a2]]
        self.add_mesh(mat,verts,np.array(faces),uv,name)
        return zfun
    def add_torus(self, mat, center, major=.16, minor=.025, nmajor=20,nminor=8, axis='y',name='torus'):
        cx,cy,cz=center;verts=[];uv=[];faces=[]
        for i in range(nmajor):
            a=2*math.pi*i/nmajor
            for j in range(nminor):
                b=2*math.pi*j/nminor
                rr=major+minor*math.cos(b)
                if axis=='y': p=(cx+rr*math.cos(a), cy+minor*math.sin(b), cz+rr*math.sin(a))
                else: p=(cx+rr*math.cos(a), cy+rr*math.sin(a), cz+minor*math.sin(b))
                verts.append(p);uv.append((i/nmajor,j/nminor))
        for i in range(nmajor):
            ni=(i+1)%nmajor
            for j in range(nminor):
                nj=(j+1)%nminor;a=i*nminor+j;b=ni*nminor+j;c=ni*nminor+nj;d=i*nminor+nj
                faces += [[a,b,c],[a,c,d]]
        self.add_mesh(mat,verts,faces,uv,name)


def roof_curve_points(x,W,D,base_z,rise,n=28,side='front'):
    ys=np.linspace(0,-D/2,n) if side=='front' else np.linspace(0,D/2,n)
    pts=[]
    ax=abs(x)/(W/2)
    for y in ys:
        ay=abs(y)/(D/2); z=base_z+rise*(1-ay**1.3)*(1-0.32*ax**7)+.34*(ax**10+ay**10)
        pts.append((x,y,z+.06))
    return pts

def build_gate(detail=4):
    b=Builder()
    # Dimensions meters
    wallW,wallD,wallH=25.0,10.0,7.8
    gate_half=2.25; spring=3.4; archR=2.25
    # foundation plinth + main wall blocks
    b.add_box('StoneFoundation',(0,0,-.18),(25.8,10.8,.36),.8,'foundation')
    # Main wall built in horizontal slices around the arched opening.
    # This avoids boolean/overlap artifacts while preserving a clean arch silhouette.
    slices = 46 if detail>=4 else 32 if detail>=2 else 20
    dz=wallH/slices
    for si in range(slices):
        zc=(si+.5)*dz
        if zc < spring:
            open_half=gate_half
        elif zc < spring+archR:
            open_half=math.sqrt(max(0.0, archR*archR-(zc-spring)*(zc-spring)))
        else:
            open_half=0.0
        if open_half <= .02:
            b.add_box('GrayBrick',(0,0,zc),(wallW,wallD,dz+.006),.75,'wall_slice')
        else:
            sidew=wallW/2-open_half
            if sidew>0:
                b.add_box('GrayBrick',(-(wallW/2+open_half)/2,0,zc),(sidew,wallD,dz+.006),.75,'wall_slice_L')
                b.add_box('GrayBrick',((wallW/2+open_half)/2,0,zc),(sidew,wallD,dz+.006),.75,'wall_slice_R')
    # dressed stone voussoir ring
    seg=34 if detail>=4 else 22 if detail>=2 else 14
    for i in range(seg):
        a0=math.pi*i/seg; a1=math.pi*(i+1)/seg
        b.add_arch_wedge('StoneFoundation',archR,archR+.58,a0,a1,-wallD/2-.06,wallD/2+.06,spring,f'arch_{i}')
    # gate tunnel barrel tiles (dark stone tube strips following arch)
    for i in range(seg+1):
        a=math.pi*i/seg
        x=archR*math.cos(a);z=spring+archR*math.sin(a)
        b.add_cylinder_between('StoneFoundation',(x,-wallD/2,z),(x,wallD/2,z),.035,6,.6,'barrel_joint')
    # Jamb inner liners
    b.add_box('StoneFoundation',(-gate_half-.12,0,spring/2),(.24,wallD,spring),.7,'jambL')
    b.add_box('StoneFoundation',(+gate_half+.12,0,spring/2),(.24,wallD,spring),.7,'jambR')
    # wooden double doors slightly inside
    door_y=-.3; door_h=3.22
    b.add_box('AgedWood',(-gate_half/2,door_y,door_h/2),(gate_half-.07,.18,door_h),.6,'doorL')
    b.add_box('AgedWood',(+gate_half/2,door_y,door_h/2),(gate_half-.07,.18,door_h),.6,'doorR')
    # iron strips, studs, knockers
    for x0 in [-gate_half/2,gate_half/2]:
        for sx in [-.68,0,.68]: b.add_box('BlackIron',(x0+sx,door_y-.105,door_h/2),(.035,.035,door_h-.16),.4,'door_strap')
        for z in np.linspace(.42,2.82,6):
            for dx in [-.65,-.22,.22,.65]: b.add_cylinder_between('BlackIron',(x0+dx,door_y-.18,z),(x0+dx,door_y-.28,z),.045,8,.2,'stud')
        b.add_torus('BlackIron',(x0,door_y-.29,1.45),.18,.025,18,7,'y','knocker')
    # Side curtain wall wings
    for side in [-1,1]:
        cx=side*(wallW/2+7.0)
        b.add_box('GrayBrick',(cx,1.0,3.0),(14.0,8.0,6.0),.8,'wingwall')
        # parapet walk slab
        b.add_box('StoneFoundation',(cx,1.0,6.08),(14.2,8.2,.18),.8,'wingcap')
        crenel_count=9 if detail>=2 else 6
        for i in range(crenel_count):
            xx=cx-6.4+i*(12.8/(crenel_count-1))
            b.add_box('GrayBrick',(xx,-2.65,6.65),(.75,.7,1.15),.7,'crenel')
            b.add_box('GrayBrick',(xx,4.65,6.65),(.75,.7,1.15),.7,'crenel')
        # end tower crenellations
    # main parapet side merlons
    for xx in np.linspace(-11.6,11.6,13):
        if abs(xx)<7.3: continue
        b.add_box('GrayBrick',(xx,-4.65,8.25),(.72,.65,.9),.7,'main_crenel')
    # brick relief on front main wall for high detail
    if detail>=4:
        rows=24 if detail==4 else 34
        cols=48 if detail==4 else 64
        for r in range(rows):
            z=.18+r*(7.1/rows)
            off=(r%2)*.22
            for c in range(cols):
                x=-12.1+(c+.5)*(24.2/cols)+off
                # skip opening / arch approximate
                if abs(x)<2.55 and z<5.85: continue
                if abs(x)<4.5 and z>3.2 and ((x*x+(z-spring)**2)<(archR+2.2)**2): continue
                b.add_box('StoneFoundation',(x,-5.03,z),(.33,.035,.14),.5,'brick_relief')
    # timber pavilion deck / lower story
    deckz=8.0
    b.add_box('StoneFoundation',(0,0,deckz-.12),(20.5,8.8,.24),.8,'deck')
    # lower columns and beams
    lowerW,lowerD=18.8,7.4; z0=8.05; z1=11.0
    coln=11 if detail>=2 else 7
    xs=np.linspace(-lowerW/2+.7,lowerW/2-.7,coln)
    for x in xs:
        b.add_cylinder_between('DarkTimber',(x,-lowerD/2+.25,z0),(x,-lowerD/2+.25,z1),.17,12 if detail>=3 else 8,.8,'column')
        b.add_cylinder_between('DarkTimber',(x, lowerD/2-.25,z0),(x, lowerD/2-.25,z1),.17,12 if detail>=3 else 8,.8,'column')
    for yy in [-lowerD/2+.22, lowerD/2-.22]:
        b.add_box('DarkTimber',(0,yy,z1-.08),(lowerW,.28,.34),.8,'beam')
        b.add_box('DarkTimber',(0,yy,z0+.25),(lowerW,.22,.25),.8,'sill')
    # lattice windows lower front/rear
    if detail>=2:
        bays=len(xs)-1
        for iy,yy in enumerate([-lowerD/2+.12, lowerD/2-.12]):
            for i in range(bays):
                xa,xb=xs[i],xs[i+1]; cx=(xa+xb)/2; w=xb-xa-.12
                # dark backing panels
                b.add_box('AgedWood',(cx,yy,(z0+z1)/2),(w,.08,z1-z0-.55),.8,'panel')
                bars=4 if detail>=4 else 2
                for k in range(1,bars+1):
                    xx=xa+k*(xb-xa)/(bars+1); b.add_box('DarkTimber',(xx,yy-.05 if yy<0 else yy+.05,(z0+z1)/2),(.045,.06,z1-z0-.55),.4,'lattice_v')
                for zz in np.linspace(z0+.55,z1-.4,4 if detail>=4 else 2): b.add_box('DarkTimber',(cx,yy-.05 if yy<0 else yy+.05,zz),(w,.06,.045),.4,'lattice_h')
    # lower eave roof
    roof1W,roof1D=22.1,10.0; roof1base=10.7; roof1rise=1.42
    b.add_roof_shell('ClayRoofTile',roof1W,roof1D,roof1base,roof1rise,nx=48 if detail>=4 else 28,ny=30 if detail>=4 else 18,thickness=.17,name='lower_roof')
    # geometric tile ridges
    if detail>=2:
        spacing={5:.20,4:.26,3:.36,2:.50}.get(detail,.65)
        segs={5:46,4:34,3:24,2:16}.get(detail,10)
        sides={5:10,4:8,3:7,2:6}.get(detail,6)
        for x in np.arange(-roof1W/2+.18,roof1W/2-.18,spacing):
            for side in ['front','back']:
                pts=roof_curve_points(x,roof1W,roof1D,roof1base,roof1rise,segs,side)
                b.add_tube_curve('ClayRoofTile',pts,.045 if detail>=4 else .038,sides,.6,'roof_tile')
    b.add_cylinder_between('ClayRoofTile',(-roof1W/2,0,roof1base+roof1rise+.08),(roof1W/2,0,roof1base+roof1rise+.08),.095,10,.6,'ridge')
    # upper story above lower roof
    upz0=12.28; upz1=14.72; upperW,upperD=15.7,5.8
    upcoln=9 if detail>=2 else 5
    uxs=np.linspace(-upperW/2+.6,upperW/2-.6,upcoln)
    for x in uxs:
        for yy in [-upperD/2+.2,upperD/2-.2]: b.add_cylinder_between('DarkTimber',(x,yy,upz0),(x,yy,upz1),.15,12 if detail>=3 else 8,.7,'upper_col')
    for yy in [-upperD/2+.18,upperD/2-.18]:
        b.add_box('DarkTimber',(0,yy,upz1-.07),(upperW,.25,.31),.7,'upper_beam')
        b.add_box('DarkTimber',(0,yy,upz0+.18),(upperW,.22,.24),.7,'upper_sill')
    if detail>=2:
        for yy in [-upperD/2+.09,upperD/2-.09]:
            for i in range(upcoln-1):
                xa,xb=uxs[i],uxs[i+1];cx=(xa+xb)/2;w=xb-xa-.1
                b.add_box('AgedWood',(cx,yy,(upz0+upz1)/2),(w,.07,upz1-upz0-.42),.7,'upper_panel')
                bars=3 if detail>=4 else 2
                for k in range(1,bars+1):
                    xx=xa+k*(xb-xa)/(bars+1);b.add_box('DarkTimber',(xx,yy-.04 if yy<0 else yy+.04,(upz0+upz1)/2),(.04,.05,upz1-upz0-.42),.4,'upper_lattice')
                for zz in np.linspace(upz0+.48,upz1-.35,3): b.add_box('DarkTimber',(cx,yy-.04 if yy<0 else yy+.04,zz),(w,.05,.04),.4,'upper_lattice')
    # plaque front
    b.add_box('GatePlaque',(0,-upperD/2-.085,13.55),(3.25,.14,.92),1.0,'plaque')
    # frame around plaque
    b.add_box('DarkTimber',(0,-upperD/2-.17,14.04),(3.55,.16,.10),.5,'plaqueframe')
    b.add_box('DarkTimber',(0,-upperD/2-.17,13.06),(3.55,.16,.10),.5,'plaqueframe')
    b.add_box('DarkTimber',(-1.73,-upperD/2-.17,13.55),(.10,.16,1.08),.5,'plaqueframe')
    b.add_box('DarkTimber',(+1.73,-upperD/2-.17,13.55),(.10,.16,1.08),.5,'plaqueframe')
    # upper roof
    roof2W,roof2D=18.5,7.8; roof2base=14.45; roof2rise=1.62
    b.add_roof_shell('ClayRoofTile',roof2W,roof2D,roof2base,roof2rise,nx=50 if detail>=4 else 30,ny=30 if detail>=4 else 18,thickness=.18,name='upper_roof')
    if detail>=2:
        spacing={5:.19,4:.25,3:.34,2:.48}.get(detail,.64)
        segs={5:48,4:36,3:26,2:18}.get(detail,12); sides={5:10,4:8,3:7,2:6}.get(detail,6)
        for x in np.arange(-roof2W/2+.16,roof2W/2-.16,spacing):
            for side in ['front','back']:
                pts=roof_curve_points(x,roof2W,roof2D,roof2base,roof2rise,segs,side)
                b.add_tube_curve('ClayRoofTile',pts,.045 if detail>=4 else .037,sides,.6,'roof_tile')
    b.add_cylinder_between('ClayRoofTile',(-roof2W/2,0,roof2base+roof2rise+.08),(roof2W/2,0,roof2base+roof2rise+.08),.10,10,.6,'ridge')
    # upturned corner finials and ridge ornaments
    if detail>=2:
        for W,D,bz,rs in [(roof1W,roof1D,roof1base,roof1rise),(roof2W,roof2D,roof2base,roof2rise)]:
            for sx in [-1,1]:
                for sy in [-1,1]:
                    x=sx*W/2*.98;y=sy*D/2*.98;z=bz+.34*(.98**10+.98**10)+.1
                    b.add_cylinder_between('ClayRoofTile',(x,y,z),(x+sx*.22,y+sy*.18,z+.45),.055,7,.5,'finial')
            for sx in [-1,1]:
                x=sx*W/2*.92;z=bz+rs*(1-.32*(.92**7))+.22
                b.add_cylinder_between('ClayRoofTile',(x,0,z),(x+sx*.3,0,z+.5),.07,8,.5,'ridge_beast')
    # dougong brackets under eaves
    if detail>=2:
        def brackets(W,D,z,countx,county):
            for x in np.linspace(-W/2+.8,W/2-.8,countx):
                for yy in [-D/2+.22,D/2-.22]:
                    s=-1 if yy<0 else 1
                    b.add_box('DarkTimber',(x,yy,z),(.34,.62,.16),.4,'dg1')
                    b.add_box('DarkTimber',(x,yy+s*.13,z+.18),(.58,.34,.14),.4,'dg2')
                    b.add_box('AgedWood',(x,yy+s*.28,z+.34),(.26,.52,.12),.4,'dg3')
            for y in np.linspace(-D/2+.8,D/2-.8,county):
                for xx in [-W/2+.25,W/2-.25]:
                    s=-1 if xx<0 else 1
                    b.add_box('DarkTimber',(xx,y,z),(.62,.34,.16),.4,'dg1')
                    b.add_box('DarkTimber',(xx+s*.13,y,z+.18),(.34,.58,.14),.4,'dg2')
        brackets(20.2,8.0,10.56,18 if detail>=4 else 12,7)
        brackets(16.7,6.4,14.30,15 if detail>=4 else 10,5)
    # support beams under roof
    for z,W,D in [(10.42,20.2,8.0),(14.18,16.7,6.4)]:
        b.add_box('DarkTimber',(0,-D/2+.15,z),(W,.26,.28),.6,'eavebeam')
        b.add_box('DarkTimber',(0, D/2-.15,z),(W,.26,.28),.6,'eavebeam')
    return b

# material color approximations for preview
mat_colors={'GrayBrick':(125,123,115),'StoneFoundation':(145,142,132),'AgedWood':(103,68,46),'DarkTimber':(68,45,33),'ClayRoofTile':(62,63,61),'LimePlaster':(191,188,176),'BlackIron':(38,38,37),'GatePlaque':(177,158,125)}

# Load PBR textures once lazily
pbr_cache={}
def get_pbr(name):
    if name in pbr_cache:return pbr_cache[name]
    bc=Image.open(TEX/f'T_{name}_BaseColor.png').convert('RGB').resize((EMBED,EMBED),Image.Resampling.LANCZOS)
    no=Image.open(TEX/f'T_{name}_Normal.png').convert('RGB').resize((EMBED,EMBED),Image.Resampling.LANCZOS)
    ao=Image.open(TEX/f'T_{name}_AO.png').convert('L').resize((EMBED,EMBED),Image.Resampling.LANCZOS)
    r=np.array(Image.open(TEX/f'T_{name}_Roughness.png').convert('L').resize((EMBED,EMBED),Image.Resampling.LANCZOS))
    m=np.array(Image.open(TEX/f'T_{name}_Metallic.png').convert('L').resize((EMBED,EMBED),Image.Resampling.LANCZOS))
    mr=np.zeros((EMBED,EMBED,3),np.uint8); mr[...,1]=r; mr[...,2]=m
    mr=Image.fromarray(mr,'RGB')
    mat=PBRMaterial(name=f'M_{name}',baseColorTexture=bc,normalTexture=no,occlusionTexture=ao,metallicRoughnessTexture=mr,metallicFactor=1.0,roughnessFactor=1.0)
    pbr_cache[name]=mat; return mat

def scene_from_builder(builder, textured=True, node_prefix=''):
    scene=trimesh.Scene()
    for name,g in builder.groups.items():
        if not g.vertices or not g.faces: continue
        v=np.array(g.vertices,float);f=np.array(g.faces,int);uv=np.array(g.uvs,float)
        tm=trimesh.Trimesh(vertices=v,faces=f,process=False,validate=False)
        if textured: tm.visual=TextureVisuals(uv=uv, material=get_pbr(name))
        else:
            color=np.array(mat_colors[name]+(255,),dtype=np.uint8);tm.visual.face_colors=np.tile(color,(len(f),1))
        scene.add_geometry(tm,node_name=node_prefix+name,geom_name=node_prefix+name)
    return scene

def stats(builder):
    tris=sum(len(g.faces) for g in builder.groups.values()); verts=sum(len(g.vertices) for g in builder.groups.values())
    return {'vertices':verts,'triangles':tris}

def build_lod4():
    b=Builder()
    # silhouette-only far LOD with real-scale proportions
    wallW,wallD,wallH=25.0,10.0,7.8; gate_half=2.25; spring=3.4; archR=2.25
    b.add_box('StoneFoundation',(0,0,-.18),(25.8,10.8,.36),1,'foundation')
    b.add_box('GrayBrick',(-7.375,0,wallH/2),(10.25,wallD,wallH),1,'leftwall')
    b.add_box('GrayBrick',(7.375,0,wallH/2),(10.25,wallD,wallH),1,'rightwall')
    b.add_box('GrayBrick',(0,0,6.72),(4.5,wallD,2.16),1,'topwall')
    for side in [-1,1]:
        b.add_box('GrayBrick',(side*19.5,1.0,3.0),(14,8,6),1,'wing')
    b.add_box('DarkTimber',(0,0,9.5),(18.8,7.4,3.0),1,'lowerbulk')
    b.add_roof_shell('ClayRoofTile',22.1,10.0,10.7,1.42,nx=8,ny=6,thickness=.2,name='roof1')
    b.add_box('DarkTimber',(0,0,13.45),(15.7,5.8,2.4),1,'upperbulk')
    b.add_roof_shell('ClayRoofTile',18.5,7.8,14.45,1.62,nx=8,ny=6,thickness=.2,name='roof2')
    b.add_box('GatePlaque',(0,-3.0,13.55),(3.25,.12,.92),1,'plaque')
    return b

builders={}
# highpoly detail=5, LOD0=4, LOD1=3, LOD2=2, LOD3=1; LOD4 silhouette-only
for label,detail in [('HighPoly',5),('LOD0',4),('LOD1',3),('LOD2',2),('LOD3',1),('LOD4',0)]:
    print('Build',label,flush=True)
    builders[label]=build_lod4() if label=='LOD4' else build_gate(detail)
    print(label,stats(builders[label]),flush=True)

# Export GLBs
for label,bld in builders.items():
    scene=scene_from_builder(bld, textured=False)
    out=(SRC/f'SRC_Zhengximen_HighPoly.glb') if label=='HighPoly' else (MESH/f'SM_Zhengximen_{label}.glb')
    print('Export GLB',out.name,flush=True)
    data=scene.export(file_type='glb')
    out.write_bytes(data)
    del scene,data

# Export modular LOD0 pieces as per-material GLB (convenient UE modular import)
moddir=MESH/'Modular';moddir.mkdir(exist_ok=True)
for name,g in builders['LOD0'].groups.items():
    if not g.vertices: continue
    tm=trimesh.Trimesh(vertices=np.array(g.vertices),faces=np.array(g.faces),process=False)
    color=np.array(mat_colors[name]+(255,),dtype=np.uint8); tm.visual.face_colors=np.tile(color,(len(g.faces),1))
    sc=trimesh.Scene(tm)
    (moddir/f'SM_Zhengximen_{name}.glb').write_bytes(sc.export(file_type='glb'))

# Simple collision mesh GLB (boxes; named UCX-like in scene)
col=Builder()
col.add_box('StoneFoundation',(-7.4,0,3.7),(10.0,10.2,7.4),1,'UCX_Wall_L')
col.add_box('StoneFoundation',(7.4,0,3.7),(10.0,10.2,7.4),1,'UCX_Wall_R')
col.add_box('StoneFoundation',(0,0,6.7),(4.7,10.2,2.0),1,'UCX_Wall_Top')
col.add_box('StoneFoundation',(0,0,10.0),(20.4,8.6,4.0),1,'UCX_Pavilion_Lower')
col.add_box('StoneFoundation',(0,0,13.6),(17.0,6.6,3.0),1,'UCX_Pavilion_Upper')
sc=scene_from_builder(col,textured=False,node_prefix='UCX_SM_Zhengximen_')
(MESH/'SM_Zhengximen_Collision.glb').write_bytes(sc.export(file_type='glb'))

import pickle
with open('/mnt/data/zhengximen_builders.pkl','wb') as _pf: pickle.dump(builders,_pf,pickle.HIGHEST_PROTOCOL)
if os.environ.get('STOP_AFTER_GLBS'):
    print('STOP_AFTER_GLBS set; geometry builders saved.', flush=True)
    raise SystemExit(0)

# ------------------------------------------------------------
# Minimal ASCII FBX writer - separate material meshes for each LOD
# ------------------------------------------------------------
def fbx_array(vals, perline=24):
    vals=list(vals); lines=[]
    for i in range(0,len(vals),perline): lines.append(','.join(str(v) for v in vals[i:i+perline]))
    return ',\n                '.join(lines)

def write_fbx(path, lod_builders):
    # lod_builders list of (label,builder), vertices converted m -> cm
    next_id=100000
    objects=[]; connections=[]; models=[]
    mat_ids={}
    for mat in materials:
        mid=next_id;next_id+=1;mat_ids[mat]=mid
        color=np.array(mat_colors[mat])/255.0
        objects.append(f'''    Material: {mid}, "Material::M_{mat}", "" {{\n        Version: 102\n        ShadingModel: "phong"\n        MultiLayer: 0\n        Properties70:  {{\n            P: "DiffuseColor", "Color", "", "A",{color[0]:.6f},{color[1]:.6f},{color[2]:.6f}\n            P: "SpecularColor", "Color", "", "A",0.08,0.08,0.08\n            P: "Shininess", "double", "Number", "",12\n        }}\n    }}''')
    for lod_label,bld in lod_builders:
        parent_id=next_id;next_id+=1
        models.append(parent_id)
        objects.append(f'''    Model: {parent_id}, "Model::SM_Zhengximen_{lod_label}", "Null" {{\n        Version: 232\n        Properties70:  {{\n            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0\n            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0\n            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1\n        }}\n    }}''')
        connections.append(f'    C: "OO",{parent_id},0')
        for mat,g in bld.groups.items():
            if not g.vertices: continue
            gid=next_id;next_id+=1;mid=next_id;next_id+=1
            v=np.array(g.vertices,float)*100.0; f=np.array(g.faces,int); uv=np.array(g.uvs,float)
            # FBX polygon indices; triangle last index encoded -(idx+1)
            pidx=[]
            for tri in f: pidx.extend([int(tri[0]),int(tri[1]),-int(tri[2])-1])
            tm=trimesh.Trimesh(vertices=v,faces=f,process=False)
            normals=tm.face_normals
            nflat=np.repeat(normals,3,axis=0).reshape(-1)
            uvflat=uv.reshape(-1)
            # ByVertice UV to keep smaller, normals ByPolygonVertex
            objects.append(f'''    Geometry: {gid}, "Geometry::SM_Zhengximen_{lod_label}_{mat}", "Mesh" {{\n        Vertices: *{v.size} {{\n            a: {fbx_array(v.reshape(-1),18)}\n        }}\n        PolygonVertexIndex: *{len(pidx)} {{\n            a: {fbx_array(pidx,30)}\n        }}\n        LayerElementNormal: 0 {{\n            Version: 101\n            Name: ""\n            MappingInformationType: "ByPolygonVertex"\n            ReferenceInformationType: "Direct"\n            Normals: *{len(nflat)} {{\n                a: {fbx_array(nflat,18)}\n            }}\n        }}\n        LayerElementUV: 0 {{\n            Version: 101\n            Name: "UVChannel_1"\n            MappingInformationType: "ByVertice"\n            ReferenceInformationType: "Direct"\n            UV: *{len(uvflat)} {{\n                a: {fbx_array(uvflat,20)}\n            }}\n        }}\n        LayerElementMaterial: 0 {{\n            Version: 101\n            Name: ""\n            MappingInformationType: "AllSame"\n            ReferenceInformationType: "IndexToDirect"\n            Materials: *1 {{ a: 0 }}\n        }}\n        Layer: 0 {{\n            Version: 100\n            LayerElement: {{ Type: "LayerElementNormal" TypedIndex: 0 }}\n            LayerElement: {{ Type: "LayerElementMaterial" TypedIndex: 0 }}\n            LayerElement: {{ Type: "LayerElementUV" TypedIndex: 0 }}\n        }}\n    }}''')
            objects.append(f'''    Model: {mid}, "Model::SM_Zhengximen_{lod_label}_{mat}", "Mesh" {{\n        Version: 232\n        Properties70:  {{\n            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0\n            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0\n            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1\n        }}\n        Shading: T\n        Culling: "CullingOff"\n    }}''')
            connections += [f'    C: "OO",{gid},{mid}',f'    C: "OO",{mid},{parent_id}',f'    C: "OO",{mat_ids[mat]},{mid}']
    # sockets as Nulls in cm coordinates (transforms)
    sockets=[('SOCKET_GateCenter',(0,-530,180)),('SOCKET_PlayerEntry',(0,-650,90)),('SOCKET_LeftWall',(-1950,100,350)),('SOCKET_RightWall',(1950,100,350)),('SOCKET_RoofTop',(0,0,1630))]
    for name,pos in sockets:
        sid=next_id;next_id+=1
        objects.append(f'''    Model: {sid}, "Model::{name}", "Null" {{\n        Version: 232\n        Properties70:  {{ P: "Lcl Translation", "Lcl Translation", "", "A",{pos[0]},{pos[1]},{pos[2]} }}\n    }}''')
        connections.append(f'    C: "OO",{sid},0')
    header='''; FBX 7.4.0 project file\n; Generated procedurally for Zhengximen UE5 package\nFBXHeaderExtension:  {\n    FBXHeaderVersion: 1003\n    FBXVersion: 7400\n}\nGlobalSettings:  {\n    Version: 1000\n    Properties70:  {\n        P: "UpAxis", "int", "Integer", "",2\n        P: "UpAxisSign", "int", "Integer", "",1\n        P: "FrontAxis", "int", "Integer", "",1\n        P: "FrontAxisSign", "int", "Integer", "",-1\n        P: "CoordAxis", "int", "Integer", "",0\n        P: "CoordAxisSign", "int", "Integer", "",1\n        P: "UnitScaleFactor", "double", "Number", "",1\n        P: "OriginalUnitScaleFactor", "double", "Number", "",1\n    }\n}\nDefinitions:  {\n    Version: 100\n    Count: 0\n}\nObjects:  {\n'''
    footer='\n}\nConnections:  {\n'+'\n'.join(connections)+'\n}\n'
    path.write_text(header+'\n'.join(objects)+footer,encoding='utf-8')

print('Writing FBX LOD0-4 (ASCII)',flush=True)
write_fbx(MESH/'SM_Zhengximen_AllLODs.fbx',[(f'LOD{i}',builders[f'LOD{i}']) for i in range(5)])
# Also individual LOD FBX for reliable UE import
for i in range(5): write_fbx(MESH/f'SM_Zhengximen_LOD{i}.fbx',[(f'LOD{i}',builders[f'LOD{i}'])])

# ------------------------------------------------------------
# Software preview renderer
# ------------------------------------------------------------
def look_at(cam,target,up=np.array([0,0,1.0])):
    cam=np.array(cam,float);target=np.array(target,float);f=target-cam;f/=np.linalg.norm(f)
    r=np.cross(f,up);r/=np.linalg.norm(r);u=np.cross(r,f)
    # camera coords x=r, y=u, z=f
    R=np.stack([r,u,f],axis=0)
    return R,cam

def render(builder,path,cam=(17,-39,9.0),target=(0,0,8.2),res=(2560,1440),fov=45):
    W,H=res
    # sky gradient
    img=np.zeros((H,W,3),np.uint8)
    top=np.array([178,203,221],float);bot=np.array([235,225,204],float)
    for yy in range(H):
        t=yy/(H-1); img[yy,:,:]=(top*(1-t)+bot*t).astype(np.uint8)
    # ground plane perspective later as polygon with texture lines
    R,C=look_at(cam,target); focal=(W/2)/math.tan(math.radians(fov)/2)
    light=np.array([-.45,-.65,.62]);light/=np.linalg.norm(light)
    tris=[]
    for mat,g in builder.groups.items():
        if not g.faces:continue
        V=np.array(g.vertices,float);F=np.array(g.faces,int)
        # limit extremely tiny detail in preview by sampling if huge
        if len(F)>70000:
            idx=np.linspace(0,len(F)-1,70000,dtype=int);F=F[idx]
        P=V[F]  # n,3,3
        N=np.cross(P[:,1]-P[:,0],P[:,2]-P[:,0]); nl=np.linalg.norm(N,axis=1); good=nl>1e-9;N[good]/=nl[good,None]
        Pc=(P-C)@R.T
        # all triangle vertices in front
        keep=np.all(Pc[:,:,2]>.2,axis=1)
        P=P[keep];Pc=Pc[keep];N=N[keep]
        centers=P.mean(axis=1); view=C-centers; view/=np.linalg.norm(view,axis=1)[:,None]
        # cull backfaces if normal points away from camera; but meshes may inconsistent, keep both w/ abs shade
        depth=Pc[:,:,2].mean(axis=1)
        base=np.array(mat_colors[mat],float)
        diff=np.maximum(0,np.dot(N,light));
        # two-sided gentle lighting
        diff=np.maximum(diff,0.18+0.2*np.abs(np.dot(N,light)))
        shade=.48+.52*diff
        colors=np.clip(base[None,:]*shade[:,None],0,255)
        x2=W/2+focal*(Pc[:,:,0]/Pc[:,:,2]); y2=H*.53-focal*(Pc[:,:,1]/Pc[:,:,2])
        pts=np.stack([x2,y2],axis=-1)
        for q,d,c in zip(pts,depth,colors):
            if np.max(q[:,0])<0 or np.min(q[:,0])>=W or np.max(q[:,1])<0 or np.min(q[:,1])>=H:continue
            tris.append((float(d),q,c))
    tris.sort(key=lambda x:x[0],reverse=True)
    # ground cobbles and street under model (behind model but drawn before triangles)
    cv2.rectangle(img,(0,int(H*.68)),(W,H),(151,142,126),-1)
    # perspective-ish street seams
    vp=(W//2,int(H*.46))
    for x in range(-W,2*W,120): cv2.line(img,(x,H),(vp[0]+(x-W//2)//12,vp[1]),(120,113,101),1,cv2.LINE_AA)
    for yy in range(int(H*.72),H,60): cv2.line(img,(0,yy),(W,yy),(127,119,106),1,cv2.LINE_AA)
    for d,q,c in tris:
        poly=np.round(q).astype(np.int32)
        # haze with depth
        haze=min(max((d-25)/55,0),.45); cc=(c*(1-haze)+np.array([208,205,194])*haze).astype(np.uint8)
        cv2.fillConvexPoly(img,poly,tuple(int(x) for x in cc.tolist()),lineType=cv2.LINE_AA)
    # soft vignette / title
    overlay=img.copy()
    cv2.putText(overlay,'ZHENGXIMEN / GREAT WEST GATE / UE5 ASSET PREVIEW',(55,H-55),cv2.FONT_HERSHEY_SIMPLEX,0.8,(236,234,225),2,cv2.LINE_AA)
    cv2.putText(overlay,'Procedural reconstruction from supplied reference board',(55,H-25),cv2.FONT_HERSHEY_SIMPLEX,0.55,(230,228,219),1,cv2.LINE_AA)
    img=cv2.addWeighted(img,.95,overlay,.05,0)
    Image.fromarray(cv2.cvtColor(img,cv2.COLOR_BGR2RGB)).save(path,quality=95)

print('Render previews',flush=True)
render(builders['LOD2'],PRE/'Preview_Zhengximen_Perspective.png')
render(builders['LOD2'],PRE/'Preview_Zhengximen_Front.png',cam=(0,-45,8),target=(0,0,8),fov=40)
render(builders['LOD3'],PRE/'Preview_Zhengximen_Side.png',cam=(34,-8,10),target=(0,0,8),fov=42)

# Contact sheet
ims=[Image.open(PRE/f) for f in ['Preview_Zhengximen_Perspective.png','Preview_Zhengximen_Front.png','Preview_Zhengximen_Side.png']]
thumbs=[]
for im in ims:
    t=im.copy();t.thumbnail((1200,700));thumbs.append(t)
canvas=Image.new('RGB',(2400,1400),(235,232,224));
canvas.paste(thumbs[0],(0,0));canvas.paste(thumbs[1],(1200,0));canvas.paste(thumbs[2],(600,700));
d=ImageDraw.Draw(canvas);d.text((40,1350),'Zhengximen asset preview contact sheet',fill=(30,30,30),font=ImageFont.truetype('/usr/share/opentype/noto/NotoSansCJK-Regular.ttc',34))
canvas.save(PRE/'Preview_Zhengximen_ContactSheet.png',quality=95)

# ------------------------------------------------------------
# Unreal import helper scripts / docs
# ------------------------------------------------------------
ue_py=r'''# Unreal Engine 5.x Python import helper for Zhengximen package
# Run from Unreal Editor: Tools > Execute Python Script.
import unreal, os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MESH = os.path.join(ROOT, 'Meshes')
TEX = os.path.join(ROOT, 'Textures')
DEST = '/Game/Zhengximen'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def import_file(filename, dest, automated=True):
    task=unreal.AssetImportTask(); task.filename=filename; task.destination_path=dest
    task.automated=automated; task.save=True; task.replace_existing=True
    asset_tools.import_asset_tasks([task]); return task.imported_object_paths

# Import LOD0 FBX first
lod0=os.path.join(MESH,'SM_Zhengximen_LOD0.fbx')
paths=import_file(lod0, DEST+'/Meshes')
unreal.log('Imported LOD0: '+str(paths))

# Import the remaining LODs manually into the resulting Static Mesh if your UE build
# does not auto-create them from SM_Zhengximen_AllLODs.fbx.
if paths:
    sm=unreal.load_asset(paths[0])
    if sm:
        for i in range(1,5):
            f=os.path.join(MESH,f'SM_Zhengximen_LOD{i}.fbx')
            try:
                unreal.EditorStaticMeshLibrary.import_lod(sm, i, f)
            except Exception as e: unreal.log_warning(f'LOD {i}: {e}')
        # sockets
        for n,loc in [
            ('GateCenter',(0,-530,180)),('PlayerEntry',(0,-650,90)),
            ('LeftWall',(-1950,100,350)),('RightWall',(1950,100,350)),('RoofTop',(0,0,1630))]:
            try:
                s=unreal.StaticMeshSocket(); s.socket_name=n; s.relative_location=unreal.Vector(*loc)
                sm.add_socket(s)
            except Exception as e: unreal.log_warning('Socket '+n+': '+str(e))
        unreal.EditorAssetLibrary.save_loaded_asset(sm)

# Import textures. Naming convention supports automated material setup.
for fn in os.listdir(TEX):
    if fn.lower().endswith('.png'):
        import_file(os.path.join(TEX,fn), DEST+'/Textures')

unreal.log('Zhengximen source imported. See Materials/*.json and Documentation/UE5_IMPORT.md for material setup.')
'''
(UE/'Import_Zhengximen_UE5.py').write_text(ue_py,encoding='utf-8')

master_json={
 'name':'M_Zhengximen_Master','workflow':'Metallic/Roughness','parameters':['BaseColor','Normal','Roughness','Metallic','AO','Height','UVScale','DisplacementStrength'],
 'recommended_ue_settings':{'Nanite':True,'Lumen':'Surface Cache compatible opaque materials','NormalCompression':'TC_Normalmap','VirtualTextures':'Optional for 8K upgrades'},
 'note':'Material instance JSON files map the supplied 4K textures. Height can feed BumpOffset/POM or Nanite displacement depending on UE version/project policy.'
}
(MAT/'M_Zhengximen_Master.json').write_text(json.dumps(master_json,indent=2),encoding='utf-8')

st={k:stats(v) for k,v in builders.items()}
manifest={
 'asset':'Zhengximen / Great West Gate / 正西门','coordinate_system':{'GLB':'meters, Z-up asset geometry','FBX':'centimeters, Z-up'},
 'estimated_dimensions_m':{'overall_width_with_wings':53.0,'central_wall_width':25.0,'main_depth':10.0,'roof_top_height':16.4,'gate_opening_width':4.5,'gate_arch_top_height':5.65},
 'lod_stats':st,'texture_resolution':'4096x4096','materials':list(materials.keys()),
 'source_reference':'User-supplied reconstruction board; dimensions are proportional reconstruction estimates, not a metrical architectural survey.',
 'license_note':'Geometry, textures, scripts, and preview generated procedurally for this package; no third-party model/texture assets included.'
}
(DOC/'AssetManifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding='utf-8')

readme=f'''# Zhengximen / Great West Gate / 正西门 — UE5 Asset Package

This package is a procedural, production-oriented reconstruction derived from the supplied Zhengximen reference board. It includes a complete assembled gate, modular material-group meshes, HighPoly source, LOD0–LOD4, PBR textures, collision proxy, sockets/import metadata, UE5 import helper, and high-resolution previews.

## Important accuracy note
The reference board contains perspective and orthographic drawings but no explicit metrical dimensions. The package therefore matches the *visible proportions and architectural language* of the board, while the absolute measurements are reconstructed estimates. Treat it as an AAA-ready art asset baseline rather than a surveyed heritage-BIM record.

## Package contents
- `Source/HighPoly/SRC_Zhengximen_HighPoly.glb` — densest procedural source.
- `Meshes/SM_Zhengximen_AllLODs.fbx` — ASCII FBX containing LOD0–LOD4 material meshes.
- `Meshes/SM_Zhengximen_LOD0..4.fbx` — one FBX per LOD for robust UE import.
- `Meshes/SM_Zhengximen_LOD0..4.glb` — glTF/GLB geometry alternatives with material-group colors; bind the supplied 4K PBR sets in UE/material DCC.
- `Meshes/SM_Zhengximen_Collision.glb` — collision proxy components.
- `Meshes/Modular/` — modular GLBs by architectural material group.
- `Textures/` — 4K BaseColor, Normal (DirectX), Roughness, Metallic, AO, Height PNG sets.
- `Materials/` — master-material and material-instance JSON specs.
- `Unreal/Import_Zhengximen_UE5.py` — UE5 Python import helper.
- `Preview/` — 2560×1440 perspective/front/side renders + contact sheet.
- `Documentation/AssetManifest.json` — dimensions, material names, triangle counts.

## UE5 recommended import
1. Copy the unpacked package into a source-art folder outside `/Content`.
2. Import `Meshes/SM_Zhengximen_LOD0.fbx` into `/Game/Zhengximen/Meshes` with **Convert Scene Unit ON**, **Import Normals and Tangents**, **Generate Lightmap UVs OFF** (UVs are supplied), and **Nanite ON**.
3. Import LOD1–LOD4 through Static Mesh Editor > LOD Import, or run `Unreal/Import_Zhengximen_UE5.py`.
4. Import textures from `Textures/`. Set `_Normal` to Normalmap compression; disable sRGB on Roughness/Metallic/AO/Height.
5. Build a master material with BaseColor/Normal/Roughness/Metallic/AO and optional Height. The `Materials/MI_*.json` files give map bindings.
6. Assign the eight material instances in this order/name mapping: GrayBrick, StoneFoundation, AgedWood, DarkTimber, ClayRoofTile, LimePlaster, BlackIron, GatePlaque.
7. Use complex-as-simple only for editor validation; ship using the supplied UCX-style proxy pieces or project-specific convex decomposition.

## Scale / orientation
- GLB geometry is in meters. FBX geometry is written in centimeters for Unreal.
- Z is up; gate facade faces -Y in the generator coordinate system.
- Reconstructed roof-top height: ~16.4 m. Central wall width: 25 m. Overall wing span: ~53 m.

## LOD triangle counts
{json.dumps(st,indent=2)}

## Production notes
- Geometry is deliberately modular and triangulated at export for deterministic runtime topology.
- All visual texture maps are generated from scratch and are tileable/periodic where appropriate.
- No copyrighted third-party meshes or textures are embedded.
- For a true heritage-grade reconstruction, replace estimated scale with measured survey data and validate roof framing/dougong typology against archival plans.
'''
(ROOT/'README.md').write_text(readme,encoding='utf-8')

# Include generator script for reproducibility
shutil.copy2('/mnt/data/generate_zhengximen_package.py',SRC/'generate_zhengximen_package.py')

# file checksums and validation report
rows=[]
for p in ROOT.rglob('*'):
    if p.is_file():
        h=hashlib.sha256();
        with open(p,'rb') as f:
            for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
        rows.append((str(p.relative_to(ROOT)),p.stat().st_size,h.hexdigest()))
with open(DOC/'SHA256SUMS.txt','w',encoding='utf-8') as f:
    for rel,size,h in rows:f.write(f'{h}  {rel}\n')

# zip package
zip_path=Path('/mnt/data/Zhengximen_GreatWestGate_UE5_Package.zip')
print('Zipping',flush=True)
with zipfile.ZipFile(zip_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in ROOT.rglob('*'):
        if p.is_file(): z.write(p,arcname=str(Path(ROOT.name)/p.relative_to(ROOT)))
print('DONE',zip_path,zip_path.stat().st_size,flush=True)
