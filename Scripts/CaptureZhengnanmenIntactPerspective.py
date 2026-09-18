"""Capture one perspective angle on the exact V2 gate without changing maps.

Execute wrappers with INTACT_VIEW and INTACT_STAGE globals. Local lighting is
placed on the camera's side; the existing capture utility handles cleanup.
"""
from pathlib import Path
import math
ROOT=Path(__file__).resolve().parents[1]
VIEW=globals().get('INTACT_VIEW','hero')
STAGE=globals().get('INTACT_STAGE','after')
views={
 'hero':((0,0,1160),(2500,4860,1350)),
 'rear':((0,0,1160),(-2500,-4860,1350)),
 'left':((0,0,1160),(-4860,2500,1000)),
 'right':((0,0,1160),(4860,-2500,1000)),
 'under-eaves':((0,600,1240),(1800,3300,-160)),
 'roof':((0,0,1700),(1500,3200,1900)),
 'arch':((0,600,345),(450,1700,180)),
 'floor-close':((400,600,1460),(800,1600,80)),
}
target,offset=views[VIEW]
dx,dy=offset[:2];length=math.hypot(dx,dy);dx/=length;dy/=length
path=ROOT/'Scripts/CaptureZhengnanmenManual4K.py'
code=path.read_text().replace('    capture=actors.spawn_actor_from_class',f'    target=unreal.Vector({target[0]},{target[1]},{100000+target[2]})\n    offset=unreal.Vector{offset}\n    capture=actors.spawn_actor_from_class')
code=code.replace('unreal.Vector(w*.5,w,h*.8)',f'unreal.Vector(w*{dx},w*{dy},h*.8)').replace('unreal.Vector(-w*.8,w*.4,h*.5)',f'unreal.Vector(w*{dx-.7*dy},w*{dy+.7*dx},h*.4)')
exec(compile(code,str(path),'exec'),{'__file__':str(path),'CAPTURE_STAGE':'intact-'+STAGE,'CAPTURE_VIEW':VIEW})
