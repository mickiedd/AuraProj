"""Consolidate Xiaobeimen OBJ groups by material without changing geometry/UVs.
Run with regular Python before ImportXiaobeimen.py in Unreal Editor.
The original ZIP and extracted source files remain untouched.
"""
from pathlib import Path
from collections import defaultdict
import json
root=Path('C:/Works/Raw3DModels/Extracted/Xiaobeimen_SmallNorthGate_Unreal_v3_Textured/Xiaobeimen_SmallNorthGate_Unreal_v3_Textured')
source=root/'SM_Xiaobeimen_SmallNorthGate_Textured.obj'
out=root/'SM_Xiaobeimen_Combined.obj'
attrs=[]; faces=defaultdict(list); material=None
counts=[0,0,0]
for line in source.read_text().splitlines(True):
    kind=line.split(' ',1)[0]
    if kind in ('v','vt','vn'):
        attrs.append(line);counts[('v','vt','vn').index(kind)]+=1
    elif kind=='usemtl':material=line.split()[1]
    elif kind=='f':
        assert material
        # Resolve relative indices before reordering the attribute/face blocks.
        refs=[]
        for vertex in line.split()[1:]:
            parts=vertex.split('/')
            refs.append('/'.join(str(int(n) if int(n)>0 else counts[i]+int(n)+1) if n else '' for i,n in enumerate(parts)))
        faces[material].append('f '+' '.join(refs)+'\n')
with out.open('w') as f:
    f.write('# Derived from supplied OBJ: cm, Z-up; groups merged by material.\n')
    f.writelines(attrs)
    for mat,fs in faces.items():
        f.write('o '+mat+'\nusemtl '+mat+'\n');f.writelines(fs)
print(json.dumps({'output':str(out),'attribute_counts':counts,'material_groups':len(faces),'faces':sum(map(len,faces.values()))}))
