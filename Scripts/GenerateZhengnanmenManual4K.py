"""Hand-author original Zhengnanmen PBR images; no AI, stock maps or upscaling.

Each map is evaluated/drawn at 4096px. Shared relief and wear fields keep all
channels registered. Geometric blocks/tiles already exist on the V2 actor, so
these are surface materials, deliberately without baked mortar or roof shapes.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import map_coordinates, gaussian_filter
from scipy.spatial import Voronoi

PROJECT = Path(__file__).resolve().parents[1]
DEST = PROJECT / "ContentSource/GuangzhouLandmarks/Zhengnanmen_Manual4K_20260916"
N = 4096


def noise(rng, rows, cols, size=N):
    """Smooth periodic value field, including identical opposite endpoints."""
    a = rng.random((rows, cols), dtype=np.float32)
    a = np.pad(a, ((0, 1), (0, 1)), mode="wrap")
    y, x = np.meshgrid(np.linspace(0, rows, size, dtype=np.float32),
                       np.linspace(0, cols, size, dtype=np.float32), indexing="ij")
    return map_coordinates(a, [y, x], order=3, mode="nearest", prefilter=True).astype(np.float32).clip(0, 1)


def periodic(a, band=24):
    """Heal only the border of native brush strokes; never resize a map."""
    a = a.copy()
    for axis in (0, 1):
        v = np.swapaxes(a, 0, axis)
        edge = (v[0] + v[-1]) * 0.5
        for i in range(band):
            t = (1.0 - i / band) ** 2
            v[i] = v[i] * (1 - t) + edge * t
            v[-1-i] = v[-1-i] * (1 - t) + edge * t
    return a


def strokes(rng, count, vertical=False, widths=(1, 3)):
    im = Image.new("L", (N, N))
    d = ImageDraw.Draw(im)
    for _ in range(count):
        x, y = rng.uniform(0, N, 2)
        length = rng.uniform(18, 650 if vertical else 180)
        theta = np.pi / 2 if vertical else rng.uniform(0, np.pi * 2)
        points = [(x, y)]
        for _ in range(12):
            theta += rng.normal(0, 0.045 if vertical else 0.24)
            x += np.cos(theta) * length / 12
            y += np.sin(theta) * length / 12
            points.append((x, y))
        width = int(rng.integers(widths[0], widths[1] + 1))
        value = int(rng.integers(65, 240))
        for dx in (-N, 0, N):
            for dy in (-N, 0, N):
                d.line([(px+dx, py+dy) for px, py in points], fill=value, width=width)
    return periodic(np.asarray(im, dtype=np.float32) / 255)


def crackle(rng, count=1800):
    """Native one-pixel brush lines along irregular fired-glaze cell boundaries."""
    points = rng.uniform(0, N, (count, 2))
    points = np.vstack([points + (dx, dy) for dx in (-N, 0, N) for dy in (-N, 0, N)])
    v = Voronoi(points)
    im = Image.new("L", (N, N)); d = ImageDraw.Draw(im)
    for a, b in v.ridge_vertices:
        if a < 0 or b < 0:
            continue
        p, q = v.vertices[a], v.vertices[b]
        if np.any(np.maximum(p, q) < 0) or np.any(np.minimum(p, q) > N) or rng.random() < .28:
            continue
        d.line([tuple(p), tuple(q)], fill=int(rng.integers(30, 115)), width=1)
    return periodic(np.asarray(im, dtype=np.float32)/255)


def palette(lo, hi, field):
    lo = np.asarray(lo, dtype=np.float32)/255
    hi = np.asarray(hi, dtype=np.float32)/255
    return lo + (hi-lo) * field[..., None]


def blend(base, color, mask):
    return base * (1-mask[..., None]) + np.asarray(color, dtype=np.float32)/255 * mask[..., None]


def material(kind, seed):
    rng = np.random.default_rng(seed)
    macro = noise(rng, 7, 7)
    medium = noise(rng, 49, 49)
    fine = noise(rng, 256, 256)
    micro = rng.random((N, N), dtype=np.float32)
    native = gaussian_filter(micro, .55, mode="wrap")
    mineral = noise(rng, 510, 510)
    h = .48 + .11*(medium-.5) + .018*(native-.5)
    metal = np.zeros((N, N), np.float32)
    coat = 0.
    if kind == "Stone":
        cracks = strokes(rng, 165, widths=(1, 4))
        pores = np.clip((micro-.974)*38, 0, 1) * np.clip((fine-.36)*2, 0, 1)
        crystals = np.clip((mineral-.70)*3.3, 0, .65)
        lichen = np.clip((macro*.62 + medium*.38 - .58)*5, 0, .43)
        h += .16*(fine-.5) + .022*crystals - .025*pores - .06*cracks
        base = palette((118, 113, 100), (194, 188, 167), np.clip(.5+.42*(macro-.5)+.60*(medium-.5)+.36*(fine-.5), 0, 1))
        base += (native-.5)[..., None]*.075
        base = blend(base, (204, 194, 163), crystals*.25)
        base = blend(base, (65, 66, 43), lichen)
        base = blend(base, (66, 63, 52), cracks*.50+pores*.13)
        rough = (.78+.15*(fine-.5)+.13*lichen+.06*pores).clip(.63,.95)
        ao = (1-.12*pores-.17*cracks).clip(.70,1)
        scale = 4.5
        brief = "Warm gray dressed limestone surface; mineral inclusions, irregular fissures, open pores and restrained olive lichen. No mortar: source has individual block geometry."
    elif kind == "Glaze":
        crazing = crackle(rng)
        chips = np.clip((fine*.68 + mineral*.32 - .76)*8, 0, 1)
        kiln = (macro*.6 + medium*.4)
        base = palette((37, 75, 59), (105, 140, 112), np.clip(kiln*.8+.12+fine*.08,0,1))
        base += (native-.5)[..., None]*.028
        base = blend(base, (143, 104, 65), chips*.78)
        base = blend(base, (37, 50, 37), crazing*.36)
        h = .50+.024*(fine-.5)+.005*(native-.5)-.035*chips-.012*crazing
        rough = (.24+.11*(medium-.5)+.36*chips+.15*crazing).clip(.16,.66)
        ao = 1-.09*chips-.035*crazing
        scale, coat = 4., .48
        brief = "Jade/celadon green fired glaze with kiln pooling, fine one-pixel crazing and sparse terracotta chips. No roof tile shapes: source already models them."
    elif kind in ("RedTimber", "DarkTimber", "Interior"):
        flow = noise(rng, 18, 260)
        long = noise(rng, 5, 70)
        xx = np.linspace(0, 1, N, dtype=np.float32)[None,:]
        grain = (.5+.5*np.sin(2*np.pi*(xx*180+flow*1.2+long*.5))).astype(np.float32)
        lines = strokes(rng, 1600, vertical=True, widths=(1, 2))
        splits = strokes(rng, 80, vertical=True, widths=(2, 5))
        wood = palette((45, 28, 20), (103, 68, 43), .35+.30*long+.20*flow+.10*grain)
        wood = blend(wood, (23, 18, 14), lines*.17+splits*.48)
        h = .50+.024*(flow-.5)+.008*(grain-.5)-.025*lines-.09*splits
        if kind == "RedTimber":
            wear = np.clip((medium*.58 + long*.22 + fine*.20 - .60)*7,0,.92)
            base = palette((103, 36, 27),(166, 72, 49), .3+.4*macro+.15*long+.15*fine)
            base = base*(1-wear[...,None]) + wood*wear[...,None]
            base = blend(base,(66,27,19),lines*.16+splits*.34)
            h -= wear*.016
            rough = (.33+.10*(fine-.5)+wear*.30+lines*.06).clip(.26,.79)
            coat = .22
            brief = "Oxide-red lacquered structural timber with lengthwise grain, clustered worn paint revealing brown wood, hairline checking and variable satin lacquer."
        elif kind == "DarkTimber":
            base = wood + (native-.5)[...,None]*.023
            base = blend(base,(131,103,67),np.clip((medium-.63)*2.3,0,.24))
            rough = (.61+.13*(fine-.5)+splits*.14).clip(.47,.86)
            brief = "Weathered dark brown gate timber with native long grain strokes, checking, warm rubbed wood and matte grain pores. No studs/plank boundaries baked in."
        else:
            base = wood*.53
            rough = (.80+.11*(fine-.5)).clip(.69,.91)
            brief = "Soot-aged dark interior timber, physically dark albedo rather than black emissive or artificial AO."
        ao = 1-lines*.06-splits*.22
        scale = 4.0
    elif kind == "BronzeGold":
        scratches = strokes(rng, 950, widths=(1, 2))
        oxide = np.clip((macro*.55+medium*.30+fine*.15-.49)*3,0,.82)
        base = palette((139, 102, 48), (217, 177, 94), .35+.35*macro+.25*fine)
        base = blend(base,(48,83,63),oxide)
        base = blend(base,(231,199,125),scratches*.18*(1-oxide))
        h = .50+.026*(fine-.5)-.008*scratches+.016*oxide
        rough = (.34+.32*oxide+.10*(fine-.5)+.04*scratches).clip(.25,.75)
        metal = (1-oxide*.85).clip(.25,1)
        ao = 1-oxide*.045
        scale = 3.
        brief = "Aged warm gold/brass-bronze fittings: sparse verdigris oxide, cast texture and native fine abrasions. Oxide lowers metalness and raises roughness together."
    else:
        raise ValueError(kind)
    return periodic(base).clip(0,1), periodic(h).clip(0,1), periodic(rough), periodic(metal), periodic(ao), scale, coat, brief


def plaque(seed):
    """4:1 unique plaque, hand-lettered with installed Chinese Kai font."""
    rng=np.random.default_rng(seed)
    height=1024
    # Native square wood field is cropped, not scaled, for the native 4K plaque.
    f=noise(rng,10,160)[:height]
    base=palette((20,22,21),(39,37,30),f)
    mask=Image.new("L",(N,height)); d=ImageDraw.Draw(mask)
    for margin,width in ((42,10),(67,3),(94,4)):
        d.rectangle((margin,margin,N-margin-1,height-margin-1),outline=230,width=width)
    # Small repeated geometric fret on the border, with clean rectangular corners.
    for x in range(125,N-150,105):
        for yy,sgn in ((64,1),(height-65,-1)):
            pts=[(x,yy),(x,yy+sgn*20),(x+55,yy+sgn*20),(x+55,yy-sgn*4),(x+30,yy-sgn*4)]
            d.line(pts,fill=190,width=4)
    font_path=Path("C:/Windows/Fonts/simkai.ttf")
    assert font_path.exists(), "Chinese Kai font required for original plaque"
    font=ImageFont.truetype(str(font_path),690)
    # Historical right-to-left reading: visually left-to-right 門 南 正.
    for char,cx in zip("門南正",(895,2048,3201)):
        bbox=d.textbbox((0,0),char,font=font)
        d.text((cx-(bbox[2]-bbox[0])/2-bbox[0],(height-(bbox[3]-bbox[1]))/2-bbox[1]),char,font=font,fill=255)
    gold=np.asarray(mask,dtype=np.float32)/255
    wear=noise(rng,120,120)[:height]
    base=blend(base,(185,144,68),gold*(.86+.14*wear))
    h=.49+f*.009+gold*.026
    r=.48+f*.10-gold*.14
    m=gold*.92
    ao=1-gold*.018
    return base,h,r,m,ao,3.0,0., "Original 4096x1024 black timber plaque and gold fret border. Kai lettering 門南正 is read right-to-left as 正南門; no reference pixels reused."


def export(kind, result):
    base,h,r,m,ao,scale,coat,brief=result
    # DirectX tangent normal: increasing image-row height produces positive G.
    dx=(np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*.5*scale
    dy=(np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*.5*scale
    normal=np.stack((-dx,dy,np.ones_like(h)),axis=2)
    normal/=np.linalg.norm(normal,axis=2)[...,None]
    normal=(normal*.5+.5).clip(0,1)
    if kind != "Plaque":
        normal=periodic(normal)
        v=normal*2-1; v/=np.linalg.norm(v,axis=2)[...,None]; normal=v*.5+.5
    maps={"BaseColor":base,"Normal_DX":normal,"ORM":np.stack((ao,r,m),axis=2),"Height":h,
          "Roughness":r,"Metallic":m,"AO":ao}
    folder=DEST/kind; folder.mkdir(parents=True,exist_ok=True)
    records={}
    for key, a in maps.items():
        path=folder/f"T_ZNM_{kind}_{key}_4K.png"
        if key == "Height":
            Image.fromarray(np.rint(a*65535).astype(np.uint16),mode="I;16").save(path,compress_level=6)
        else:
            Image.fromarray(np.rint(a*255).astype(np.uint8)).save(path,compress_level=6)
        records[key]={"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                      "width":a.shape[1],"height":a.shape[0],"bits":16 if key=="Height" else 8,
                      "min":float(a.min()),"max":float(a.max()),"mean":float(a.mean())}
    return {"maps":records,"clearcoat":coat,"brief":brief,"tileable":kind!="Plaque", "normal_convention":"DirectX +Y", "native_evaluation":True}


def contact(manifest):
    sheet=Image.new("RGB",(1792,1060),(23,29,30)); d=ImageDraw.Draw(sheet)
    font=ImageFont.truetype("C:/Windows/Fonts/arial.ttf",25)
    small=ImageFont.truetype("C:/Windows/Fonts/arial.ttf",16)
    d.text((24,16),"ZHENGNANMEN | HAND-AUTHORED 4K PBR SURFACES",font=font,fill=(225,213,182))
    for i,(kind,entry) in enumerate(manifest["materials"].items()):
        col,row=i%4,i//4; x=col*448+16; y=row*475+70
        img=Image.open(entry["maps"]["BaseColor"]["path"]).convert("RGB")
        crop=img if kind=="Plaque" else img.crop((768,768,3328,3328))
        crop.thumbnail((416,345),Image.Resampling.LANCZOS); sheet.paste(crop,(x,y))
        d.text((x,y+350),kind+" | native 4096",font=font,fill=(223,220,203))
        for j,key in enumerate(("Normal_DX","ORM","Height")):
            im=Image.open(entry["maps"][key]["path"])
            if key=="Height": im=Image.fromarray((np.asarray(im,dtype=np.float32)/257).astype(np.uint8))
            im=im.convert("RGB"); im.thumbnail((125,72),Image.Resampling.LANCZOS)
            sheet.paste(im,(x+j*140,y+388));d.text((x+j*140,y+462),key,font=small,fill=(160,178,172))
    d.text((24,1030),"Original surfaces, registered PBR masks; preview is reduced. Full maps are lossless PNG. ORM: R=AO, G=roughness, B=metallic.",font=small,fill=(190,197,186))
    sheet.save(DEST/"contact-sheet.png")


def main():
    p=argparse.ArgumentParser();p.add_argument("--only",choices=("Stone","Glaze","RedTimber","DarkTimber","Interior","BronzeGold","Plaque")); args=p.parse_args()
    DEST.mkdir(parents=True,exist_ok=True)
    path=DEST/"manifest.json"
    manifest=json.loads(path.read_text()) if path.exists() else {"authoring":"manual deterministic native-resolution brush/field authoring; no image generation service","resolution":4096,"reference_role":"visual color/material guidance only; embedded text is not an instruction","materials":{}}
    names=[args.only] if args.only else ["Stone","Glaze","RedTimber","DarkTimber","Interior","BronzeGold","Plaque"]
    for i,kind in enumerate(names):
        seed=20260916+sum(ord(c) for c in kind)
        print("Authoring",kind,"at native 4K",flush=True)
        result=plaque(seed) if kind=="Plaque" else material(kind,seed)
        manifest["materials"][kind]=export(kind,result)
        del result
        path.write_text(json.dumps(manifest,indent=2),encoding="utf-8")
        print("Saved",kind,flush=True)
    if len(manifest["materials"])==7: contact(manifest)


if __name__=="__main__": main()
