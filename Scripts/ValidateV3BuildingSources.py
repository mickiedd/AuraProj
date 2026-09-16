"""Verify derived source geometry, regenerated texture channels and old asset hashes."""
import hashlib
import json
from pathlib import Path
import struct
import zlib

import numpy as np
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / 'Saved/RawModelImport/V3'


def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def load_glb(p):
    b = p.read_bytes()
    size = struct.unpack_from('<I', b, 12)[0]
    return json.loads(b[20:20+size]), b[28+size:]


def accessor(d, blob, i):
    a = d['accessors'][i]
    v = d['bufferViews'][a['bufferView']]
    width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[a['type']]
    dtype = np.dtype({5121: 'u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']])
    return np.ndarray((a['count'], width), dtype=dtype, buffer=blob,
        offset=v.get('byteOffset', 0)+a.get('byteOffset', 0),
        strides=(v.get('byteStride', dtype.itemsize*width), dtype.itemsize))


def png_valid(p):
    b = p.read_bytes()
    assert b[:8] == b'\x89PNG\r\n\x1a\n', str(p)
    i, ended = 8, False
    while i + 12 <= len(b):
        size = struct.unpack_from('>I', b, i)[0]
        assert i + 12 + size <= len(b), str(p)
        chunk = b[i+4:i+8+size]
        assert zlib.crc32(chunk) & 0xffffffff == struct.unpack_from('>I', b, i+8+size)[0], str(p)
        i += 12 + size
        if chunk[:4] == b'IEND':
            ended = True
            break
    assert ended and i == len(b), str(p)
    with Image.open(p) as im:
        im.load()


def main():
    configs = json.loads((ROOT / 'packages.json').read_text())
    derived, png_count = [], 0
    for cfg in configs:
        source_root = Path(cfg['root'])
        assert sha(Path(cfg['archive'])) == cfg['archive_sha256']
        for record in cfg['files']:
            src = source_root / record['file']
            assert sha(src) == record['sha256'], str(src)
            if src.suffix == '.png':
                repair = cfg.get('texture_repairs', {}).get(record['file'])
                png_valid(Path(repair['source']) if repair else src)
                png_count += 1
        for entry in cfg['meshes']:
            if not entry['projected_uvs']:
                continue
            a, ba = load_glb(source_root / entry['relative'])
            b, bb = load_glb(Path(entry['source']))
            assert a['nodes'] == b['nodes'] and a['scenes'] == b['scenes']
            assert a['materials'] == b['materials']
            assert len(a['meshes']) == len(b['meshes'])
            count = 0
            for ma, mb in zip(a['meshes'], b['meshes']):
                assert len(ma['primitives']) == len(mb['primitives'])
                for pa, pb in zip(ma['primitives'], mb['primitives']):
                    original = accessor(a, ba, pa['attributes']['POSITION'])
                    original = original[accessor(a, ba, pa['indices']).ravel()]
                    result = accessor(b, bb, pb['attributes']['POSITION'])
                    assert np.array_equal(original, result), entry['relative']
                    uv = accessor(b, bb, pb['attributes']['TEXCOORD_0'])
                    assert len(uv) == len(result) and np.isfinite(uv).all()
                    assert pa['material'] == pb['material']
                    count += len(result)//3
            assert count == entry['triangles']
            derived.append({'source': entry['relative'], 'triangles': count, 'all_positions_identical': True})
    cfg = configs[0]
    repaired = np.asarray(Image.open(next(iter(cfg['texture_repairs'].values()))['source']))
    for i, channel in enumerate(('AO', 'Roughness', 'Metallic')):
        original = np.asarray(Image.open(Path(cfg['root']) / ('Textures/Moss/Moss_' + channel + '_4K.png')).convert('L'))
        assert np.array_equal(repaired[:, :, i], original), channel
    baseline = json.loads((ROOT / 'existing-assets-before.json').read_text())
    changed = [name for name, digest in baseline.items() if not (PROJECT / name).is_file() or sha(PROJECT / name) != digest]
    assert not changed, 'Pre-existing assets changed: ' + str(changed)
    report = {'passed': True, 'source_files_unchanged': sum(len(c['files']) for c in configs),
              'pngs_validated': png_count, 'regenerated_textures': 1, 'orm_channels_match': True,
              'derived_geometry': derived, 'unchanged_existing_assets_and_maps': len(baseline)}
    (ROOT / 'source-validation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == '__main__':
    main()
