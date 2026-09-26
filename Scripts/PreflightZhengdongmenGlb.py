"""Plain-Python GLB preflight for the Zhengdongmen parts, before any UE writer runs.

Checks the container length, that every node transform is identity, that each GLB
carries exactly one mesh and one material, and computes the per-part and union
footprint from the POSITION accessor bounds. That last number is what the landmark
validator compares against the declared envelope, so a geometry change that alters
the building's height has to be visible here first.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
PACKAGE = PROJECT / 'Raw3DPacket/Zhengdongmen/prepared'
GROUPS = ['Stone', 'Wood', 'RoofTile', 'Plaster', 'Iron', 'DoorWood', 'Sign']


def read_glb(path: Path):
    blob = path.read_bytes()
    magic, version, length = struct.unpack('<III', blob[:12])
    assert magic == 0x46546C67, f'{path.name}: not a GLB'
    assert length == len(blob), f'{path.name}: header length {length} != file {len(blob)}'
    offset, chunks = 12, {}
    while offset < length:
        size, kind = struct.unpack('<II', blob[offset:offset + 8])
        chunks[kind] = blob[offset + 8:offset + 8 + size]
        offset += 8 + size
    document = json.loads(chunks[0x4E4F534A].decode('utf-8'))
    binary = chunks.get(0x004E4942, b'')
    return document, binary


def accessor_bounds(document, index):
    accessor = document['accessors'][index]
    if 'min' in accessor and 'max' in accessor:
        return accessor['min'], accessor['max']
    raise AssertionError('accessor has no bounds')


def main():
    report = {'parts': {}, 'problems': []}
    low = [float('inf')] * 3
    high = [float('-inf')] * 3
    for group in GROUPS:
        path = PACKAGE / 'Meshes' / f'SM_ZDM_{group}.glb'
        document, binary = read_glb(path)
        entry = {'bytes': path.stat().st_size,
                 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()[:16],
                 'meshes': len(document.get('meshes', [])),
                 'materials': len(document.get('materials', [])),
                 'nodes': len(document.get('nodes', []))}
        if entry['meshes'] != 1:
            report['problems'].append(f'{group}: {entry["meshes"]} meshes, expected 1')
        if entry['materials'] != 1:
            report['problems'].append(f'{group}: {entry["materials"]} materials, expected 1')
        for node in document.get('nodes', []):
            if any(key in node for key in ('matrix', 'translation', 'rotation', 'scale')):
                report['problems'].append(f'{group}: node transform is not identity')
                break
        primitive = document['meshes'][0]['primitives'][0]
        if 'TEXCOORD_0' not in primitive.get('attributes', {}):
            report['problems'].append(f'{group}: no TEXCOORD_0')
        part_low, part_high = accessor_bounds(document, primitive['attributes']['POSITION'])
        entry['bounds_m'] = [[round(v, 4) for v in part_low],
                             [round(v, 4) for v in part_high]]
        entry['size_cm'] = [round((part_high[i] - part_low[i]) * 100, 2) for i in range(3)]
        entry['triangles'] = document['accessors'][primitive['indices']]['count'] // 3
        for axis in range(3):
            low[axis] = min(low[axis], part_low[axis])
            high[axis] = max(high[axis], part_high[axis])
        report['parts'][group] = entry

    report['union_cm'] = [round((high[i] - low[i]) * 100, 2) for i in range(3)]
    report['union_min_cm'] = [round(low[i] * 100, 2) for i in range(3)]
    report['union_max_cm'] = [round(high[i] * 100, 2) for i in range(3)]
    report['total_triangles'] = sum(p['triangles'] for p in report['parts'].values())

    tuned = json.loads((PACKAGE / 'reference-tuning.json').read_text())
    report['manifest_total_triangles'] = tuned['total_triangles']
    if report['total_triangles'] != tuned['total_triangles']:
        report['problems'].append(
            f'GLB total {report["total_triangles"]} != manifest {tuned["total_triangles"]}')
    for group, entry in report['parts'].items():
        want = tuned['groups'][group]
        if entry['triangles'] != want:
            report['problems'].append(
                f'{group}: GLB {entry["triangles"]} triangles != manifest {want}')
    report['passed'] = not report['problems']

    out = PROJECT / 'Saved/Reports/Zhengdongmen/glb-preflight.json'
    out.write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
