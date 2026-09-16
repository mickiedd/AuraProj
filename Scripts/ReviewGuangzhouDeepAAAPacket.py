"""Independent local review of the Guangzhou Deep AAA evidence packet.

This review is intentionally Unreal-free: it checks the persisted reports,
manifests, captures, archive, and source-script syntax after the live-editor
validators have completed.
"""
import json
import struct
import subprocess
import sys
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
CAPTURE_ROOT = ROOT / "DeepAAA"
ARCHIVE = PROJECT / "Docs/Reports/Change-Archive/2026-09-15-guangzhou-gates-deep-aaa"
SCRIPTS = (
    "GenerateGuangzhouLandmarkDeepGeometry.py",
    "ApplyGuangzhouLandmarkDeepAAA.py",
    "ValidateGuangzhouLandmarkDeepAAA.py",
    "ValidateWuxianmenDeepAAA.py",
    "ValidateZhengximenDeepAAA.py",
    "CaptureGuangzhouLandmarkDeepAAA.py",
    "CaptureWuxianmenDeepFront.py",
    "CaptureWuxianmenDeepClose.py",
    "CaptureWuxianmenDeepRear.py",
    "CaptureZhengximenDeepFront.py",
    "CaptureZhengximenDeepClose.py",
    "CaptureZhengximenDeepRear.py",
)
EXPECTED = {
    "Wuxianmen": {
        "report": ROOT / "Wuxianmen_V4-deep-aaa.json",
        "validation": ROOT / "Wuxianmen_V4-deep-aaa-validation.json",
        "manifest": ROOT / "DeepAAA/Wuxianmen_V4_DeepAAA.json",
        "materials": 8,
        "vertices": 16436,
        "triangles": 70848,
        "capture_prefix": "Wuxianmen_V4",
        "rollback_suffix": "_AAA_Redone",
    },
    "Zhengximen": {
        "report": ROOT / "Zhengximen_V4-deep-aaa.json",
        "validation": ROOT / "Zhengximen_V4-deep-aaa-validation.json",
        "manifest": ROOT / "DeepAAA/Zhengximen_V4_DeepAAA.json",
        "materials": 7,
        "vertices": 24604,
        "triangles": 109136,
        "capture_prefix": "Zhengximen_V4",
        "rollback_suffix": "_AAARedone",
    },
}


def _assert(condition, message):
    if not condition:
        raise AssertionError(message)


def _png_size(path):
    data = path.read_bytes()
    _assert(data[:8] == b"\x89PNG\r\n\x1a\n", path)
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def main():
    for script in SCRIPTS:
        result = subprocess.run([sys.executable, "-m", "py_compile", str(PROJECT / "Scripts" / script)], cwd=PROJECT)
        _assert(result.returncode == 0, "py_compile failed: %s" % script)

    _assert(ARCHIVE.with_suffix(".md").exists(), ARCHIVE.with_suffix(".md"))
    _assert(ARCHIVE.with_suffix(".svg").exists(), ARCHIVE.with_suffix(".svg"))
    index = (PROJECT / ".claude/memory/visual-change-archive.md").read_text(encoding="utf-8")
    _assert("2026-09-15-guangzhou-gates-deep-aaa" in index, "archive index entry missing")

    checked_captures = 0
    for gate, expected in EXPECTED.items():
        report = json.loads(expected["report"].read_text(encoding="utf-8"))
        validation = json.loads(expected["validation"].read_text(encoding="utf-8"))
        manifest = json.loads(expected["manifest"].read_text(encoding="utf-8"))
        _assert(report["variant_suffix"] == "_AAADeep", gate)
        _assert(report["geometry_changed"] is True, gate)
        _assert(report["uvs_changed"] is False, gate)
        _assert(report["collision_changed"] is False, gate)
        _assert(report["placement_changed"] is False, gate)
        _assert(len(report["materials"]) == expected["materials"], gate)
        _assert(report["detail_source"]["vertex_count"] == expected["vertices"], gate)
        _assert(report["detail_source"]["triangle_count"] == expected["triangles"], gate)
        _assert(report["detail_source"]["geometry_changed"] is True, gate)
        _assert(report["detail_source"]["uv0_preserved_on_source_assemblies"] is True, gate)
        _assert("NoCollision" in report["detail_collision_intent"], gate)
        _assert(all(record["normal_layers"] == 2 for record in report["materials"].values()), gate)
        _assert(all(record["height_ratio"] > 0.015 for record in report["materials"].values()), gate)
        _assert(all(any("BumpOffset" in graph for graph in record["graph"]) for record in report["materials"].values()), gate)
        _assert(all(any("additive normal reinforcement" in graph for graph in record["graph"]) for record in report["materials"].values()), gate)
        _assert(validation["passed"] is True, gate)
        _assert(validation["detail_vertices"] == expected["vertices"], gate)
        _assert(validation["detail_triangles"] == expected["triangles"], gate)
        _assert(manifest["vertex_count"] == expected["vertices"], gate)
        _assert(manifest["triangle_count"] == expected["triangles"], gate)
        _assert(any(path.endswith("_AAA") for path in report["rollback_assets"]), gate)
        _assert(any(path.endswith("_ReferenceTuned") for path in report["rollback_assets"]), gate)
        _assert(any(path.endswith(expected["rollback_suffix"]) for path in report["rollback_assets"]), gate)
        for view in ("front", "close", "rear"):
            capture = CAPTURE_ROOT / (expected["capture_prefix"] + "-deep-" + view + ".png")
            _assert(capture.exists() and capture.stat().st_size > 500000, capture)
            _assert(_png_size(capture) == (1600, 1000), capture)
            checked_captures += 1

    print("GUANGZHOU_DEEP_AAA_LOCAL_REVIEW_PASS", json.dumps({"gates": list(EXPECTED), "captures_checked": checked_captures, "scripts_compiled": len(SCRIPTS)}))


if __name__ == "__main__":
    main()
