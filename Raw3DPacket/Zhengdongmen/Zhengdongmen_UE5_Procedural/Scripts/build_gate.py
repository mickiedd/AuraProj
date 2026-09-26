"""Original procedural reconstruction STUDY, not a surveyed historical mesh.
Run: python Scripts/build_gate.py [--preview-only] ; coordinates in metres, Z up, front = -Y.
Uses numpy pillow trimesh scipy. Mesh/texture generation is deterministic.
"""
from pathlib import Path
import os, math, random, json, zipfile, io, sys
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageFilter
from scipy.ndimage import gaussian_filter, sobel
import trimesh
from trimesh.visual.material import PBRMaterial
from trimesh.visual.texture import TextureVisuals
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Meshes'; TEX=ROOT/'Textures'
for d in (OUT,TEX,ROOT/'Collision',ROOT/'Preview'):d.mkdir(parents=True,exist_ok=True)
random.seed(1965)
# UV per planar face: coordinates are metres divided by repeating tile size.
MATERIALS=['Stone','Wood','RoofTile','Plaster','Iron','DoorWood','Sign']
BASECOL={'Stone':(114,110,101),'Wood':(86,63,45),'RoofTile':(75,74,70),'Plaster':(170,159,141),'Iron':(57,58,58),'DoorWood':(90,60,41),'Sign':(57,32,24)}

def textures():
    rng=np.random.default_rng(1888)
    n=768; xx,yy=np.meshgrid(np.arange(n),np.arange(n));
    material_preview={}
    for name in MATERIALS:
        if name=='Sign':
            c=np.zeros((n,n,3),np.uint8); c[:]=BASECOL[name]
            img=Image.fromarray(c); draw=ImageDraw.Draw(img)
            f='/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
            try: font=ImageFont.truetype(f,168,index=2)
            except: font=ImageFont.truetype(f,162)
            draw.rectangle((18,18,n-18,n-18),outline=(173,125,59),width=13)
            draw.text((n/2,n/2),'正东门',font=font,fill=(223,185,101),anchor='mm',stroke_width=1)
            col=np.asarray(img).copy(); h=np.full((n,n),0.48,np.float32)
        else:
            fnoise=gaussian_filter(rng.normal(size=(n,n)).astype('float32'),2.4)
            fnoise/=max(fnoise.std(),1e-5)
            detail=gaussian_filter(rng.normal(size=(n,n)).astype('float32'),0.8)
            detail/=max(detail.std(),1e-5)
            h=(0.47+0.045*fnoise+0.012*detail).clip(0,1)
            if name=='Stone':
                course=64; length=128
                mortar=(yy%course<4) | (((xx+(yy//course%2)*64)%length)<5)
                h[mortar]-=0.13
                speck=(rng.random((n,n))<0.018); h[speck]-=0.04
            elif name=='RoofTile':
                u=xx%70; v=yy%95
                raised=np.cos(2*np.pi*u/70)*0.065
                seams=(u<3)|(v<5)
                h+=raised; h[seams]-=0.13
            elif name in ('Wood','DoorWood'):
                h+=(np.sin(xx*0.23+gaussian_filter(rng.normal(size=(n,n)).astype('float32'),15)*0.4)*0.042)
                if name=='DoorWood':h[((xx%152)<5)]-=0.12
            elif name=='Plaster':
                h+=gaussian_filter(rng.normal(size=(n,n)).astype('float32'),8)*0.08
            elif name=='Iron':h+=0.009*np.sin(xx*0.24)
            h=np.clip(h,0,1)
            variation=np.stack([fnoise*6+detail*2, fnoise*6+detail*2, fnoise*6+detail*2],axis=-1)
            col=np.clip(np.array(BASECOL[name])[None,None,:]+variation+((h-0.48)*70)[...,None],0,255).astype(np.uint8)
            if name=='Stone':
                pale=rng.random((n//64+1,n//128+1)); bright=np.repeat(np.repeat(pale,64,axis=0),128,axis=1)[:n,:n]
                col=np.clip(col.astype('float32')+(bright[...,None]-.5)*26,0,255).astype(np.uint8)
                col[mortar]=np.array([72,71,67],np.uint8)
        dx=sobel(h,axis=1)/8; dy=sobel(h,axis=0)/8
        nn=np.stack([-dx*4,-dy*4,np.ones_like(h)],axis=-1)
        nn/=np.maximum(np.linalg.norm(nn,axis=-1,keepdims=True),1e-6)
        normal=np.clip((nn*.5+.5)*255,0,255).astype('uint8')
        rough_value={'Stone':225,'Wood':201,'RoofTile':217,'Plaster':240,'Iron':162,'DoorWood':194,'Sign':153}[name]
        rough=np.clip(rough_value+(h-.5)*30,1,255).astype('uint8')
        metal=np.full((n,n),225 if name=='Iron' else 0,'uint8')
        if name=='Sign': metal=np.full((n,n),6,'uint8')
        ao=np.clip(218+(h-.5)*58,0,255).astype('uint8')
        channels={'BaseColor':Image.fromarray(col,'RGB'),'Normal':Image.fromarray(normal,'RGB'),
                  'Roughness':Image.fromarray(rough,'L'),'Metallic':Image.fromarray(metal,'L'),
                  'AO':Image.fromarray(ao,'L'),'Height':Image.fromarray((h*255).astype('uint8'),'L')}
        # 4096x4096 output; underlying procedural structure is 768px, not hand-authored scan detail.
        for k,v in channels.items():
            dest=TEX/f'T_ZDM_{name}_{k}_4K.png'
            if not dest.exists():v.resize((4096,4096),Image.Resampling.BICUBIC).save(dest,compress_level=1)
        material_preview[name]=Image.open(TEX/f'T_ZDM_{name}_BaseColor_4K.png').resize((512,512))
    return material_preview

class Builder:
    def __init__(self):self.a={m:{'v':[],'f':[],'uv':[]} for m in MATERIALS};self.parts={}
    def poly(self,m,verts,uv=None,part=''): # triangles as explicit triangles, each with unique UVs
        a=self.a[m]; base=len(a['v']); a['v'].extend(verts)
        if uv is None:uv=[(p[0]/1.6,p[2]/1.6) for p in verts]
        a['uv'].extend(uv)
        for i in range(1,len(verts)-1):a['f'].append((base,base+i,base+i+1))
        if part:self.parts[part]=self.parts.get(part,0)+max(0,len(verts)-2)
    def box(self,m,x0,x1,y0,y1,z0,z1,part='',s=1.3):
        if x1<=x0 or y1<=y0 or z1<=z0:return
        v=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0), (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
        for idx in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            pts=[v[i] for i in idx]
            edges=np.array(pts[1])-pts[0]; edges2=np.array(pts[3])-pts[0]
            ulen=np.linalg.norm(edges)/s;vlen=np.linalg.norm(edges2)/s
            self.poly(m,pts,[(0,0),(ulen,0),(ulen,vlen),(0,vlen)],part)
    def beam(self,m,start,end,width,part='Beams'):
        a=np.array(start,float);b=np.array(end,float); d=b-a; d/=np.linalg.norm(d)
        q=np.array((0,0,1),float) if abs(d[2])<.9 else np.array((0,1,0),float)
        side=np.cross(d,q); side/=np.linalg.norm(side)
        up=np.cross(d,side);up/=np.linalg.norm(up)
        v=[]
        for cen in (a,b):
            v.extend([tuple(cen+sign1*side*width/2+sign2*up*width/2) for sign1,sign2 in [(-1,-1),(1,-1),(1,1),(-1,1)]])
        for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            self.poly(m,[v[i] for i in ids],part=part)
    def arch(self,r0,r1,cy,zbase,y0,y1,m='Stone',seg=40,part='Arch_voussoirs'):
        for k in range(seg):
            a=math.pi*k/seg;b=math.pi*(k+1)/seg
            def p(r,t,y):return (r*math.cos(t),y,zbase+r*math.sin(t))
            q=[p(r0,a,y0),p(r1,a,y0),p(r1,b,y0),p(r0,b,y0)]
            self.poly(m,q,part=part)
            self.poly(m,[p(r0,b,y1),p(r1,b,y1),p(r1,a,y1),p(r0,a,y1)],part=part)
            self.poly(m,[p(r0,b,y0),p(r0,a,y0),p(r0,a,y1),p(r0,b,y1)],part=part)
            self.poly(m,[p(r1,a,y0),p(r1,b,y0),p(r1,b,y1),p(r1,a,y1)],part=part)
            if k%4==0:
                self.beam('Stone',p(r1+.018,a,y0-.035),p(r1+.018,b,y0-.035),.065,part)
    def mesh(self,name,m,base_color):
        a=self.a[m];
        if not a['f']:return None
        mat=PBRMaterial(name=f'M_ZDM_{m}',baseColorTexture=base_color,metallicFactor=(0.82 if m=='Iron' else 0.0),roughnessFactor=({'Iron':.67,'Sign':.66}.get(m,.94)),doubleSided=True)
        v=np.array(a['v'],dtype=np.float32); f=np.array(a['f'],dtype=np.int64)
        me=trimesh.Trimesh(vertices=v,faces=f,process=False,validate=False)
        me.visual=TextureVisuals(uv=np.array(a['uv'],dtype=np.float32),material=mat)
        return me

def build(level):
    # level=0 is the most elaborate procedural high mesh, 4 the coarsest.
    skip=2**level;b=Builder();rng=random.Random(1888)
    def bx(m,*args,part='',s=1.3):b.box(m,*args,part=part,s=s)
    W=28.;Y=7.4; wall=9.45; R=2.66; CZ=3.4;OR=3.22
    # The gate has an actual, unobstructed centre arch, front to rear.
    for x0,x1 in [(-14,-3.24),(3.24,14)]:
        bx('Stone',x0,x1,-Y,Y,0,wall,part='Foundation_and_wall')
    bx('Stone',-3.24,3.24,-Y,Y,CZ+OR-.07,wall,part='Arch_spandrel')
    for side in (-1,1):
        a,bx1=(-3.24,-R) if side==-1 else (R,3.24)
        bx('Stone',a,bx1,-Y,Y,0,CZ+OR,part='Arch_piers')
        bx('Stone',a,bx1,-Y-.10,-Y+.04,.2,4.8,part='Arch_piers')
    b.arch(R,OR,0,CZ,-Y-.08,Y+.04,seg=max(10,48//skip))
    # arch inner lining, framing bands, keystone and revealed stone voussoirs
    if level<3:
        for depth in [-Y+.24,-Y+1.2,Y-1.2,Y-.24]:
            b.arch(R-.07,R+.035,0,CZ,depth-.035,depth+.035,seg=28//skip+8,part='Tunnel_stone_rib')
    bx('Stone',-14.35,14.35,-7.9,7.9,-.2,.36,part='Foundation_plinth')
    # Side abutments / side stair-ramp wall behind historical facade silhouette
    for sign in (-1,1):
        xx=sign*13
        bx('Stone',xx-.45,xx+.45,-7.8,7.8,9.45,10.0,part='Battlement_base')
    # masonry courses and block relief on exterior walls, avoiding the arch opening.
    if level<=2:
        stride=1 if level==0 else (2 if level==1 else 4)
        for front in [-Y-.022,Y+.022]:
            outward=front<0
            y0,y1=(front-.065,front+.035) if outward else (front-.035,front+.065)
            for ri in range(1,26,stride):
                z=ri*.37; offset=(ri%2)*.38
                for xi in range(-18,19,stride):
                    x=(xi+offset)*.76
                    if x<-13.8 or x>13.8:continue
                    if abs(x)<3.32 and z<CZ+OR+.1:continue
                    if rng.random() < .22:continue
                    bx('Stone',x-.31*stride,x+.31*stride,y0,y1,z,z+.06,part='Masonry_relief')
    # merlons across perimeter; parapet looks like photo, crenellations project above the wall.
    for front in [-7.2,7.2]:
        for i in range(-14,15,skip):
            x=i*.96; bx('Stone',x-.33*min(skip,2),x+.33*min(skip,2),front-.29,front+.29,9.45,10.40,part='Crenellations')
    for x in [-13.65,13.65]:
        for i in range(-7,8,skip):
            y=i*.94;bx('Stone',x-.28,x+.28,y-.32*min(skip,2),y+.32*min(skip,2),9.45,10.35,part='Crenellations')
    # Main pavilion lower level: columns, horizontal beams, lime rendered infill, fenestration and railings.
    bx('Wood',-12.15,12.15,-5.72,5.72,9.40,9.67,part='Lower_floor_frame')
    bx('Plaster',-11.7,11.7,-4.82,4.82,9.67,11.13,part='Lower_infill')
    for front in [-5.8,5.8]:
        for x in np.arange(-11.2,11.3,2.25*skip):
            bx('Wood',x-.13,x+.13,front-.16,front+.16,9.5,12.11,part='Lower_columns')
            if level<3:
                for p in (x-.93,x+.93):bx('Wood',p-.042,p+.042,front-.18,front+.18,10.03,11.35,part='Lower_window_mullions')
        bx('Wood',-12.1,12.1,front-.18,front+.18,11.68,11.84,part='Lower_eave_beam')
        bx('Wood',-12.1,12.1,front-.22,front+.22,10.18,10.31,part='Lower_balcony_rail')
        if level<=1:
            for x in np.arange(-11.8,11.9,.37*skip):
                bx('Wood',x-.035,x+.035,front-.19,front+.19,9.72,10.28,part='Lower_balusters')
    for x in [-11.95,11.95]:
        for y in np.arange(-5.4,5.5,2.7*skip):
            bx('Wood',x-.12,x+.12,y-.1,y+.1,9.6,12.1,part='Lower_side_columns')
    # Recessed, dark framed window panels face outward, leaving the column rhythm visible.
    for ysign in (-1,1):
        yf=ysign*5.72
        for x in np.arange(-10.10,10.11,2.25*skip):
            bx('DoorWood',x-.74,x+.74,yf-.055,yf+.055,10.34,11.42,part='Lower_window_dark_recesses')
            if level<=1:
                for dx in (-.53,0,.53):
                    bx('Wood',x+dx-.033,x+dx+.033,yf-.095,yf+.095,10.33,11.43,part='Lower_window_lattice')
    # Lower roof as four sloping architectural surfaces framing raised upper floor.
    def roof_ring(xin,yin,xout,yout,zinner,zouter,part,step,tiles=True):
        # south/north slopes
        for sig in [-1,1]:
            b.poly('RoofTile',[(-xout,sig*yout,zouter),(xout,sig*yout,zouter),(xin,sig*yin,zinner),(-xin,sig*yin,zinner)],
                  [(0,0),(xout*2/1.2,0),(xin*2/1.2,(yout-yin)/1.2),(0,(yout-yin)/1.2)],part)
            if tiles:
                for xx in np.arange(-xout+.22,xout,.41*step):
                    for yy in np.arange(yin+.22,yout,.38*step):
                        t=(yy-yin)/(yout-yin);z=zinner*(1-t)+zouter*t
                        y=sig*yy
                        # true 3D individual overlapping curved roof tiles (arched cross section)
                        b.beam('RoofTile',(xx,y-sig*.16,z+.038),(xx,y+sig*.16,z+.038),.20,part='Curved_tile_courses')
        for sig in [-1,1]:
            b.poly('RoofTile',[(sig*xout,-yout,zouter),(sig*xout,yout,zouter),(sig*xin,yin,zinner),(sig*xin,-yin,zinner)],part=part)
        for y in [-yout,yout]:b.beam('Wood',(-xout,y,zouter-.15),(xout,y,zouter-.15),.18,'Eave_fascia')
        for x in [-xout,xout]:b.beam('Wood',(x,-yout,zouter-.13),(x,yout,zouter-.13),.18,'Eave_fascia')
        for x in [-xout,xout]:
            for y in [-yout,yout]:
                b.beam('RoofTile',(x,y,zouter),(x+np.sign(x)*.36,y+np.sign(y)*.38,zouter+.30),.20,'Upturned_eave_corners')
    roof_ring(9.8,4.2,13.02,6.56,13.45,12.04,'Lower_hip_roof',skip,level<=2)
    # Raised upper pavilion is recessed from the lower wall and roof; glazing is dark.
    bx('Wood',-9.87,9.87,-4.16,4.16,13.40,13.67,part='Upper_floor_frame')
    bx('Plaster',-9.33,9.33,-3.75,3.75,13.6,16.43,part='Upper_infill')
    for y in [-4.14,4.14]:
        for x in np.arange(-9.25,9.26,1.84*skip):
            bx('Wood',x-.13,x+.13,y-.17,y+.17,13.57,17.12,part='Upper_columns')
            if level<=2:
                for p in [x-.64,x+.64]:bx('Wood',p-.045,p+.045,y-.185,y+.185,14.35,16.30,part='Upper_window_mullions')
        bx('Wood',-9.8,9.8,y-.22,y+.22,14.38,14.51,part='Upper_balcony_rail')
        bx('Wood',-9.8,9.8,y-.2,y+.2,16.86,17.04,part='Upper_eave_beam')
        if level<=1:
            for x in np.arange(-9.60,9.61,.37*skip):
                bx('Wood',x-.04,x+.04,y-.19,y+.19,13.78,14.42,part='Upper_balusters')
    for x in [-9.74,9.74]:
        for y in np.arange(-3.8,3.81,1.9*skip):
            bx('Wood',x-.15,x+.15,y-.13,y+.13,13.52,17.12,part='Upper_side_columns')
    for ysign in (-1,1):
        yf=ysign*4.14
        for x in np.arange(-8.38,8.39,1.84*skip):
            bx('DoorWood',x-.62,x+.62,yf-.075,yf+.075,14.58,16.72,part='Upper_window_dark_recesses')
            if level<=1:
                for dx in (-.42,0,.42):
                    bx('Wood',x+dx-.032,x+dx+.032,yf-.11,yf+.11,14.58,16.72,part='Upper_window_lattice')
    # Upper hip roof with ridge and curved tile texture/geometry.
    for sig in [-1,1]:
        b.poly('RoofTile',[(-8.45,sig*.5,18.94),(8.45,sig*.5,18.94),(10.9,sig*5.42,17.10),(-10.9,sig*5.42,17.10)],part='Upper_hip_roof')
        b.poly('RoofTile',[(sig*8.45,-.5,18.94),(sig*10.90,-5.42,17.10),(sig*10.90,5.42,17.10),(sig*8.45,.5,18.94)],part='Upper_hip_roof')
        if level<=2:
            for x in np.arange(-10.48,10.5,.41*skip):
                for yy in np.arange(.65,5.38,.37*skip):
                    t=(yy-.5)/4.92
                    z=18.94-1.84*t
                    if abs(x)>8.5+2.4*t:continue
                    y=sig*yy
                    b.beam('RoofTile',(x,y-sig*.17,z+.05),(x,y+sig*.17,z+.05),.19,'Upper_individual_roof_tiles')
    b.beam('RoofTile',(-8.53,0,18.95),(8.53,0,18.95),.30,'Upper_roof_ridge')
    for y in [-5.43,5.43]:b.beam('Wood',(-10.9,y,17.03),(10.9,y,17.03),.20,'Upper_eave_fascia')
    for x in [-10.9,10.9]:
        for y in [-5.43,5.43]:
            b.beam('RoofTile',(x,y,17.15),(x+np.sign(x)*.5,y+np.sign(y)*.39,17.54),.28,'Upper_upturned_corners')
    # Dougong-like layered wood bracket blocks at columns, with clear 3-D depth.
    if level<=2:
        for z,xmax,yfront,nx in [(11.75,12,5.75,11),(16.8,9.6,4.16,10)]:
            for x in np.linspace(-xmax,xmax,nx//skip+1):
                for y in [-yfront,yfront]:
                    for layer in range(3 if level==0 else 2):
                        off=.19*layer
                        bx('Wood',x-.19-off,x+.19+off,y-.23-off,y+.23+off,z-.11+layer*.18,z+.10+layer*.18,part='Layered_dougong_brackets')
    # Recessed door leaves: one on each side of gateway opening, center stays walkable.
    for sign in (-1,1):
        x0,x1=((-2.42,-.68) if sign<0 else (.68,2.42))
        bx('DoorWood',x0,x1,2.15,2.28,.09,3.37,part='Recessed_timber_gate_leaves')
        for j in range(1,5):
            bx('Wood',x0,x1,2.11,2.19,j*.64,j*.64+.10,part='Door_crossbars')
        if level<=1:
            for i in range(3):
                for j in range(5):
                    x=x0+(i+.5)*(x1-x0)/3;z=.37+j*.51
                    bx('Iron',x-.048,x+.048,2.065,2.115,z-.045,z+.045,part='Door_iron_studs')
        bx('Iron',(x0+x1)*.5-.06,(x0+x1)*.5+.06,2.04,2.10,1.37,1.59,part='Door_hardware')
    # Plaque with explicit Chinese characters in separate texture.
    bx('Sign',-1.65,1.65,-5.97,-5.89,10.72,11.48,part='Signboard_Zhengdongmen',s=3.4)
    # Full UV for the front of the plaque, rather than a repeated/tiled crop.
    b.poly('Sign',[(-1.65,-5.982,10.72),(1.65,-5.982,10.72),(1.65,-5.982,11.48),(-1.65,-5.982,11.48)],[(0,0),(1,0),(1,1),(0,1)],part='Plaque_full_uv')
    # Grey stone ramp / cobbled paving to establish arch and ground contact.
    bx('Stone',-3.16,3.16,-8.24,8.15,-.23,-.035,part='Tunnel_roadway')
    return b

def save_scene(builder,level,images):
    s=trimesh.Scene(); stats={}
    for name in MATERIALS:
        mesh=builder.mesh('ZDM',name,images[name]);
        if mesh is not None:
            s.add_geometry(mesh,geom_name=f'SM_ZDM_LOD{level}_{name}',node_name=f'LOD{level}_{name}')
            stats[name]={'triangles':len(mesh.faces),'vertices':len(mesh.vertices)}
    outfile=OUT/f'Zhengdongmen_LOD{level}.glb'; outfile.write_bytes(s.export(file_type='glb'))
    return outfile,stats

def main():
    images={m:Image.open(TEX/f'T_ZDM_{m}_BaseColor_4K.png').resize((512,512)) for m in MATERIALS} if all((TEX/f'T_ZDM_{m}_BaseColor_4K.png').exists() for m in MATERIALS) else textures()
    summary={}
    for level in range(5):
        b=build(level); p,stats=save_scene(b,level,images)
        summary[f'LOD{level}']={'glb':p.name,'materials':stats,'triangles':sum(v['triangles'] for v in stats.values()),'parts':b.parts}
        print(f"LOD{level}: {summary[f'LOD{level}']['triangles']:,} triangles; {p.stat().st_size/1048576:.1f} MiB",flush=True)
    # Unreal UCX prefix is recognized for autogenerated simple collision; kept separate.
    collision=trimesh.Scene()
    for name,x0,x1,y0,y1,z0,z1 in [('LeftWall',-14,-3.25,-7.4,7.4,0,9.45),('RightWall',3.25,14,-7.4,7.4,0,9.45),('TopArch',-3.25,3.25,-7.4,7.4,6.9,9.45),('Gatehouse',-12,12,-5.0,5.0,9.45,18.95)]:
        size=np.array([x1-x0,y1-y0,z1-z0]); cen=np.array([(x1+x0)/2,(y1+y0)/2,(z1+z0)/2]);m=trimesh.creation.box(extents=size);m.apply_translation(cen)
        collision.add_geometry(m,geom_name=f'UCX_SM_ZDM_{name}_00')
    (ROOT/'Collision'/'Zhengdongmen_simple_collision.glb').write_bytes(collision.export(file_type='glb'))
    (ROOT/'Documentation'/'mesh_report.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False),encoding='utf-8')
    print('DONE',flush=True)
if __name__=='__main__':main()
