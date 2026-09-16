"""Remove two disconnected low roof fragments embedded in the gate opening.

The source roof mesh contains two isolated 216-vertex pieces at z=2.44-3.30 m;
they render as a floating teal slab across the lower arch. The source GLB is
left untouched. Degenerate triangles preserve buffer layout while removing only
those two disconnected fragments from a derived asset.
"""
import hashlib
import json
import struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'Saved/RawModelImport/V3/XiaobeiMen_UE5_Production_Package/Meshes/SM_XiaobeiMen_Gate_Nanite.glb'
output_dir = ROOT / 'Saved/GateDetailFix'
output_dir.mkdir(exist_ok=True)
output = output_dir / 'RoofClean.glb'
b = source.read_bytes()
json_length = struct.unpack_from('<I', b, 12)[0]
document = json.loads(b[20:20 + json_length])
binary = bytearray(b[28 + json_length:])


def read_accessor(index):
    accessor = document['accessors'][index]
    view = document['bufferViews'][accessor['bufferView']]
    count = accessor['count'] * ({'VEC3': 3, 'SCALAR': 1}[accessor['type']])
    dtype = '<f4' if accessor['componentType'] == 5126 else '<u4'
    offset = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
    return np.frombuffer(binary, dtype=dtype, count=count, offset=offset).reshape(accessor['count'], -1)


primitive = document['meshes'][5]['primitives'][0]
positions = read_accessor(primitive['attributes']['POSITION'])
indices = read_accessor(primitive['indices']).reshape(-1, 3)
parent = list(range(len(positions)))


def root(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i


for triangle in indices:
    a, b0, c = [int(v) for v in triangle]
    parent[root(b0)] = root(a)
    parent[root(c)] = root(a)
groups = {}
for i in range(len(positions)):
    groups.setdefault(root(i), []).append(i)
low_groups = [ids for ids in groups.values() if positions[ids, 2].max() < 4.0]
low_vertices = {i for ids in low_groups for i in ids}
assert len(low_groups) == 2 and len(low_vertices) == 432, (len(low_groups), len(low_vertices))
mask = np.array([any(int(v) in low_vertices for v in triangle) for triangle in indices])
assert int(mask.sum()) == 856, int(mask.sum())
indices[mask] = 0

accessor = document['accessors'][primitive['indices']]
view = document['bufferViews'][accessor['bufferView']]
offset = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
binary[offset:offset + view['byteLength']] = indices.astype('<u4').tobytes()
document['meshes'][5]['name'] = 'SM_XiaobeiMen_RoofTile_Green_LOD0_RoofClean'
roof_mesh = document['meshes'][5]
document['meshes'] = [roof_mesh]
document['nodes'] = [{'name': roof_mesh['name'], 'mesh': 0}]
document['scenes'] = [{'nodes': [0]}]
document['scene'] = 0
document['accessors'][primitive['indices']]['min'] = [0]

json_blob = json.dumps(document, separators=(',', ':')).encode('utf-8')
json_blob += b' ' * ((-len(json_blob)) % 4)
binary_blob = bytes(binary)
glb = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(json_blob) + 8 + len(binary_blob))
glb += struct.pack('<II', len(json_blob), 0x4E4F534A) + json_blob
glb += struct.pack('<II', len(binary_blob), 0x004E4942) + binary_blob
output.write_bytes(glb)
report = {
    'source': str(source),
    'source_sha256': hashlib.sha256(b).hexdigest(),
    'derived': str(output),
    'removed_components': len(low_groups),
    'removed_vertices': len(low_vertices),
    'removed_triangles': int(mask.sum()),
    'preserved_non_roof_geometry': True,
}
(output_dir / 'roof-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
