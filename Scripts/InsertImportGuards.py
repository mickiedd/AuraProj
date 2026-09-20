"""Insert a hard import-memory guard into every script that bakes meshes on import.

Six scripts in the project set `bake_meshes = True`. Two of them need it (Interchange ignores
the import rotation otherwise), but none of them checks what baking would materialise — and on
a shared-mesh instanced source, baking writes every node out as its own mesh. The preflight
numbers are brutal:

    GreatSouthGate_Zhengnanmen_UE5_HighPoly      2,449 nodes /  16 geoms  ->  ~2791 GB baked
    GreatSouthGate_Zhengnanmen_UE5_HighFidelity 12,196 nodes / 112 geoms  ->   ~448 GB baked
    Xiaobeimen_SmallNorthGate_..._500M_Instanced 1,552 nodes /   6 geoms  ->   ~183 GB baked
    Guidemen_GuideGate_UE5_100M_Instanced       18,823 nodes /  48 geoms  ->   ~148 GB baked

against a 97.8 GB commit limit. The Guidemen import was never going to fit; it reached 41.8 GB
and died with "The paging file is too small for this operation to complete".

This inserts a self-contained guard immediately before each `bake_meshes = True` assignment.
The guard reads the source GLB's JSON chunk, computes exactly what the import would
materialise under the requested setting, and refuses if it exceeds a 24 GB budget — with the
triangles, the gigabytes, and the fix in the message. It prints the figures on success so a
large-but-legal import is visible rather than silent.

The guard is inlined rather than imported because these scripts are executed by Unreal's
script runner, where `__file__` and sibling-module imports are not dependable.

Run this once. Re-running it is a no-op because the marker is checked first.
"""
from __future__ import annotations

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SCRIPTS = PROJECT / "Scripts"

TARGETS = {
    "ImportAndPlaceGreatWestGate.py": "str(SOURCE)",
    "ImportGreatNorthGateDabeiMen.py": "str(SOURCE)",
    "ImportGreatNorthGateHighDetail.py": "str(SOURCE)",
    "ImportGreatSouthGateZhengnanmen.py": "str(SOURCE)",
    "ImportGuidemen.py": "str(SOURCE)",
    "ImportGuangzhouLandmarks.py": "str(matches[0])",
}

MARKER = "_import_guard("

GUARD = '''
# --- import memory guard ---------------------------------------------------------------
# A bake on a shared-mesh instanced source writes every node out as its own mesh. Guidemen
# would have materialised ~148 GB that way and crashed the editor with an out-of-memory
# fatal error. See Scripts/ImportPreflight.py for the whole project's risk table.
def _import_guard(path, bake, budget_gb=24.0):
    import json as _json, struct as _struct
    with open(path, 'rb') as _handle:
        _magic, _, _length = _struct.unpack('<III', _handle.read(12))
        _doc = None
        while _handle.tell() < _length:
            _chunk_length, _chunk_type = _struct.unpack('<II', _handle.read(8))
            _payload = _handle.read(_chunk_length)
            if _chunk_type == 0x4E4F534A and _doc is None:
                _doc = _json.loads(_payload.decode('utf-8'))
    _unique = 0
    for _mesh in _doc.get('meshes', []):
        for _primitive in _mesh.get('primitives', []):
            if 'indices' in _primitive:
                _unique += _doc['accessors'][_primitive['indices']]['count'] // 3
    _baked = 0
    for _node in _doc.get('nodes', []):
        if 'mesh' in _node:
            for _primitive in _doc['meshes'][_node['mesh']].get('primitives', []):
                if 'indices' in _primitive:
                    _baked += _doc['accessors'][_primitive['indices']]['count'] // 3
    _materialised = _baked if bake else _unique
    _gb = _materialised * 220.0 / (1024 ** 3)
    _nodes = len([_node for _node in _doc.get('nodes', []) if 'mesh' in _node])
    _geoms = max(1, len(_doc.get('meshes', [])))
    assert _gb <= budget_gb, (
        'refusing import of {}: bake_meshes={} would materialise {:,} triangles (~{:.1f} GB) '
        'across {} nodes and {} geometries, over the {:.0f} GB budget. Use bake_meshes=False '
        'and keep instancing, or split the source. See Scripts/ImportPreflight.py'.format(
            path, bake, _materialised, _gb, _nodes, _geoms, budget_gb))
    print('IMPORT_GUARD ok: {:,} unique / {:,} materialised triangles (~{:.2f} GB), '
          'bake={}'.format(_unique, _materialised, _gb, bake))

_import_guard(__SOURCE__, True)
# --- end import memory guard ------------------------------------------------------------
'''


def main():
    changed, skipped = [], []
    for name, source_expression in TARGETS.items():
        path = SCRIPTS / name
        if not path.exists():
            skipped.append((name, "missing"))
            continue
        text = path.read_text(encoding="utf-8")
        if MARKER in text:
            skipped.append((name, "already guarded"))
            continue
        needle = "    pipeline.common_meshes_properties.bake_meshes = True"
        if needle not in text:
            needle = "pipeline.common_meshes_properties.bake_meshes = True"
        if needle not in text:
            skipped.append((name, "bake_meshes assignment not found"))
            continue
        indent = needle[:len(needle) - len(needle.lstrip())]
        block = "\n".join(
            (indent + line) if line.strip() else line
            for line in GUARD.replace("__SOURCE__", source_expression).strip("\n").split("\n")
        )
        replacement = block + "\n" + needle
        text = text.replace(needle, replacement, 1)
        path.write_text(text, encoding="utf-8")
        changed.append(name)
        print(f"GUARD_INSERTED {name}  ->  _import_guard({source_expression}, True)")
    print()
    print("GUARD_CHANGED", len(changed), changed)
    print("GUARD_SKIPPED", len(skipped), skipped)


if __name__ == "__main__":
    main()
