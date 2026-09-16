"""Inventory the V3 archives and prepare UVs for the UV-less South Gate GLBs.

Run with system Python (numpy required), before ImportV3Buildings.py in Unreal.
Original packages are never edited. Derived meshes retain every source triangle.
"""
import hashlib
import json
from pathlib import Path
import struct
import zipfile

import numpy as np
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / 'Saved/RawModelImport/V3'
SOURCE = Path('C:/Works/Raw3DModels/V3')
PACKAGES = [
    ('Xiaobeimen_SmallNorthGate_UE5_AAA.zip', 'Xiaobeimen_SmallNorthGate_UE5_AAA', 'Xiaobeimen_AAA_V3', 'Models/Full/Xiaobeimen_HP/Xiaobeimen_HP.gltf'),
    ('XiaobeiMen_UE5_Production_Package.zip', 'XiaobeiMen_UE5_Production_Package', 'Xiaobeimen_Production_V3', 'Meshes/SM_XiaobeiMen_Gate_Nanite.glb'),
    ('Zhengnanmen_UE5_AAA_Package.zip', 'Zhengnanmen_UE5_AAA', 'Zhengnanmen_AAA_V3', 'Meshes/SM_Zhengnanmen_HighPoly.glb'),
]


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def gltf(path):
    if path.suffix == '.gltf':
        return json.loads(path.read_text(encoding='utf-8')), None
    b = path.read_bytes()
    size = struct.unpack_from('<I', b, 12)[0]
    return json.loads(b[20:20 + size]), b[28 + size:]


def project_uvs(src, dst):
    d, blob = gltf(src)
    out = bytearray()
    views, accessors = [], []
    types = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: 'u1'}

    def read(index):
        a = d['accessors'][index]
        v = d['bufferViews'][a['bufferView']]
        width = {'SCALAR': 1, 'VEC3': 3}[a['type']]
        dtype = np.dtype(types[a['componentType']])
        offset = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        return np.ndarray((a['count'], width), dtype=dtype, buffer=blob,
                          offset=offset, strides=(v.get('byteStride', width * dtype.itemsize), dtype.itemsize)).copy()

    def add(a, kind):
        a = np.ascontiguousarray(a, dtype='<f4')
        while len(out) % 4:
            out.append(0)
        views.append({'buffer': 0, 'byteOffset': len(out), 'byteLength': a.nbytes, 'target': 34962})
        out.extend(a.tobytes())
        accessors.append({'bufferView': len(views) - 1, 'componentType': 5126,
                          'count': len(a), 'type': kind,
                          'min': a.min(axis=0).tolist(), 'max': a.max(axis=0).tolist()})
        return len(accessors) - 1

    triangles = 0
    scales = {'M_RoofGlaze': .35, 'M_RedWood': .55, 'M_Stone': 1.2,
              'M_DoorWood': .65, 'M_GoldBronze': .25, 'M_Moss': .45}
    for m in d['meshes']:
        for p in m['primitives']:
            if 'material' not in p:
                continue  # collision sources are not processed
            assert set(p['attributes']) == {'POSITION'}, 'Unexpected attributes: avoid losing source data'
            pos = read(p['attributes']['POSITION'])
            ids = read(p['indices']).reshape(-1)
            verts = pos[ids]
            tri = verts.reshape(-1, 3, 3)
            normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
            axes = np.repeat(np.argmax(np.abs(normals), axis=1), 3)
            material = d['materials'][p['material']]['name']
            uv = np.zeros((len(verts), 2), dtype='<f4')
            if material == 'M_Signboard':
                lo, hi = pos.min(axis=0), pos.max(axis=0)
                uv[:, 0] = (verts[:, 0] - lo[0]) / (hi[0] - lo[0])
                uv[:, 1] = 1 - (verts[:, 2] - lo[2]) / (hi[2] - lo[2])
            else:
                for axis, pair in enumerate(((1, 2), (0, 2), (0, 1))):
                    pick = axes == axis
                    uv[pick] = verts[pick][:, pair] / scales[material]
                uv[:, 1] *= -1
            # Per-triangle projected UVs avoid seams stretching across hard corners.
            assert np.array_equal(verts, pos[ids])
            p['attributes'] = {'POSITION': add(verts, 'VEC3'), 'TEXCOORD_0': add(uv, 'VEC2')}
            del p['indices']
            triangles += len(tri)
    d['bufferViews'], d['accessors'] = views, accessors
    d['buffers'] = [{'byteLength': len(out)}]
    js = json.dumps(d, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    out.extend(b'\0' * (-len(out) % 4))
    result = struct.pack('<III', 0x46546c67, 2, 28 + len(js) + len(out))
    result += struct.pack('<II', len(js), 0x4e4f534a) + js
    result += struct.pack('<II', len(out), 0x004e4942) + out
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(result)
    return triangles


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    baseline = ROOT / 'existing-assets-before.json'
    if not baseline.exists():
        paths = list((PROJECT / 'Content/Assets/Environment/GuangzhouLandmarks').rglob('*.uasset'))
        paths += list((PROJECT / 'Content').rglob('*.umap'))
        baseline.write_text(json.dumps({str(p.relative_to(PROJECT)): sha(p) for p in paths}, indent=2))
    configs = []
    for zipname, folder, name, primary in PACKAGES:
        archive = SOURCE / zipname
        if not (ROOT / folder).exists():
            with zipfile.ZipFile(archive) as z:
                for member in z.infolist():
                    assert (ROOT / member.filename).resolve().is_relative_to(ROOT.resolve())
                z.extractall(ROOT)
        package = ROOT / folder
        repairs = {}
        if name == 'Xiaobeimen_AAA_V3':
            # The supplied Moss ORM PNG is truncated mid-IDAT. Recover its
            # packed data from the complete, independently supplied channels.
            rel = 'Textures/Moss/Moss_ORM_4K.png'
            repaired = ROOT / 'Prepared' / folder / rel
            repaired.parent.mkdir(parents=True, exist_ok=True)
            channels = [Image.open(package / ('Textures/Moss/Moss_' + c + '_4K.png')).convert('L')
                        for c in ('AO', 'Roughness', 'Metallic')]
            assert all(c.size == (4096, 4096) for c in channels)
            Image.merge('RGB', channels).save(repaired)
            check = np.asarray(Image.open(repaired))
            assert all(np.array_equal(check[:, :, i], np.asarray(c)) for i, c in enumerate(channels))
            repairs[rel] = {'source': str(repaired), 'reason': 'Original PNG truncated mid-IDAT; lossless R=AO G=Roughness B=Metallic reconstruction',
                            'sha256': sha(repaired)}
        records, meshes = [], []
        for p in sorted(package.rglob('*')):
            if not p.is_file():
                continue
            rel = p.relative_to(package).as_posix()
            records.append({'file': rel, 'bytes': p.stat().st_size, 'sha256': sha(p)})
            if p.suffix not in ('.gltf', '.glb'):
                continue
            d, _ = gltf(p)
            import_path = p
            uv_triangles = None
            if name == 'Zhengnanmen_AAA_V3' and 'Collision' not in rel:
                import_path = ROOT / 'Prepared' / folder / rel
                uv_triangles = project_uvs(p, import_path)
            triangles = sum((d['accessors'][q['indices']]['count'] if 'indices' in q else d['accessors'][q['attributes']['POSITION']]['count']) // 3 for m in d['meshes'] for q in m['primitives'])
            if uv_triangles is not None:
                assert uv_triangles == triangles
            meshes.append({'relative': rel, 'source': str(import_path), 'stem': p.stem,
                           'mesh_count': len(d['meshes']), 'instance_count': sum('mesh' in n for n in d['nodes']),
                           'triangles': triangles, 'primary': rel == primary,
                           'collision': 'Collision' in rel, 'projected_uvs': uv_triangles is not None})
        cfg = {'name': name, 'root': str(package), 'archive': str(archive), 'archive_sha256': sha(archive),
               'destination': '/Game/Assets/Environment/GuangzhouLandmarks/V3/' + name,
               'meshes': meshes, 'files': records, 'texture_repairs': repairs}
        configs.append(cfg)
        print(name, len(meshes), 'mesh sources;', len(records), 'source files')
    (ROOT / 'packages.json').write_text(json.dumps(configs, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
