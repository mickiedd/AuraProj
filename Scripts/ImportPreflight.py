"""Preflight every glTF/GLB source in the project for import memory risk.

Run this BEFORE importing any GLB into Unreal. It exists because a single import crashed the
shared editor with a fatal out-of-memory error: the Guidemen source is 74.8 MB on disk but
declares **169,891,784 expanded triangles against only 38,276 unique**, and the import ran with
`bake_meshes = True`, which writes every one of the 18,823 nodes out as its own mesh. The editor
reached 41.8 GB and then failed with "The paging file is too small for this operation to
complete".

For every GLB it reports the node, geometry and triangle counts from the JSON chunk alone — no
geometry decoding, so it is fast even on a 200 MB file — then computes what an import would
materialise under each baking setting and gives a verdict:

    SAFE            bake_meshes either way stays inside budget
    BAKE-FORBIDDEN  only safe with bake_meshes = False; instancing must be preserved
    LARGE           heavy but within budget if the editor is freshly started and idle
    REFUSE          exceeds the safe budget under either setting

It also reports the machine's commit headroom, because the failure mode is the commit limit,
not physical RAM.

Read-only. Runnable with the plain interpreter; needs no Unreal.
"""
from __future__ import annotations

import ctypes
import json
import struct
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SEARCH_ROOTS = [
    Path("C:/Works/Raw3DModels"),
    PROJECT / "ContentSource",
    PROJECT / "Saved/RawModelImport",
]
OUTPUT = PROJECT / "Saved/RawModelImport/import-preflight.json"

# Keep a single import well inside the commit limit. The editor itself holds several GB, and
# other agents may be running, so the budget is deliberately conservative.
IMPORT_BUDGET_GB = 24.0
# Rough bytes of working memory per materialised triangle during an Interchange import
# (source vertices, index buffer, UVs/normals/tangents, plus the build copies).
BYTES_PER_TRIANGLE = 220.0


def _read_glb_json(path: Path) -> dict:
    with path.open("rb") as handle:
        magic, _, length = struct.unpack("<III", handle.read(12))
        assert magic == 0x46546C67, f"{path} is not a GLB"
        document = None
        while handle.tell() < length:
            chunk_length, chunk_type = struct.unpack("<II", handle.read(8))
            payload = handle.read(chunk_length)
            if chunk_type == 0x4E4F534A and document is None:
                document = json.loads(payload.decode("utf-8"))
    assert document is not None, path
    return document


def _memory() -> dict:
    class Memory(ctypes.Structure):
        _fields_ = [
            ("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong),
            ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
            ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
            ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
            ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
        ]

    value = Memory()
    value.dwLength = ctypes.sizeof(Memory)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(value))
    gb = 1024 ** 3
    return {
        "physical_total_gb": round(value.ullTotalPhys / gb, 2),
        "physical_available_gb": round(value.ullAvailPhys / gb, 2),
        "commit_limit_gb": round(value.ullTotalPageFile / gb, 2),
        "commit_available_gb": round(value.ullAvailPageFile / gb, 2),
        "page_file_gb": round((value.ullTotalPageFile - value.ullTotalPhys) / gb, 2),
        "memory_load_percent": int(value.dwMemoryLoad),
    }


def _analyse(path: Path) -> dict:
    document = _read_glb_json(path)
    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    accessors = document.get("accessors", [])

    unique_triangles = 0
    for mesh in meshes:
        for primitive in mesh.get("primitives", []):
            index_accessor = primitive.get("indices")
            if index_accessor is None:
                continue
            unique_triangles += accessors[index_accessor]["count"] // 3

    mesh_nodes = [node for node in nodes if "mesh" in node]
    instances = len(mesh_nodes)
    geometries = len(meshes)
    # with baking, every mesh-bearing node becomes its own mesh
    baked_triangles = unique_triangles * instances // max(1, geometries) * max(1, geometries)
    # without baking, only the geometry definitions are built, once each
    unbaked_triangles = unique_triangles

    baked_gb = baked_triangles * BYTES_PER_TRIANGLE / (1024 ** 3)
    unbaked_gb = unbaked_triangles * BYTES_PER_TRIANGLE / (1024 ** 3)

    if unbaked_gb > IMPORT_BUDGET_GB:
        verdict = "REFUSE"
        advice = "even without baking this exceeds the budget; split the source first"
    elif baked_gb > IMPORT_BUDGET_GB and instances > geometries:
        verdict = "BAKE-FORBIDDEN"
        advice = ("shared-mesh instanced: bake_meshes must be False, or the import will "
                  "materialise every instance as its own mesh")
    elif baked_gb > IMPORT_BUDGET_GB * 0.5:
        verdict = "LARGE"
        advice = "run only with a freshly started, idle editor and nothing else heavy open"
    else:
        verdict = "SAFE"
        advice = "either baking setting stays inside budget"

    return {
        "source": str(path),
        "bytes": path.stat().st_size,
        "nodes": len(nodes),
        "mesh_nodes": instances,
        "geometries": geometries,
        "materials": len(document.get("materials", [])),
        "unique_triangles": unique_triangles,
        "baked_triangles": baked_triangles,
        "unbaked_triangles": unbaked_triangles,
        "estimated_baked_gb": round(baked_gb, 2),
        "estimated_unbaked_gb": round(unbaked_gb, 3),
        "instancing_ratio": round(instances / max(1, geometries), 1),
        "verdict": verdict,
        "advice": advice,
    }


def main():
    memory = _memory()
    print("PREFLIGHT_MEMORY", json.dumps(memory))
    budget = min(IMPORT_BUDGET_GB, max(4.0, memory["commit_available_gb"] - 16.0))
    print(f"PREFLIGHT_BUDGET  import budget {budget:.1f} GB "
          f"(commit available {memory['commit_available_gb']:.1f} GB, keeping 16 GB headroom)")

    seen, results = set(), []
    for root in SEARCH_ROOTS:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*.glb")):
            if path in seen:
                continue
            seen.add(path)
            try:
                results.append(_analyse(path))
            except Exception as error:
                results.append({"source": str(path), "error": repr(error)[:140]})
    results.sort(key=lambda item: -item.get("baked_triangles", 0))

    print()
    print(f"{'verdict':16s} {'nodes':>7s} {'geoms':>6s} {'unique tris':>12s} "
          f"{'baked tris':>14s} {'baked GB':>9s}  source")
    for item in results:
        if "error" in item:
            print(f"{'ERROR':16s} {'':>7s} {'':>6s} {'':>12s} {'':>14s} {'':>9s}  "
                  f"{Path(item['source']).name}  {item['error']}")
            continue
        print(f"{item['verdict']:16s} {item['nodes']:7d} {item['geometries']:6d} "
              f"{item['unique_triangles']:12,d} {item['baked_triangles']:14,d} "
              f"{item['estimated_baked_gb']:9.2f}  {Path(item['source']).name}")
    print()
    for item in results:
        if item.get("verdict") in ("BAKE-FORBIDDEN", "REFUSE", "LARGE"):
            print(f"PREFLIGHT_ALERT {item['verdict']:14s} {Path(item['source']).name}: {item['advice']}")

    OUTPUT.write_text(json.dumps({"memory": memory, "budget_gb": budget, "sources": results},
                                 indent=1), encoding="utf-8")
    print("PREFLIGHT_OUTPUT", OUTPUT)
    return 0 if not any(item.get("verdict") in ("REFUSE",) for item in results) else 1


if __name__ == "__main__":
    sys.exit(main())
