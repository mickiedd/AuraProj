"""Measure landmark mesh geometry from the OBJs exported by ExportLandmarkMeshes.

Runs outside the editor, where numpy is available. For each exported mesh it
reports the things that make a vertex "not make sense":

  boundary_edges      edges used by only one triangle -> the surface is open
  nonmanifold_edges   edges used by more than two triangles
  winding_flipped     edges traversed the same way twice -> inconsistent facing
  signed_volume_cm3   negative on a closed solid -> the whole solid is inside-out
  degenerate_faces    zero-area triangles or faces with repeated corners
  duplicate_faces     the same triangle present more than once
  nonfinite_vertices  NaN or infinite coordinates

OBJ vertices are split at UV and normal seams, so positions are welded before any
topology is computed; otherwise every seam reads as a boundary and every mesh
looks open.

Each OBJ is deleted after it is measured, so only one batch occupies disk at a
time.
"""

import json
import sys
from collections import Counter
from pathlib import Path

import numpy as np

AUDIT_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit")
OBJ_DIR = AUDIT_DIR / "obj"
REPORT = AUDIT_DIR / "geometry-audit.json"

# Optional first argument: measure a different folder and write a different
# report. Used to re-measure the repaired meshes after import without disturbing
# the original audit.
if len(sys.argv) > 1:
    OBJ_DIR = Path(sys.argv[1])
    REPORT = OBJ_DIR.parent / (OBJ_DIR.name + "-audit.json")

WELD_DECIMALS = 4
DEGENERATE_AREA_EPS = 1e-9


def parse_obj(path):
    vertices = []
    faces = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("v "):
                parts = line.split()
                vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
            elif line.startswith("f "):
                parts = line.split()[1:]
                corners = []
                for token in parts:
                    index = int(token.split("/")[0])
                    corners.append(index - 1 if index > 0 else len(vertices) + index)
                # Fan-triangulate quads and n-gons.
                for i in range(1, len(corners) - 1):
                    faces.append((corners[0], corners[i], corners[i + 1]))
    return np.asarray(vertices, dtype=np.float64), np.asarray(faces, dtype=np.int64)


def weld(vertices, faces):
    """Merge vertices that share a position, returning the remapped faces."""
    if len(vertices) == 0:
        return vertices, faces
    rounded = np.round(vertices, WELD_DECIMALS)
    unique, inverse = np.unique(rounded, axis=0, return_inverse=True)
    return unique, inverse[faces]


def analyse(vertices, faces):
    report = {}
    report["vertices_raw"] = int(len(vertices))
    report["faces"] = int(len(faces))

    nonfinite = int(np.count_nonzero(~np.isfinite(vertices)))
    report["nonfinite_vertices"] = nonfinite
    if nonfinite:
        vertices = np.nan_to_num(vertices)

    if len(faces) == 0:
        report["error"] = "no faces"
        return report

    welded, remapped = weld(vertices, faces)
    report["vertices_welded"] = int(len(welded))

    # Degenerate faces: repeated corner after welding, or zero area.
    repeated = ((remapped[:, 0] == remapped[:, 1])
                | (remapped[:, 1] == remapped[:, 2])
                | (remapped[:, 2] == remapped[:, 0]))
    a = welded[remapped[:, 0]]
    b = welded[remapped[:, 1]]
    c = welded[remapped[:, 2]]
    cross = np.cross(b - a, c - a)
    area = np.linalg.norm(cross, axis=1) * 0.5
    zero_area = area <= DEGENERATE_AREA_EPS
    report["degenerate_faces"] = int(np.count_nonzero(repeated | zero_area))
    report["degenerate_by_repeat"] = int(np.count_nonzero(repeated))
    report["degenerate_by_zero_area"] = int(np.count_nonzero(zero_area))

    good = ~(repeated | zero_area)
    report["good_faces"] = int(np.count_nonzero(good))
    if report["good_faces"] == 0:
        report["error"] = "all faces degenerate"
        return report

    # Duplicate faces (as sorted corner triples).
    sorted_faces = np.sort(remapped[good], axis=1)
    unique_faces, counts = np.unique(sorted_faces, axis=0, return_counts=True)
    report["duplicate_face_groups"] = int(np.count_nonzero(counts > 1))
    report["duplicate_face_excess"] = int(np.sum(counts - 1))

    # Signed volume of the welded surface (divergence theorem).
    va = welded[remapped[good, 0]]
    vb = welded[remapped[good, 1]]
    vc = welded[remapped[good, 2]]
    report["signed_volume_cm3"] = float(np.sum(np.einsum("ij,ij->i", va,
                                                         np.cross(vb, vc))) / 6.0)

    # Edge topology over welded positions.
    tri = remapped[good]
    edges = np.concatenate([
        np.stack([tri[:, 0], tri[:, 1]], axis=1),
        np.stack([tri[:, 1], tri[:, 2]], axis=1),
        np.stack([tri[:, 2], tri[:, 0]], axis=1),
    ])
    low = np.minimum(edges[:, 0], edges[:, 1])
    high = np.maximum(edges[:, 0], edges[:, 1])
    keys = np.stack([low, high], axis=1)
    unique_keys, key_counts = np.unique(keys, axis=0, return_counts=True)
    report["edges_total"] = int(len(unique_keys))
    report["boundary_edges"] = int(np.count_nonzero(key_counts == 1))
    report["nonmanifold_edges"] = int(np.count_nonzero(key_counts > 2))

    # Winding consistency: on a correctly wound closed surface every edge is
    # traversed once in each direction.
    forward = keys[(edges[:, 0] == low) & (edges[:, 1] == high)]
    backward = keys[(edges[:, 0] == high) & (edges[:, 1] == low)]
    forward_counts = Counter(map(tuple, forward))
    backward_counts = Counter(map(tuple, backward))
    unbalanced = 0
    for key in set(forward_counts) | set(backward_counts):
        if forward_counts.get(key, 0) != backward_counts.get(key, 0):
            unbalanced += 1
    report["winding_unbalanced_edges"] = int(unbalanced)

    report["closed"] = (report["boundary_edges"] == 0
                        and report["nonmanifold_edges"] == 0)
    report["winding_consistent"] = unbalanced == 0
    report["inward_wound_solid"] = bool(report["closed"]
                                        and report["signed_volume_cm3"] < 0)
    lo = welded.min(axis=0)
    hi = welded.max(axis=0)
    report["bounds_cm"] = [[round(float(v), 2) for v in lo],
                           [round(float(v), 2) for v in hi]]
    report["size_cm"] = [round(float(v), 2) for v in (hi - lo)]
    return report


def main():
    if not OBJ_DIR.exists():
        print("no obj dir", OBJ_DIR)
        return 1
    existing = {}
    if REPORT.exists():
        existing = json.loads(REPORT.read_text(encoding="utf-8"))

    objs = sorted(OBJ_DIR.glob("*.obj"))
    if not objs:
        print("MESH_AUDIT_BATCH none pending")
        return 0

    # Skip meshes already measured so repeated runs are cheap.
    pending = [obj for obj in objs
               if existing.get(obj.stem, {}).get("status") != "ok"]
    skipped = len(objs) - len(pending)
    print("MESH_AUDIT_PENDING", len(pending), "already_measured", skipped)

    for obj in pending:
        try:
            vertices, faces = parse_obj(obj)
            result = analyse(vertices, faces)
            result["status"] = "ok"
        except Exception as exc:
            result = {"status": "failed", "error": str(exc)}
        result["obj"] = obj.name
        result["obj_bytes"] = obj.stat().st_size
        existing[obj.stem] = result
        print("MESH_AUDIT", json.dumps({k: v for k, v in result.items()
                                        if k != "bounds_cm"}))
        # The exported OBJs are deliberately left on disk. This environment has a
        # bulk-delete guard, and the audit does not need them removed; the folder
        # is under Saved/ and is reported to the user instead.

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(existing, indent=2, sort_keys=True), encoding="utf-8")

    measured = [entry for entry in existing.values() if entry.get("status") == "ok"]
    print("MESH_AUDIT_TOTAL", len(existing))
    print("MESH_AUDIT_MEASURED", len(measured))
    print("MESH_AUDIT_OPEN", sum(1 for e in measured if not e["closed"]))
    print("MESH_AUDIT_INWARD", sum(1 for e in measured if e["inward_wound_solid"]))
    print("MESH_AUDIT_WINDING_BAD", sum(1 for e in measured if not e["winding_consistent"]))
    print("MESH_AUDIT_DEGENERATE", sum(1 for e in measured if e["degenerate_faces"]))
    print("MESH_AUDIT_NONMANIFOLD", sum(1 for e in measured if e["nonmanifold_edges"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
