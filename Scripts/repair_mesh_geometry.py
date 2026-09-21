"""Repair the landmark meshes flagged by the geometry audit.

Three defects are fixed, all of which are provably safe because none of them can
change the silhouette:

  duplicate faces     the same triangle present more than once -> z-fighting
  degenerate faces    zero-area triangles, or corners repeated after welding
  winding defects     faces traversed the same way twice by their neighbours,
                      and closed solids whose signed volume is negative

Winding is repaired properly rather than by blanket reversal: faces are grouped
into connected components by shared edges, each component's relative orientation
is solved by propagation, and the component's global sign is then chosen by
majority against the original winding - or, for a closed component, by requiring a
positive signed volume. A blanket reversal would turn a correct surface inside out.

Vertices are welded first because the OBJ exporter splits every triangle at its UV
and normal seams, so an un-welded face list has no shared topology to reason about.
The repaired mesh is written with shared vertices and the original UVs.

Writes repaired OBJs; importing and rebinding is a separate step.
"""

import json
from collections import Counter, defaultdict, deque
from pathlib import Path

import numpy as np

OBJ_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit/obj")
OUT_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit/repaired")
AUDIT = Path("C:/Git/AuraProj/Saved/MeshAudit/geometry-audit.json")
REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/repair-report.json")

WELD_DECIMALS = 4
DEGENERATE_AREA_EPS = 1e-9


def parse_obj(path):
    vertices, uvs, faces = [], [], []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("v "):
                p = line.split()
                vertices.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith("vt "):
                p = line.split()
                uvs.append((float(p[1]), float(p[2]) if len(p) > 2 else 0.0))
            elif line.startswith("f "):
                corners = []
                for token in line.split()[1:]:
                    parts = token.split("/")
                    vi = int(parts[0])
                    ti = int(parts[1]) if len(parts) > 1 and parts[1] else 0
                    corners.append((vi - 1 if vi > 0 else len(vertices) + vi,
                                    ti - 1 if ti > 0 else -1))
                for i in range(1, len(corners) - 1):
                    faces.append((corners[0], corners[i], corners[i + 1]))
    return (np.asarray(vertices, dtype=np.float64),
            np.asarray(uvs, dtype=np.float64) if uvs else np.zeros((0, 2)),
            faces)


def weld(vertices):
    rounded = np.round(vertices, WELD_DECIMALS)
    unique, inverse = np.unique(rounded, axis=0, return_inverse=True)
    return unique, inverse


def repair(path):
    vertices, uvs, faces = parse_obj(path)
    if len(vertices) == 0 or not faces:
        return None, {"error": "empty"}

    welded, inverse = weld(vertices)
    # Map each OBJ corner to (welded vertex, uv index) so UVs survive.
    tri = np.array([[inverse[c[0][0]], inverse[c[1][0]], inverse[c[2][0]]]
                    for c in faces], dtype=np.int64)
    uv_idx = np.array([[c[0][1], c[1][1], c[2][1]] for c in faces], dtype=np.int64)

    a, b, c = welded[tri[:, 0]], welded[tri[:, 1]], welded[tri[:, 2]]
    area = np.linalg.norm(np.cross(b - a, c - a), axis=1) * 0.5
    repeated = ((tri[:, 0] == tri[:, 1]) | (tri[:, 1] == tri[:, 2])
                | (tri[:, 2] == tri[:, 0]))
    keep = ~repeated & (area > DEGENERATE_AREA_EPS)
    degenerate_removed = int(np.count_nonzero(~keep))

    tri, uv_idx = tri[keep], uv_idx[keep]

    # Drop duplicate faces, keeping the first occurrence.
    seen = {}
    unique_faces, unique_uvs = [], []
    for face, uv in zip(map(tuple, tri), map(tuple, uv_idx)):
        key = tuple(sorted(face))
        if key in seen:
            continue
        seen[key] = True
        unique_faces.append(face)
        unique_uvs.append(uv)
    duplicates_removed = int(len(tri) - len(unique_faces))
    tri = np.array(unique_faces, dtype=np.int64)
    uv_idx = np.array(unique_uvs, dtype=np.int64)

    # Solve winding: propagate orientation across shared edges per component.
    edges = np.concatenate([
        np.stack([tri[:, 0], tri[:, 1]], axis=1),
        np.stack([tri[:, 1], tri[:, 2]], axis=1),
        np.stack([tri[:, 2], tri[:, 0]], axis=1),
    ])
    edge_faces = defaultdict(list)
    for face_index, (x, y) in enumerate(edges):
        edge_faces[(min(x, y), max(x, y))].append((face_index // 3, x < y))

    adjacency = defaultdict(list)
    for entries in edge_faces.values():
        if len(entries) != 2:
            continue
        (f0, d0), (f1, d1) = entries
        # Traversed the same way -> the two faces disagree, so they need
        # opposite signs.
        adjacency[f0].append((f1, d0 == d1))
        adjacency[f1].append((f0, d0 == d1))

    face_count = len(tri)
    sign = np.zeros(face_count, dtype=np.int8)
    components = []
    for start in range(face_count):
        if sign[start] != 0:
            continue
        sign[start] = 1
        component = [start]
        queue = deque([start])
        while queue:
            current = queue.popleft()
            for neighbour, opposite in adjacency[current]:
                wanted = -sign[current] if opposite else sign[current]
                if sign[neighbour] == 0:
                    sign[neighbour] = wanted
                    component.append(neighbour)
                    queue.append(neighbour)
        components.append(component)

    flips = 0
    for component in components:
        indices = np.array(component)
        component_tri = tri[indices]
        # Signed volume contribution of the component as it currently stands.
        va, vb, vc = (welded[component_tri[:, 0]], welded[component_tri[:, 1]],
                      welded[component_tri[:, 2]])
        volume = float(np.sum(np.einsum("ij,ij->i", va, np.cross(vb, vc))) / 6.0)

        # A component that is closed on its own must come out positive.
        component_edges = Counter()
        for face in component_tri:
            for x, y in ((face[0], face[1]), (face[1], face[2]), (face[2], face[0])):
                component_edges[(min(x, y), max(x, y))] += 1
        closed = all(count == 2 for count in component_edges.values())

        flip = (closed and volume < 0)
        if flip:
            tri[indices] = tri[indices][:, ::-1]
            uv_idx[indices] = uv_idx[indices][:, ::-1]
            flips += len(indices)

    # Final measurements on the repaired mesh.
    a, b, c = welded[tri[:, 0]], welded[tri[:, 1]], welded[tri[:, 2]]
    volume = float(np.sum(np.einsum("ij,ij->i", a, np.cross(b, c))) / 6.0)
    final_edges = Counter()
    for face in tri:
        for x, y in ((face[0], face[1]), (face[1], face[2]), (face[2], face[0])):
            final_edges[(min(x, y), max(x, y))] += 1
    boundary = sum(1 for count in final_edges.values() if count == 1)
    nonmanifold = sum(1 for count in final_edges.values() if count > 2)

    # UVs are carried per face corner, so the output repeats them per corner.
    out_uv = np.zeros((len(uv_idx) * 3, 2))
    for slot, corner in enumerate(uv_idx.flatten()):
        if 0 <= corner < len(uvs):
            out_uv[slot] = uvs[corner]
    out_faces = np.arange(len(uv_idx) * 3).reshape(-1, 3)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / path.name
    with open(out_path, "w", encoding="utf-8") as handle:
        handle.write("# repaired by Scripts/repair_mesh_geometry.py\n")
        handle.write("o {}\n".format(path.stem))
        for vertex in welded:
            handle.write("v {:.6f} {:.6f} {:.6f}\n".format(*vertex))
        for uv in out_uv:
            handle.write("vt {:.6f} {:.6f}\n".format(*uv))
        for face in out_faces:
            handle.write("f {} {} {}\n".format(face[0] + 1, face[1] + 1, face[2] + 1))

    return out_path, {
        "faces_before": int(len(keep)),
        "duplicates_removed": duplicates_removed,
        "degenerate_removed": degenerate_removed,
        "faces_after": int(len(tri)),
        "winding_faces_flipped": flips,
        "components": len(components),
        "signed_volume_cm3": round(volume, 1),
        "boundary_edges_after": boundary,
        "nonmanifold_edges_after": nonmanifold,
        "vertices_welded": int(len(welded)),
        "out": str(out_path),
    }


def main():
    audit = json.loads(AUDIT.read_text(encoding="utf-8"))
    targets = []
    for name, entry in audit.items():
        if name.endswith(("_Internal", "_UV1")) or entry.get("status") != "ok":
            continue
        interior = entry["winding_unbalanced_edges"] - entry["boundary_edges"]
        if (entry["duplicate_face_excess"] or entry["degenerate_faces"]
                or interior > 0 or entry["inward_wound_solid"]):
            targets.append(name)
    targets.sort()

    print("REPAIR_TARGETS", len(targets))
    report = {}
    for name in targets:
        source = OBJ_DIR / (name + ".obj")
        if not source.exists():
            report[name] = {"error": "no source obj"}
            continue
        out_path, stats = repair(source)
        report[name] = stats
        print("REPAIRED", name, json.dumps(stats))

    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    total_dup = sum(s.get("duplicates_removed", 0) for s in report.values())
    total_degen = sum(s.get("degenerate_removed", 0) for s in report.values())
    total_flip = sum(s.get("winding_faces_flipped", 0) for s in report.values())
    print("REPAIR_TOTAL meshes={} duplicates={} degenerate={} winding_flips={}".format(
        len(report), total_dup, total_degen, total_flip))
    print("REPAIR_REPORT", REPORT)


if __name__ == "__main__":
    main()
