"""Reference-tuned Zhengdongmen geometry, derived from the supplied procedural source.
Metres, Z up, facade -Y. Run Scripts/TuneZhengdongmenReference.py to stage UE meshes.
The original source package remains immutable; artwork comes from its Compact set.
"""
import math, random
from collections import Counter
import numpy as np
import trimesh
from trimesh.visual.material import PBRMaterial
from trimesh.visual.texture import TextureVisuals

random.seed(1965)
# UV per planar face: coordinates are metres divided by repeating tile size.
MATERIALS=['Stone','Wood','RoofTile','Ridge','Plaster','Iron','DoorWood','Sign']
BASECOL={'Stone':(114,110,101),'Wood':(86,63,45),'RoofTile':(75,74,70),'Ridge':(64,64,61),'Plaster':(170,159,141),'Iron':(57,58,58),'DoorWood':(90,60,41),'Sign':(57,32,24)}

class Builder:
    def __init__(self):self.a={m:{'v':[],'f':[],'uv':[]} for m in MATERIALS};self.parts={};self.roof_courses={};self.roof_extents={};self.roof_profile={}
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
    def beam(self,m,start,end,width,part='Beams',uv=None,s=1.6):
        a=np.array(start,float);b=np.array(end,float); d=b-a; L=float(np.linalg.norm(d)); d/=L
        q=np.array((0,0,1),float) if abs(d[2])<.9 else np.array((0,1,0),float)
        side=np.cross(d,q); side/=np.linalg.norm(side)
        up=np.cross(d,side);up/=np.linalg.norm(up)
        # Keep the section's "up" pointing up. Flipping both axes leaves the eight
        # corners in exactly the same places, so this changes no geometry — it only
        # makes a section-following UV put the same band on the same face whatever
        # direction the beam runs. Without it the horizontal ridge beams had their
        # "up" pointing down.
        if up[2]<0:side,up=-side,-up
        v=[]
        for cen in (a,b):
            v.extend([tuple(cen+sign1*side*width/2+sign2*up*width/2) for sign1,sign2 in [(-1,-1),(1,-1),(1,1),(-1,1)]])
        around=[0.0,.25,.5,.75]
        for ids in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            pts=[v[i] for i in ids]
            if uv!='axis' or ids in [(0,3,2,1),(4,5,6,7)]:
                self.poly(m,pts,part=part);continue
            # Along the beam on u, around its section on v. The face that wraps the
            # seam takes v 0.75 -> 1.0 rather than 0.75 -> 0, or it mirrors.
            uvs=[]
            for i in ids:
                along=0.0 if i<4 else L
                k=i%4
                frac=1.0 if (ids==(3,0,4,7) and k==0) else around[k]
                uvs.append((along/s,frac))
            self.poly(m,pts,uvs,part)

    # Section of a roof ridge (屋脊): a flat base carrying a half-round cap. It fills
    # the SAME bounding box as the square beam it replaces — a semicircle of radius
    # width/2 on top of a width/2 base — so the measured envelope does not move, but
    # the silhouette reads as a ridge tile instead of a plank. The square section it
    # replaces was the visible defect: swept along a diagonal hip, a 0.34 m square
    # tube is a flat-sided beam, and with the default world-XZ planar UV its roof-tile
    # texture smeared along the ridge instead of running down it.
    RIDGE_ARC=8

    def ridge_beam(self,m,start,end,width,part='Ridges',s=1.5):
        """A ridge cap: half-round 脊瓦 section swept along a line, UV along the ridge."""
        a=np.array(start,float);b=np.array(end,float); d=b-a; L=float(np.linalg.norm(d)); d/=L
        q=np.array((0,0,1),float) if abs(d[2])<.9 else np.array((0,1,0),float)
        side=np.cross(d,q); side/=np.linalg.norm(side)
        up=np.cross(d,side);up/=np.linalg.norm(up)
        if up[2]<0:side,up=-side,-up
        r=width/2.0
        section=[(-r,-r),(r,-r)]
        for k in range(self.RIDGE_ARC+1):
            t=math.pi*k/self.RIDGE_ARC
            section.append((r*math.cos(t),r*math.sin(t)))
        per=[0.0]
        for i in range(1,len(section)):
            per.append(per[-1]+math.hypot(section[i][0]-section[i-1][0],
                                          section[i][1]-section[i-1][1]))
        total=per[-1]+math.hypot(section[0][0]-section[-1][0],
                                 section[0][1]-section[-1][1])
        rings=[[tuple(cen+cs*side+cu*up) for cs,cu in section] for cen in (a,b)]
        n=len(section)
        for i in range(n):
            j=(i+1)%n
            v1=(per[j] if j else total)/total
            self.poly(m,[rings[0][i],rings[1][i],rings[1][j],rings[0][j]],
                      [(0.0,per[i]/total),(L/s,per[i]/total),(L/s,v1),(0.0,v1)],part)
        for ring in rings:
            for i in range(1,n-1):
                self.poly(m,[ring[0],ring[i],ring[i+1]],part=part)
    # Cross-slope profile of one roof course, in metres. `q` runs -1..1 across a
    # single tile pitch: a barrel rib (筒瓦) centred at q=0 with the pans (板瓦) it
    # caps falling away to the pitch edges. Modelled as a corrugation rather than a
    # flat cap, because a roof reads as its down-slope courses, and courses only
    # read if the ribs stay in line.
    RIB_H=0.066         # rib proud of the slope plane
    RIB_FRACTION=0.46   # share of the pitch the barrel rib spans
    PAN_D=0.020         # pan trough below the rib base
    SHELL=0.030         # tile shell thickness, and so the course-step height
    BASE=0.010          # clearance of the trough above the slope plane
    CROSS=11            # samples across a pitch

    @staticmethod
    def _crown(q):
        """Rounded rib over the middle of the pitch, concave pan either side."""
        a=abs(q); w=Builder.RIB_FRACTION
        if a<w:
            return Builder.RIB_H*math.cos(math.pi*q/(2.0*w))**2
        return -Builder.PAN_D*math.sin(math.pi*(a-w)/(2.0*(1.0-w)))**2

    def roof_tile(self, surface, t0, t1, run_centre, span0, span1, col, row, across_pitch):
        """One overlapping course segment: a piece of a single down-slope run.

        `span0`/`span1` are the across ranges at the two course ends, so a run the
        hip trims becomes a cut tile instead of leaving a staircase gap. The crown
        phase is anchored to the run centre rather than to the clipped span, so the
        corrugation stays locked to the rib grid even where a course is cut.
        """
        cross=np.linspace(0.0,1.0,self.CROSS)
        vertices=[];uvs=[]
        for underside in (False,True):
            for t,(lo,hi) in ((t0,span0),(t1,span1)):
                for f in cross:
                    across=lo+(hi-lo)*f
                    x,y,z=surface(across,t)
                    q=2.0*(across-run_centre)/across_pitch
                    z+=self.BASE+self._crown(q)-(self.SHELL if underside else 0.0)
                    vertices.append((x,y,z))
                    # One texture pitch per run across, one course per row down.
                    # The tile atlas is seamless, so wrapping past 0..1 is fine.
                    u=(run_centre/across_pitch+0.2+q*0.5)/8.0
                    v=(row+(0.0 if t==t0 else 1.0))/8.0
                    uvs.append((u,v))
        base=len(self.a['RoofTile']['v'])
        a=self.a['RoofTile']
        a['v'].extend(vertices);a['uv'].extend(uvs)
        faces=[]
        # Curved outer face and underside.
        for i in range(len(cross)-1):
            tl,tr= i,i+1
            bl,br= len(cross)+i,len(cross)+i+1
            faces.extend([(tl,tr,br),(tl,br,bl)])
            off=2*len(cross)
            faces.extend([(off+tl,off+br,off+tr),(off+tl,off+bl,off+br)])
        # Close the two sloping ends and the two curved side rails.
        off=2*len(cross)
        for i in range(len(cross)-1):
            faces.extend([(i,off+i,off+i+1),(i,off+i+1,i+1)])
            lo=len(cross)+i;hi=len(cross)+i+1
            faces.extend([(lo,hi,off+hi),(lo,off+hi,off+lo)])
        for i in (0,len(cross)-1):
            lo,hi=i,len(cross)+i
            faces.extend([(lo,off+lo,off+hi),(lo,off+hi,hi)])
        edge_use=Counter(tuple(sorted((face[k],face[(k+1)%3])))
                         for face in faces for k in range(3))
        assert len(vertices)-len(edge_use)+len(faces)==2
        assert all(uses==2 for uses in edge_use.values())
        a['f'].extend([tuple(base+index for index in face) for face in faces])
        self.parts['Continuous_curved_roof_tiles']=self.parts.get('Continuous_curved_roof_tiles',0)+len(faces)


    def arch(self,r0,r1,cy,zbase,y0,y1,m='Stone',seg=40,part='Arch_voussoirs'):
        for k in range(seg):
            a=math.pi*k/seg;b=math.pi*(k+1)/seg
            def p(r,t,y):return (r*math.cos(t),y,zbase+r*math.sin(t))
            q=[p(r0,a,y0),p(r1,a,y0),p(r1,b,y0),p(r0,b,y0)]
            self.poly(m,q,[(r*t/1.6,r/1.6) for r,t in [(r0,a),(r1,a),(r1,b),(r0,b)]],part=part)
            self.poly(m,[p(r0,b,y1),p(r1,b,y1),p(r1,a,y1),p(r0,a,y1)],part=part)
            self.poly(m,[p(r0,b,y0),p(r0,a,y0),p(r0,a,y1),p(r0,b,y1)],[(r0*b/1.6,y0/1.6),(r0*a/1.6,y0/1.6),(r0*a/1.6,y1/1.6),(r0*b/1.6,y1/1.6)],part=part)
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

def tiled_roof_slope(b, name, max_half_width, across_pitch, slope_length,
                     slope_pitch, surface, part):
    """Cover one hip-roof face with continuous down-slope tile courses.

    Every course is a run of overlapping segments laid end to end from ridge to
    eave, so the run reads as one unbroken line down the slope. Runs are laid on a
    fixed rib grid and clipped to the slope's own trapezoid, which is what closes
    the hip gaps: covering each row with a rectangle of the *narrowest* half-width
    leaves a staircase of uncovered wedges along both hips, and offsetting the odd
    rows by half a pitch (brick bond) additionally exposes both side edges.
    """
    dt=slope_pitch/slope_length
    rows=max(1,math.ceil(1.0/dt))
    w_eave=max(max_half_width(0.0),max_half_width(1.0))
    columns=max(1,int(math.ceil(2.0*w_eave/across_pitch)))
    centres=[(index+0.5-columns/2.0)*across_pitch for index in range(columns)]
    half=across_pitch/2.0
    e_ridge,e_eave=max_half_width(0.0),max_half_width(1.0)
    taper=(e_eave-e_ridge)
    minimum=across_pitch*0.03
    tile_count=0;cut=0
    extents=[]
    for row in range(rows):
        centre=(row+.5)/rows
        r0=max(0.0,centre-0.57/rows)
        r1=min(1.0,centre+0.57/rows)
        for col,across in enumerate(centres):
            # A run only exists where the trapezoid is wide enough to hold it. It
            # enters as a sliver, so start it at the parameter where its clipped
            # width reaches `minimum` rather than at zero width, and split it where
            # it finally fits whole. Every edge of the resulting tile then lies on
            # the straight hip line, so the mesh is exact and nothing is left bare.
            # Chording across either corner instead is what left uncovered wedges.
            outer=abs(across)+half
            if outer<=e_ridge:
                t_enter=t_fit=0.0
            elif taper<=0:
                t_enter=t_fit=1.0
            else:
                # Enter slightly inside the minimum width: aiming at it exactly
                # lands on the drop threshold and the course is discarded.
                t_enter=max(0.0,min(1.0,(outer-across_pitch+minimum*1.05-e_ridge)/taper))
                t_fit=min(1.0,(outer-e_ridge)/taper)
            start=max(r0,t_enter)
            if start>=r1:
                continue
            if t_fit<=start:
                pieces=[(start,r1)]
            elif t_fit<r1:
                pieces=[(start,t_fit),(t_fit,r1)]
            else:
                pieces=[(start,r1)]
            for a,c in pieces:
                spans=[]
                for t in (a,c):
                    edge=max_half_width(t)
                    spans.append((max(across-half,-edge),min(across+half,edge)))
                if min(hi-lo for lo,hi in spans)<minimum:
                    cut+=1;continue
                if spans[0]!=(across-half,across+half) or spans[1]!=(across-half,across+half):
                    cut+=1
                b.roof_tile(surface,a,c,across,spans[0],spans[1],col,row,across_pitch)
                # Recorded in slope-parametric space so the validator can prove the
                # courses cover the whole trapezoid without re-deriving the tiling.
                extents.append([round(a,6),round(c,6),round(across,6),
                                round(spans[0][0],6),round(spans[0][1],6),
                                round(spans[1][0],6),round(spans[1][1],6)])
                tile_count+=1
    b.roof_extents[name]={'half_width_ridge':max_half_width(0.0),
                          'half_width_eave':max_half_width(1.0),
                          'across_pitch':across_pitch,'rows':rows,'columns':columns,
                          'tiles':extents}
    cross=Builder.CROSS
    b.roof_courses[name]={'rows':rows,'columns':columns,'tiles':tile_count,
                          'tiles_trimmed_at_hip':cut,'stagger_rows':0,
                          'tile_width_m':round(across_pitch,3),
                          'tile_run_m':round(slope_length/rows*1.14,3),
                          'across_pitch_m':across_pitch,
                          'course_pitch_m':round(slope_length/rows,3),
                          'rib_height_m':Builder.RIB_H,'rib_fraction':Builder.RIB_FRACTION,
                          'pan_depth_m':Builder.PAN_D,
                          'cross_samples':cross,'shell_stations':2,
                          'closed_shell_vertices_per_tile':4*cross,
                          'closed_shell_triangles_per_tile':2*((2-1)*(cross-1)*2)+2*((2-1)*2)+2*((cross-1)*2),
                          'wall_thickness_cm':Builder.SHELL*100,
                          'every_mesh_edge_used_twice':True,
                          'coverage':'continuous down-slope runs, clipped to the slope trapezoid'}
    return tile_count

def profile_prism(b, material, x0, x1, y0, y1, lo0, lo1, hi0, hi1, part, swap=False, door_uv=False):
    v=[(x0,y0,lo0),(x1,y0,lo1),(x1,y0,hi1),(x0,y0,hi0),
       (x0,y1,lo0),(x1,y1,lo1),(x1,y1,hi1),(x0,y1,hi0)]
    if swap: v=[(y,x,z) for x,y,z in v]
    for ids in [(0,1,2,3),(7,6,5,4),(0,4,5,1),(3,2,6,7),(0,3,7,4),(1,5,6,2)]:
        pts=[v[i] for i in ids]
        if door_uv:
            uv=[((x+2.68)/5.36,z/6.08) for x,y,z in pts]
        else:
            normal=np.abs(np.cross(np.subtract(pts[1],pts[0]),np.subtract(pts[2],pts[0])))
            axes=[a for a in range(3) if a!=int(np.argmax(normal))]
            uv=[(p[axes[0]]/1.6,p[axes[1]]/1.6) for p in pts]
        b.poly(material,pts,uv,part)


def enclosure(b, half_x, half_y, bottom, roof_height, part):
    # Continuous four-sided shell, with overlapping corners and a roof-following
    # top edge. Inset behind existing posts, lattice and balcony balusters.
    for swap, span, face in [(False,half_x,half_y),(True,half_y,half_x)]:
        samples=np.linspace(-span-.08,span+.08,97)
        for sign in (-1,1):
            a,c=sign*face-.09,sign*face+.09
            for x0,x1 in zip(samples[:-1],samples[1:]):
                def top(x):
                    return (roof_height(sign*face,x) if swap else roof_height(x,sign*face))+.035
                profile_prism(b,'Wood',x0,x1,a,c,bottom,bottom,top(x0),top(x1),part,swap)


def build(level):
    # level=0 is the most elaborate procedural high mesh, 4 the coarsest.
    skip=2**level;b=Builder();rng=random.Random(1888)
    def bx(m,*args,part='',s=1.3):b.box(m,*args,part=part,s=s)
    W=28.;Y=7.4; wall=9.45; R=2.66; CZ=3.4;OR=3.22
    # The four slope planes are the substrate the tile courses are laid on, so they
    # must clear the troughs, which dip BASE-SHELL below the plane.
    drop=Builder.BASE-Builder.SHELL-.02
    b.roof_profile={'rib_height_m':Builder.RIB_H,'pan_depth_m':Builder.PAN_D,
                    'shell_m':Builder.SHELL,'base_clearance_m':Builder.BASE,
                    'slope_plane_drop_m':drop}
    # The stone shell leaves the centre arch open; fitted doors are added below.
    for x0,x1 in [(-14,-3.24),(3.24,14)]:
        bx('Stone',x0,x1,-Y,Y,0,wall,part='Foundation_and_wall')
    bx('Stone',-3.24,3.24,-Y,Y,CZ+OR-.07,wall,part='Arch_spandrel')
    for side in (-1,1):
        a,bx1=(-3.24,-R) if side==-1 else (R,3.24)
        bx('Stone',a,bx1,-Y,Y,0,CZ+OR,part='Arch_piers')
        bx('Stone',a,bx1,-Y-.10,-Y+.04,.2,4.8,part='Arch_piers')
    segments=max(32,128//skip)
    b.arch(R,OR,0,CZ,-Y-.08,Y+.04,seg=segments)
    # Fill every spandrel wedge above the OUTER arch, throughout tunnel depth.
    # An annular strip alone leaves triangular sky holes below a straight lintel.
    xs=OR*np.cos(np.linspace(math.pi,0,segments+1))
    zs=CZ+OR*np.sin(np.linspace(math.pi,0,segments+1))
    for i in range(segments):
        profile_prism(b,'Stone',xs[i],xs[i+1],-Y-.075,Y+.035,
                      zs[i]-.015,zs[i+1]-.015,wall,wall,'Closed_arch_spandrels')
    # arch inner lining, framing bands, keystone and revealed stone voussoirs
    if level<3:
        for depth in [-Y+.24,-Y+1.2,Y-1.2,Y-.24]:
            b.arch(R,R+.035,0,CZ,depth-.035,depth+.035,seg=segments,part='Tunnel_stone_rib')
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
            bx('Wood',x-.74,x+.74,yf-.055,yf+.055,10.34,11.42,part='Lower_window_dark_recesses')
            if level<=1:
                for dx in (-.53,0,.53):
                    bx('Wood',x+dx-.033,x+dx+.033,yf-.095,yf+.095,10.33,11.43,part='Lower_window_lattice')
    enclosure(b,11.90,5.55,9.60,
              lambda x,y:min(13.45-(abs(x)-9.8)*1.41/3.22,
                             13.45-(abs(y)-4.2)*1.41/2.36),
              'Continuous_lower_timber_enclosure')
    # Lower roof as four sloping architectural surfaces framing raised upper floor.
    def roof_ring(xin,yin,xout,yout,zinner,zouter,part,step,tiles=True):
        dx=xout-xin;dy=yout-yin;dz=zinner-zouter
        # South and north planes, plus the often-missed east and west hips.
        for sig in [-1,1]:
            b.poly('RoofTile',[(-xout,sig*yout,zouter+drop),(xout,sig*yout,zouter+drop),
                               (xin,sig*yin,zinner+drop),(-xin,sig*yin,zinner+drop)],
                  [(0,0),(xout*2/1.2,0),(xin*2/1.2,(yout-yin)/1.2),(0,(yout-yin)/1.2)],part)
            if tiles:
                def ns_surface(x,t):
                    return x,sig*(yin+dy*t),zinner-dz*t
                tiled_roof_slope(b,f'lower_ns_{sig}',
                    lambda t:xin+dx*t,.42,math.hypot(dy,dz),.35,
                    ns_surface,'lower hip north/south')
        for sig in [-1,1]:
            b.poly('RoofTile',[(sig*xout,-yout,zouter+drop),(sig*xout,yout,zouter+drop),
                               (sig*xin,yin,zinner+drop),(sig*xin,-yin,zinner+drop)],part=part)
            if tiles:
                def ew_surface(y,t):
                    return sig*(xin+dx*t),y,zinner-dz*t
                tiled_roof_slope(b,f'lower_ew_{sig}',
                    lambda t:yin+dy*t,.42,math.hypot(dx,dz),.35,
                    ew_surface,'lower hip east/west')
        for y in [-yout,yout]:b.beam('Wood',(-xout,y,zouter-.15),(xout,y,zouter-.15),.18,'Eave_fascia')
        for x in [-xout,xout]:b.beam('Wood',(x,-yout,zouter-.13),(x,yout,zouter-.13),.18,'Eave_fascia')
        for x in [-xout,xout]:
            for y in [-yout,yout]:
                b.ridge_beam('Ridge',(x,y,zouter),(x+np.sign(x)*.36,y+np.sign(y)*.38,zouter+.30),.20,'Upturned_eave_corners')
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
            bx('Wood',x-.62,x+.62,yf-.075,yf+.075,14.58,16.72,part='Upper_window_dark_recesses')
            if level<=1:
                for dx in (-.42,0,.42):
                    bx('Wood',x+dx-.032,x+dx+.032,yf-.11,yf+.11,14.58,16.72,part='Upper_window_lattice')
    enclosure(b,9.58,3.95,13.50,
              lambda x,y:min(18.94-max(0,abs(x)-8.45)*1.84/2.45,
                             18.94-max(0,abs(y)-.5)*1.84/4.92),
              'Continuous_upper_timber_enclosure')
    # End-wall framing repeats the front window rhythm over continuous timber.
    for hx,hy,z0,z1,step in [(11.90,5.55,10.34,11.42,2.25),(9.58,3.95,14.58,16.72,1.84)]:
        for sign in (-1,1):
            face=sign*(hx+.115)
            for y in np.arange(-hy+.3,hy-.2,.24):
                bx('Wood',face-.05,face+.05,y-.028,y+.028,z0,z1,part='End_window_lattice')
            for z in (z0,z0+(z1-z0)*.78,z1):
                bx('Wood',face-.06,face+.06,-hy,hy,z-.035,z+.035,part='End_window_rails')
    # Upper hip roof with complete four-face, continuous down-slope tile courses.
    for sig in [-1,1]:
        b.poly('RoofTile',[(-8.45,sig*.5,18.94+drop),(8.45,sig*.5,18.94+drop),
                           (10.9,sig*5.42,17.10+drop),(-10.9,sig*5.42,17.10+drop)],part='Upper_hip_roof')
        b.poly('RoofTile',[(sig*8.45,-.5,18.94+drop),(sig*10.90,-5.42,17.10+drop),
                           (sig*10.90,5.42,17.10+drop),(sig*8.45,.5,18.94+drop)],part='Upper_hip_roof')
        if level<=2:
            def ns_surface(x,t):return x,sig*(.5+4.92*t),18.94-1.84*t
            tiled_roof_slope(b,f'upper_ns_{sig}',lambda t:8.45+2.45*t,
                .42,math.hypot(4.92,1.84),.35,ns_surface,'upper roof north/south')
            def ew_surface(y,t):return sig*(8.45+2.45*t),y,18.94-1.84*t
            tiled_roof_slope(b,f'upper_ew_{sig}',lambda t:.5+4.92*t,
                .42,math.hypot(2.45,1.84),.35,ew_surface,'upper roof east/west')
    b.ridge_beam('Ridge',(-8.53,0,18.95),(8.53,0,18.95),.30,'Upper_roof_ridge')
    for y in [-5.43,5.43]:b.beam('Wood',(-10.9,y,17.03),(10.9,y,17.03),.20,'Upper_eave_fascia')
    for x in [-10.9,10.9]:
        for y in [-5.43,5.43]:
            b.ridge_beam('Ridge',(x,y,17.15),(x+np.sign(x)*.5,y+np.sign(y)*.39,17.54),.28,'Upper_upturned_corners')
    # Raised hip ridges (垂脊). Widened so they cap the cut course ends where the
    # trapezoid clip trims the outermost run of each slope. Deliberately NOT lifted
    # above the hip line: a +7 cm lift made the hip ridge the tallest part of the
    # building and pushed the measured height to 1940 cm against the package's
    # declared 1933 cm. Seated on the line, the ridge and the roof ridge beam top
    # out together, which is also how the reference reads.
    for xin,yin,xout,yout,zinner,zouter,name in [
        (9.8,4.2,13.02,6.56,13.45,12.04,'Lower'),
        (8.45,.5,10.9,5.42,18.94,17.10,'Upper')]:
        for sx in (-1,1):
            for sy in (-1,1):
                b.ridge_beam('Ridge',(sx*xin,sy*yin,zinner),
                       (sx*xout,sy*yout,zouter),.34,name+'_diagonal_hip_caps')
    # Dougong-like layered wood bracket blocks at columns, with clear 3-D depth.
    if level<=2:
        for z,xmax,yfront,nx in [(11.75,12,5.75,11),(16.8,9.6,4.16,10)]:
            for x in np.linspace(-xmax,xmax,nx//skip+1):
                for y in [-yfront,yfront]:
                    for layer in range(3 if level==0 else 2):
                        off=.19*layer
                        bx('Wood',x-.19-off,x+.19+off,y-.23-off,y+.23+off,z-.11+layer*.18,z+.10+layer*.18,part='Layered_dougong_brackets')
    # Closed recessed door pair fitted to the semicircular opening.
    for sign in (-1,1):
        x0,x1=((-2.68,0.005) if sign<0 else (-0.005,2.68))
        # Two solid arch-topped leaves with a hidden 2 cm frame overlap.
        # The crown follows the same circle as the masonry, not stepped boxes.
        xs=np.unique(np.r_[x0,x1,2.68*np.cos(np.linspace(0,math.pi,257))])
        xs=xs[(xs>=x0)&(xs<=x1)]
        for xa,xb in zip(xs[:-1],xs[1:]):
            za=CZ+math.sqrt(max(0,2.68**2-xa**2))
            zb=CZ+math.sqrt(max(0,2.68**2-xb**2))
            profile_prism(b,'DoorWood',xa,xb,2.15,2.32,-.015,-.015,za,zb,
                          'Arch_fitted_gate_leaves',door_uv=True)
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
