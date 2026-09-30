"""Build the reversible wall-foundation corridor profile used by the editor tool.

The profile carries only wall XY segments.  The native editor implementation
samples the locked imported landscape and writes the derived cut/fill deltas to
the ``Wall_Foundation_Adjustment`` edit layer, so the base terrain remains
recoverable and the exact source route is auditable.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "Export/Provisional/UE_Overlay_Metres.json"
PLAN = ROOT / "QA/Canton_Continuation/Walled_WallGate_Fit_Plan.json"
OUT = ROOT / "Data/Canton_WalledCity_Provisional_Wall_Foundation_Profile.json"


def local(point):
    return [(float(point[0]) - 729400.0) * 100.0,
            (float(point[1]) - 2557300.0) * 100.0]


def add_segment(segments, a, b, kind):
    if a == b:
        return
    segments.append({"a_cm": [round(a[0], 3), round(a[1], 3)],
                     "b_cm": [round(b[0], 3), round(b[1], 3)],
                     "kind": kind})


def main():
    overlay = json.loads(OVERLAY.read_text())
    plan = json.loads(PLAN.read_text())
    segments = []
    for feature in overlay["layers"]["Wall"]["features"]:
        geometry = feature["geometry"]
        lines = geometry["coordinates"] if geometry["type"] == "Polygon" else [geometry["coordinates"]]
        for line in lines:
            local_line = [local(point) for point in line]
            for a, b in zip(local_line, local_line[1:]):
                add_segment(segments, a, b, "source_trace")
    for row in plan["trace_replacements"] + plan["bridge_segments"]:
        add_segment(segments, row["point_a_cm"], row["point_b_cm"], row["kind"])
    result = {
        "schema": "canton-wall-foundation-profile-v1",
        "map": "/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL",
        "historically_accepted": False,
        "description": "Reversible provisional corridor only; no surveyed historical terrain claim.",
        "corridor_half_width_cm": 450.0,
        "blend_width_cm": 500.0,
        "max_adjustment_cm": 300.0,
        "segments": segments,
        "source_sha256": {
            "Export/Provisional/UE_Overlay_Metres.json": hashlib.sha256(OVERLAY.read_bytes()).hexdigest(),
            "QA/Canton_Continuation/Walled_WallGate_Fit_Plan.json": hashlib.sha256(PLAN.read_bytes()).hexdigest(),
        },
    }
    OUT.write_text(json.dumps(result, indent=2) + "\n")
    print(f"wrote {OUT} ({len(segments)} segments)")


if __name__ == "__main__":
    main()
