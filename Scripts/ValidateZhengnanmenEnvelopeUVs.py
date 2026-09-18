"""Check imported UV0 corner values against source glTF UVs, including seams."""
import json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/RawModelImport/ZhengnanmenIntactPerspective'
manifest=json.loads((ROOT/'ContentSource/GuangzhouLandmarks/Zhengnanmen_IntactEnvelope_20260916/envelope-manifest.json').read_text())
rows=[]
for e in manifest['entries']:
    source=np.load(e['geometry']);points=source['vertices_cm'];expected=source['uv'].copy();expected[:,1]=1-expected[:,1]
    # These imports use UE's default half-precision UV render buffer. Account
    # for its quantization before the OBJ exporter flips V back for the file.
    packed_expected=source['uv'].astype(np.float16).astype(float);packed_expected[:,1]=1-packed_expected[:,1]
    vertices=[];uvs=[];corners=[]
    for line in (OUT/'Imported'/(e['mesh_name']+'.obj')).read_text().splitlines():
        fields=line.split()
        if not fields:continue
        if fields[0]=='v':vertices.append([float(v) for v in fields[1:4]])
        elif fields[0]=='vt':uvs.append([float(v) for v in fields[1:3]])
        elif fields[0]=='f':corners.extend(tuple(int(i)-1 for i in v.split('/')[:2]) for v in fields[1:])
    vertices=np.array(vertices);uvs=np.array(uvs);corners=np.unique(corners,axis=0)
    assert len(corners)>0
    tree=cKDTree(points);neighbors=tree.query_ball_point(vertices[corners[:,0]],.005)
    worst=0;packed_worst=0
    for (vi,ui),indices in zip(corners,neighbors):
        assert indices,(e['mesh_name'],'Missing original position')
        error=np.min(np.max(abs(expected[indices]-uvs[ui]),axis=1));worst=max(worst,float(error))
        packed_error=np.min(np.max(abs(packed_expected[indices]-uvs[ui]),axis=1));packed_worst=max(packed_worst,float(packed_error))
        assert packed_error<.00001,(e['mesh_name'],'UV0 orientation changed beyond render-buffer quantization',packed_error)
    rows.append(dict(mesh=e['mesh_name'],corners=len(corners),max_uv_error=worst,max_error_after_half_precision_quantization=packed_worst,original_uv0_present=e['original_uv0_present']))
assert len(rows)==101
(OUT/'validate-uv.json').write_text(json.dumps(dict(passed=True,meshes=101,corners=sum(r['corners'] for r in rows),rows=rows,comparison='Undo OBJ exporter V flip; imported UE UV0 equals glTF UV0 for original surfaces, with per-face generated UVs only on UV-less source parts'),indent=2))
print('IMPORTED_ENVELOPE_UV0_VALIDATED',101)
