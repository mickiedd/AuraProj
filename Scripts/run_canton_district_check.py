"""Run a headless district check and normalize only a verified UE scripted quit.

UE 5.5 on this host has returned 1 after quit_editor() even when the script
wrote complete passing evidence. This runner never treats the exit code alone
as success: it requires fresh, valid JSON and a clean completed log.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODES = {
    "review": ("ReviewCantonDistrictPrototype.py", "Reload_Validation.json", 360),
    "traversal": ("ReviewCantonDistrictTraversal.py", "Traversal_Route_Matrix.json", 360),
    "seam": ("ProbeCantonDistrictSeams.py", "Seam_Probe.json", 240),
    "performance": ("ProbeCantonDistrictPerformance.py", "Performance_Probe.json", 600),
}
KNOWN_HEADLESS_ERRORS = (
    "LogAuraEditor: Error: RegisterMenus failed: no known ToolMenus path accepted the button.",
    'LogAutomationTest: Error: Locale is "C.UTF-8" but should be "C". Did something call setlocale()?',
)
MAP_CHECK_OK = re.compile(r"Map check complete: 0 Error\(s\), 0 Warning\(s\)")


def inspect_evidence(mode, raw_exit_code, result_path, log_path, started_ns):
    problems = []
    for path in (result_path, log_path):
        if not path.is_file():
            problems.append("missing " + str(path))
        elif path.stat().st_mtime_ns < started_ns:
            problems.append("stale " + str(path))
    if problems:
        return problems
    try:
        result = json.loads(result_path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return ["invalid result JSON: " + str(exc)]
    log = log_path.read_text(errors="replace")
    if raw_exit_code not in (0, 1):
        problems.append("unexpected UE exit code %d" % raw_exit_code)
    if "LogExit: Exiting." not in log or "Log file closed" not in log:
        problems.append("UE did not log a completed shutdown")
    for line in log.splitlines():
        if any(term in line for term in ("Error:", "Fatal error", "Critical error", "Failed to compile Material", "Default Material will be used")):
            if not any(known in line for known in KNOWN_HEADLESS_ERRORS):
                problems.append("unrecognized UE error: " + line[-300:])
    if mode in ("review", "traversal") and not MAP_CHECK_OK.search(log):
        problems.append("missing zero-error, zero-warning map check")
    if mode == "review":
        if result.get("passed") is not True or result.get("errors"):
            problems.append("district native validation did not pass")
        if result.get("road_collision_checks") != 100:
            problems.append("incomplete road collision sample")
        if result.get("max_road_long_edge_error_cm", float("inf")) >= 2:
            problems.append("road edge tolerance failed")
    elif mode == "traversal":
        if result.get("route_count", 0) < 7:
            problems.append("route matrix incomplete")
        if result.get("all_route_checks_pass") is not True:
            problems.append("one or more representative route checks failed")
        if result.get("pawn_traversal_performed") is not False:
            problems.append("pawn-traversal scope marker missing")
        routes = result.get("routes", [])
        if any(not route.get("nav_corner_floor_samples") or
               any(sample.get("floor_z_cm") is None
                   for sample in route["nav_corner_floor_samples"])
               for route in routes):
            problems.append("navigation-to-floor samples incomplete")
        closed = result.get("closed_gate_transit_diagnostic", {})
        if closed.get("nav_detour_ratio") is None or closed["nav_detour_ratio"] <= 1.5:
            problems.append("closed-gate obstruction diagnostic missing or unexpectedly direct")
    elif mode == "seam":
        if result.get("measurement") != "adjacent principal slab top-corner vertical step":
            problems.append("wrong seam measurement")
        if result.get("road_joint_comparisons") != 198:
            problems.append("incomplete road joint sample")
        if not isinstance(result.get("max_road_joint_step_cm"), (int, float)):
            problems.append("missing joint-step result")
    elif mode == "performance":
        if result.get("acceptance_gate_evaluated") is not False:
            problems.append("editor probe wrongly marked as runtime acceptance")
        if result.get("render_target") != [1920, 1080]:
            problems.append("wrong render resolution")
        if result.get("warmup_seconds") != 60 or result.get("capture_seconds") != 180:
            problems.append("incomplete timed sample")
        if result.get("sample_count", 0) < 1000:
            problems.append("too few editor samples")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=MODES)
    parser.add_argument("--editor", default=os.environ.get(
        "AURA_UNREAL_EDITOR",
        "/Volumes/M2/Engine/UE_5.5/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"))
    args = parser.parse_args(argv)
    script, result_name, timeout = MODES[args.mode]
    editor = Path(args.editor)
    if not editor.is_file():
        parser.error("UnrealEditor binary unavailable: " + str(editor))
    log = ROOT / ("Saved/Logs/CantonDistrictRunner_%s.log" % args.mode)
    stdout = ROOT / ("Saved/Logs/CantonDistrictRunner_%s.stdout.log" % args.mode)
    result = ROOT / "QA/Canton_District" / result_name
    command = [str(editor), str(ROOT / "Aura.uproject"),
               "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities",
               "-ExecutePythonScript=" + str(ROOT / "Scripts" / script),
               "-unattended", "-nosplash", "-NoSound", "-abslog=" + str(log)]
    started_ns = time.time_ns()
    try:
        with stdout.open("w") as stream:
            run = subprocess.run(command, cwd=ROOT, env={**os.environ, "LC_ALL": "C", "PYTHONCOERCECLOCALE": "0"}, stdout=stream,
                                 stderr=subprocess.STDOUT, timeout=timeout, check=False)
        raw_exit = run.returncode
        problems = inspect_evidence(args.mode, raw_exit, result, log, started_ns)
    except subprocess.TimeoutExpired:
        raw_exit = None
        problems = ["UE check timed out after %d seconds" % timeout]
    record = {
        "mode": args.mode, "command": command,
        "raw_exit_code": raw_exit, "normalized_exit_code": 0 if not problems else 1,
        "normalized": raw_exit == 1 and not problems,
        "evidence": str(result.relative_to(ROOT)), "log": str(log.relative_to(ROOT)),
        "problems": problems,
    }
    record_path = ROOT / "QA/Canton_District" / ("Runner_%s.json" % args.mode)
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"mode": args.mode, "raw_exit": raw_exit,
                      "result": "PASS" if not problems else "FAIL",
                      "problems": problems}, indent=2))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
