"""Read the editor logs and report the most recent crash in one step.

Diagnosing the Guidemen out-of-memory crash meant grepping the log by hand. This does it
directly: it scans every log under Saved/Logs, finds fatal errors and fatal warnings, classifies
the cause, and prints the surrounding context plus the likely fix.

Classifications it recognises:

  OUT_OF_MEMORY   "Ran out of memory", "The paging file is too small" — almost always an
                  Interchange import baking a shared-mesh instanced source. See
                  Scripts/ImportPreflight.py.
  GPU_DEVICE_LOST / DRIVER  device removed, DXGI errors
  ASSERTION       check()/ensure() failures and fatal asserts
  OTHER_FATAL     any other Fatal error

Read-only.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
LOGS = PROJECT / "Saved/Logs"
OUTPUT = PROJECT / "Saved/RawModelImport/editor-crash-report.json"

PATTERNS = [
    ("OUT_OF_MEMORY", re.compile(r"Ran out of memory|paging file is too small|out of memory allocating", re.I)),
    ("GPU_DEVICE_LOST", re.compile(r"device removed|DXGI_ERROR|GPU crash|device lost", re.I)),
    ("NANITE_BLEND_MODE", re.compile(r"IsSupportedBlendMode|Nanite::IsSupported", re.I)),
    ("ASSERTION", re.compile(r"Assertion failed|Fatal error:.*check\(|ensure condition failed", re.I)),
    ("OTHER_FATAL", re.compile(r"Fatal error", re.I)),
    ("HANDLED_ENSURE", re.compile(r"Handled ensure|Ensure condition failed", re.I)),
]
# A handled ensure is logged as an error but does not terminate the editor, so it must not be
# reported as a crash. It is still worth listing, because it marks a subsystem misusing an API.
NON_FATAL = {"HANDLED_ENSURE"}
REMARKS = {
    "OUT_OF_MEMORY": ("An allocation exceeded the commit limit. On this project that has meant an "
                      "Interchange import baking a shared-mesh instanced glTF: baking writes every "
                      "node out as its own mesh. Run Scripts/ImportPreflight.py before any GLB "
                      "import, and keep bake_meshes=False on instanced sources. The import guard "
                      "now in the import scripts refuses this case automatically."),
    "GPU_DEVICE_LOST": "The GPU was reset or removed. Check the driver and TDR settings.",
    "NANITE_BLEND_MODE": ("A material on a Nanite mesh uses a blend or shading mode Nanite cannot "
                          "render. Set that material's blend mode to Opaque (and its shading model to "
                          "Default Lit), or disable Nanite on the mesh. Any material built for these "
                          "buildings must keep blend mode Opaque while used_with_nanite is True."),
    "ASSERTION": "A code assertion fired; read the frames above the assert for the owning system.",
    "OTHER_FATAL": "Unclassified fatal error; read the surrounding frames.",
}


def _scan(path: Path) -> list:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return []
    lines = text.replace("\r", "\n").split("\n")
    found = []
    for index, line in enumerate(lines):
        for label, pattern in PATTERNS:
            if pattern.search(line):
                context = [entry.strip() for entry in lines[max(0, index - 12):index + 4] if entry.strip()]
                found.append({"classification": label, "line": index + 1,
                              "message": line.strip()[:220], "context": context[-14:]})
                break
    return found


def main():
    logs = sorted(LOGS.glob("*.log"), key=lambda item: item.stat().st_mtime, reverse=True)
    report = {"logs": [], "crashes": [], "map_saved": False}
    for path in logs:
        hits = _scan(path)
        report["logs"].append({"log": str(path), "bytes": path.stat().st_size,
                               "modified": path.stat().st_mtime, "hits": len(hits)})
        for hit in hits:
            hit["log"] = str(path)
            if hit["classification"] in NON_FATAL:
                report.setdefault("non_fatal", []).append(hit)
            else:
                report["crashes"].append(hit)

    if report["crashes"]:
        # logs are sorted newest first, so take the crash from the FIRST log that has one
        newest_log = next(item["log"] for item in report["logs"] if item["hits"])
        latest = next(hit for hit in report["crashes"] if hit["log"] == newest_log)
        report["latest_crash"] = {
            "classification": latest["classification"],
            "log": latest["log"],
            "line": latest["line"],
            "message": latest["message"],
            "likely_fix": REMARKS.get(latest["classification"], ""),
        }
        print("CRASH_REPORT newest crash:", latest["classification"])
        print("  log  :", latest["log"], "line", latest["line"])
        print("  what :", latest["message"])
        print("  fix  :", REMARKS.get(latest["classification"], ""))
        print()
        print("  context:")
        for entry in latest["context"]:
            print("   ", entry[:150])
    else:
        print("CRASH_REPORT no fatal errors found in", len(logs), "logs")
    print()
    print("CRASH_LOG_SUMMARY", json.dumps([{"log": Path(item["log"]).name, "hits": item["hits"]}
                                          for item in report["logs"]]))
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("CRASH_REPORT_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
