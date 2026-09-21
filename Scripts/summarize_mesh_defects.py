"""Summarise the landmark geometry audit into a defect report.

The OBJ exporter writes three variants per mesh (the mesh itself, an `_Internal`
backface copy, and a `_UV1` second-UV-channel copy), so the raw audit has roughly
three rows per real mesh. Only the base mesh is meaningful for topology: the
`_Internal` copy is a deliberate inverted duplicate for two-sided rendering and
would otherwise read as an inside-out solid on every mesh in the project.

Groups the real defects by landmark and prints the ones worth repairing.
"""

import json
from collections import Counter, defaultdict
from pathlib import Path

AUDIT = Path("C:/Git/AuraProj/Saved/MeshAudit/geometry-audit.json")
PROGRESS = Path("C:/Git/AuraProj/Saved/MeshAudit/progress.json")
OUT = Path("C:/Git/AuraProj/Saved/MeshAudit/geometry-defects.json")

SUFFIXES = ("_Internal", "_UV1")


def main():
    audit = json.loads(AUDIT.read_text(encoding="utf-8"))
    owners = json.loads(PROGRESS.read_text(encoding="utf-8"))["owners"]

    base = {}
    companions = Counter()
    for name, entry in audit.items():
        if name.endswith(SUFFIXES):
            companions[name.rsplit("_", 1)[-1]] += 1
            continue
        base[name] = entry

    def landmarks_for(name):
        # The mesh path is not in the audit key, so match by asset name.
        for path, ls in owners.items():
            if path.rsplit("/", 1)[-1] == name:
                return ls
        return []

    ok = {n: e for n, e in base.items() if e.get("status") == "ok"}
    failed = {n: e for n, e in base.items() if e.get("status") != "ok"}

    summary = {
        "meshes_total": len(base),
        "meshes_measured": len(ok),
        "meshes_failed": len(failed),
        "companion_variants": dict(companions),
        "open_surface": [n for n, e in ok.items() if not e["closed"]],
        "inward_wound_solid": [n for n, e in ok.items() if e["inward_wound_solid"]],
        "winding_inconsistent": [n for n, e in ok.items() if not e["winding_consistent"]],
        "nonmanifold": [n for n, e in ok.items() if e["nonmanifold_edges"]],
        "degenerate": [n for n, e in ok.items() if e["degenerate_faces"]],
        "duplicate_faces": [n for n, e in ok.items() if e["duplicate_face_excess"]],
        "nonfinite": [n for n, e in ok.items() if e["nonfinite_vertices"]],
    }

    # Per-landmark roll-up.
    per_landmark = defaultdict(lambda: Counter())
    detail = defaultdict(list)
    for name, entry in ok.items():
        for landmark in landmarks_for(name) or ["<unowned>"]:
            if not entry["closed"]:
                per_landmark[landmark]["open"] += 1
            if entry["inward_wound_solid"]:
                per_landmark[landmark]["inward"] += 1
            if not entry["winding_consistent"]:
                per_landmark[landmark]["winding_inconsistent"] += 1
            if entry["nonmanifold_edges"]:
                per_landmark[landmark]["nonmanifold"] += 1
            if entry["degenerate_faces"]:
                per_landmark[landmark]["degenerate"] += 1
            per_landmark[landmark]["meshes"] += 1
            if (not entry["closed"] or entry["inward_wound_solid"]
                    or not entry["winding_consistent"] or entry["degenerate_faces"]):
                detail[landmark].append({
                    "mesh": name,
                    "closed": entry["closed"],
                    "boundary_edges": entry["boundary_edges"],
                    "nonmanifold_edges": entry["nonmanifold_edges"],
                    "winding_unbalanced": entry["winding_unbalanced_edges"],
                    "degenerate_faces": entry["degenerate_faces"],
                    "duplicate_face_excess": entry["duplicate_face_excess"],
                    "signed_volume_cm3": round(entry["signed_volume_cm3"], 1),
                    "faces": entry["faces"],
                    "size_cm": entry["size_cm"],
                })

    summary["per_landmark"] = {k: dict(v) for k, v in sorted(per_landmark.items())}
    summary["detail"] = {k: sorted(v, key=lambda row: -row["faces"])
                         for k, v in sorted(detail.items())}
    summary["failed"] = failed

    OUT.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print("AUDIT meshes:", summary["meshes_total"], "measured:", summary["meshes_measured"])
    print("AUDIT open:", len(summary["open_surface"]),
          "| inward solids:", len(summary["inward_wound_solid"]),
          "| winding inconsistent:", len(summary["winding_inconsistent"]),
          "| nonmanifold:", len(summary["nonmanifold"]),
          "| degenerate:", len(summary["degenerate"]),
          "| duplicate faces:", len(summary["duplicate_faces"]),
          "| nonfinite:", len(summary["nonfinite"]))
    print()
    print("PER LANDMARK (meshes / open / inward / winding-bad / nonmanifold / degenerate)")
    for landmark, counts in summary["per_landmark"].items():
        print("  {:30s} {:4d} {:5d} {:7d} {:13d} {:11d} {:11d}".format(
            landmark, counts.get("meshes", 0), counts.get("open", 0),
            counts.get("inward", 0), counts.get("winding_inconsistent", 0),
            counts.get("nonmanifold", 0), counts.get("degenerate", 0)))
    print()
    print("REPORT", OUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
