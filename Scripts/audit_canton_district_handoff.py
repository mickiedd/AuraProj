"""Independent, read-only audit of the Canton provisional district handoff.

This is the *reviewer* side of the M05 delivery. It never opens Unreal and never
writes into the frozen package: it re-derives every claim it can from the R16
source and the retained evidence, so a stale, duplicated or mislabelled artifact
shows up as a finding instead of being taken on trust.

Findings are ranked BLOCKER / MAJOR / MINOR / INFO and written to
Review/M05_Independent_Audit_Findings.json for hand-off back to the implementer.
"""
import csv
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DISTRICT = ROOT / "QA/Canton_District"
R16 = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
HANDOFF = ROOT / "Review/M05_Final_Handoff.md"
MANIFEST = ROOT / "Data/M02_M05_District_Artifacts.csv"
FREEZE = ROOT / "Review/M05_Repository_Freeze.json"
OUT = ROOT / "Review/M05_Independent_Audit_Findings.json"

findings = []


def find(severity, ident, title, detail, evidence=None, action=None):
    findings.append({
        "id": ident, "severity": severity, "title": title, "detail": detail,
        "evidence": evidence or [], "requested_action": action,
    })


def sha(path):
    return hashlib.sha256((ROOT / path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads((ROOT / path).read_text())


# ---------------------------------------------------------------- integrity
# The project's change-archive index is append-only by policy: AGENTS.md requires a new
# entry for every completed job, so a freeze that hashes it can never stay valid.
APPEND_ONLY = {".claude/memory/visual-change-archive.md"}


def audit_manifest():
    rows = list(csv.DictReader((ROOT / MANIFEST).open(newline="")))
    missing, mismatched, append_only_drift = [], [], []
    for row in rows:
        path = ROOT / row["path"]
        if not path.is_file():
            missing.append(row["path"])
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != row["sha256"] or path.stat().st_size != int(row["bytes"]):
            (append_only_drift if row["path"] in APPEND_ONLY else mismatched).append(row["path"])
    if missing or mismatched:
        find("BLOCKER", "AUD-MANIFEST-001", "District manifest does not match the tree",
             "%d missing, %d hash/size mismatches" % (len(missing), len(mismatched)),
             missing[:10] + mismatched[:10],
             "Re-run Scripts/refresh_canton_district_manifest.py and re-freeze.")
    if append_only_drift:
        find("MAJOR", "AUD-FREEZE-002",
             "The freeze hashes the append-only change-archive index",
             "The manifest includes %s, which AGENTS.md requires every completed job to append "
             "to. The freeze therefore invalidates itself the moment the next job records "
             "itself — this audit did exactly that, and it is the only row that moved."
             % ", ".join(append_only_drift),
             ["Data/M02_M05_District_Artifacts.csv",
              "Scripts/refresh_canton_district_freeze.py"],
             "Drop the append-only index from the manifest and freeze its length or its tail "
             "commit instead, so a compliant archive entry cannot break the freeze.")
    return {"entries": len(rows), "missing": missing, "mismatched": mismatched,
            "append_only_drift": append_only_drift}


def audit_freeze():
    freeze = read_json("Review/M05_Repository_Freeze.json")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    problems = []
    if freeze["base_commit"] != head:
        problems.append("base_commit %s != HEAD %s" % (freeze["base_commit"][:12], head[:12]))
    for key, path in (("district_manifest_sha256", MANIFEST),
                      ("m05_handoff_sha256", HANDOFF)):
        if freeze[key] != sha(path):
            problems.append("%s stale for %s" % (key, path))
    if problems:
        find("MAJOR", "AUD-FREEZE-001", "Repository freeze record is stale",
             "; ".join(problems), ["Review/M05_Repository_Freeze.json"],
             "Regenerate the freeze after the handoff stops changing.")
    return {"base_commit": freeze["base_commit"], "head": head,
            "dirty_entries": freeze["git_status_porcelain_count"], "problems": problems}


# ------------------------------------------------------------ evidence hygiene
def audit_duplicate_evidence():
    """Distinct evidence names that are byte-identical cannot be independent checks."""
    groups = {}
    for path in sorted(DISTRICT.glob("*.json")):
        groups.setdefault(hashlib.sha256(path.read_bytes()).hexdigest(), []).append(path.name)
    clones = {h: names for h, names in groups.items() if len(names) > 1}
    for digest, names in clones.items():
        find("MAJOR", "AUD-EVIDENCE-001",
             "Distinct evidence files are byte-identical: " + ", ".join(names),
             "The handoff presents these as separate evidence items, but one artifact "
             "cannot be two independent checks. sha256 " + digest[:16],
             ["QA/Canton_District/" + name for name in names],
             "Give each claimed probe its own measurement, or cite one artifact once and "
             "drop the duplicate claim.")
    return clones


def audit_handoff_links():
    text = HANDOFF.read_text()
    links = re.findall(r"\]\(([^)#]+?)\)", text)
    broken = []
    for link in links:
        if link.startswith(("http://", "https://", "mailto:")):
            continue
        if not (HANDOFF.parent / link).resolve().exists():
            broken.append(link)
    if broken:
        find("BLOCKER", "AUD-LINK-001", "Handoff links point at missing files",
             "%d broken relative links" % len(broken), broken[:20],
             "Fix the links or restore the artifacts before the handoff is used.")
    return {"links": len(links), "broken": broken}


def audit_view_duplication():
    """A named view must resolve to one image, not two different ones."""
    pairs = []
    for path in sorted((ROOT / "Review/M05_QA_Screenshots").glob("*.png")):
        twin = DISTRICT / path.name
        if twin.is_file():
            a = hashlib.sha256(path.read_bytes()).hexdigest()
            b = hashlib.sha256(twin.read_bytes()).hexdigest()
            pairs.append({"view": path.stem, "identical": a == b,
                          "m05_bytes": path.stat().st_size, "qa_bytes": twin.stat().st_size})
    divergent = [p for p in pairs if not p["identical"]]
    for item in divergent:
        find("MAJOR", "AUD-VIEW-001",
             "Two different images share the view name '%s'" % item["view"],
             "Review/M05_QA_Screenshots/%s.png (%d B) and QA/Canton_District/%s.png (%d B) "
             "differ, and the handoff links the older copy while the capture settings "
             "record the newer run." % (item["view"], item["m05_bytes"], item["view"],
                                        item["qa_bytes"]),
             ["Review/M05_QA_Screenshots/%s.png" % item["view"],
              "QA/Canton_District/%s.png" % item["view"]],
             "Decide which capture is authoritative, replace the stale copy and re-freeze.")
    return pairs


# ------------------------------------------------------------ claim arithmetic
def audit_traversal():
    data = read_json("QA/Canton_District/Traversal_Route_Matrix.json")
    problems = []
    routes = data["routes"]
    if data["route_count"] != len(routes):
        problems.append("route_count %s != %d rows" % (data["route_count"], len(routes)))
    passed = sum(1 for r in routes if r["route_check_pass"])
    if data["route_check_pass_count"] != passed:
        problems.append("route_check_pass_count %s != %d" % (data["route_check_pass_count"], passed))
    if data["all_route_checks_pass"] != all(r["route_check_pass"] for r in routes):
        problems.append("all_route_checks_pass inconsistent")
    for r in routes:
        straight = math.dist(r["start_xy_cm"], r["end_xy_cm"])
        if r["nav_path_length_cm"] is not None:
            ratio = r["nav_path_length_cm"] / straight
            if abs(ratio - r["nav_detour_ratio"]) > 1e-6:
                problems.append("%s detour_ratio %.6f != %.6f" %
                                (r["route"], r["nav_detour_ratio"], ratio))
        expect = (r["floor_support_pass"] and r["nav_path_valid"]
                  and r["nav_detour_ratio"] is not None and r["nav_detour_ratio"] <= 1.5)
        if expect != r["route_check_pass"]:
            problems.append("%s route_check_pass does not follow from its inputs" % r["route"])
        if r["floor_trace_hits"] > r["floor_trace_count"]:
            problems.append("%s more hits than traces" % r["route"])
    if problems:
        find("MAJOR", "AUD-TRAVERSAL-001", "Route matrix arithmetic is inconsistent",
             "; ".join(problems), ["QA/Canton_District/Traversal_Route_Matrix.json"],
             "Recompute the matrix; a self-inconsistent matrix cannot gate Day 14.")
    return {"routes": len(routes), "passed": passed, "problems": problems}


def audit_r16_cross_check():
    """Re-derive the terrain z from the R16 and compare with the recorded nav z."""
    meta = read_json("Export/Provisional/Heightmap_Metadata.json")
    values = memoryview(R16.read_bytes()).cast("H")
    size = meta["vertices"]
    if len(values) != size * size:
        find("BLOCKER", "AUD-R16-001", "R16 length does not match declared vertices",
             "%d samples vs %d declared" % (len(values), size * size), [str(R16.name)],
             "Re-export the R16.")
        return {}

    def z_at(x_cm, y_cm):
        fx, fy = x_cm / 200, y_cm / 200
        col, row = math.floor(fx), math.floor(fy)
        u, v = fx - col, fy - row
        sample = lambda a, b: (values[b * size + a] - meta["code_zero"]) * meta["ue_z_scale"] / 128
        return ((1-u)*(1-v)*sample(col, row) + u*(1-v)*sample(col+1, row) +
                (1-u)*v*sample(col, row+1) + u*v*sample(col+1, row+1))

    data = read_json("QA/Canton_District/Traversal_Route_Matrix.json")
    rows = []
    for route in data["routes"]:
        sx, sy = route["start_xy_cm"]
        ex, ey = route["end_xy_cm"]
        zs, ze = z_at(sx, sy), z_at(ex, ey)
        first, last = route["nav_points_cm"][0], route["nav_points_cm"][-1]
        rows.append({
            "route": route["route"],
            "r16_z_start_cm": round(zs, 2), "nav_z_start_cm": round(first[2], 2),
            "nav_minus_r16_start_cm": round(first[2] - zs, 2),
            "r16_z_end_cm": round(ze, 2), "nav_z_end_cm": round(last[2], 2),
            "nav_minus_r16_end_cm": round(last[2] - ze, 2),
            "max_floor_vs_r16_error_cm": round(route["max_floor_vs_r16_error_cm"], 3),
        })
    # The nav mesh rides on the road slab, so the two independent references should
    # agree to within the slab build-up plus nav cell quantisation.
    worst = max(abs(r["nav_minus_r16_start_cm"]) for r in rows)
    if worst > 150:
        find("MAJOR", "AUD-R16-002",
             "Navigation height and re-derived R16 height disagree by %.1f cm" % worst,
             "A gap this large means either the R16 decode/orientation used by the "
             "review differs from the exported raster, or the nav mesh is not sitting on "
             "the authored road. Both void the floor-support claim.",
             ["QA/Canton_District/Traversal_Route_Matrix.json",
              "Export/Provisional/Heightmap_Metadata.json"],
             "State the intended slab build-up and re-measure.")
    return rows


def audit_drainage():
    grades = list(csv.DictReader((ROOT / "QA/Street_Grade_Profiles.csv").open(newline="")))
    drainage = read_json("GIS/Drainage_Locations.geojson")
    problems = []
    gutter = {int(r["y_ue_cm"]): float(r["modern_surface_m_egm2008"])
              for r in grades if r["road_id"] == "ENG-DRAIN-01"}
    for feature in drainage["features"]:
        start_y, end_y = [130000 + int(p[1] * 100) for p in feature["geometry"]["coordinates"]]
        step = 400 if end_y > start_y else -400
        heights = [gutter[y] for y in range(start_y, end_y + step, step)]
        if not all(b <= a for a, b in zip(heights, heights[1:])):
            problems.append(feature["properties"]["id"] + " not monotonic downhill")
    if problems:
        find("MAJOR", "AUD-DRAIN-001", "Declared drainage fall is not monotonic",
             "; ".join(problems), ["GIS/Drainage_Locations.geojson",
                                   "QA/Street_Grade_Profiles.csv"],
             "Re-route the gutter reach or correct the profile.")
    return problems


def audit_capture_pair():
    settings = read_json("QA/Canton_District/Capture_Settings.json")
    qa = read_json("QA/Canton_District/Capture_QA.json")
    by_view = {s["view"]: s for s in settings}
    problems = []
    for view, path in (("dry_ground", "dry_ground.png"),
                       ("after_rain_ground", "after_rain_ground.png")):
        spec = by_view.get(view)
        if not spec:
            problems.append("no capture spec for " + view)
            continue
        if not (ROOT / spec["path"]).is_file():
            problems.append("missing capture " + spec["path"])
        if (DISTRICT / path).is_file() and Path(spec["path"]).name != path:
            problems.append("spec path/name mismatch for " + view)
    dry, wet = by_view["dry_ground"], by_view["after_rain_ground"]
    if dry["camera_cm"] != wet["camera_cm"] or dry["target_cm"] != wet["target_cm"]:
        problems.append("dry/damp pair is not from the same viewpoint")
    if dry.get("damp_actors_hidden") is not True or wet.get("damp_actors_hidden") is not False:
        problems.append("damp toggle not declared as the only difference")
    if problems:
        find("MINOR", "AUD-CAPTURE-001", "Paired wetness capture is not a clean A/B",
             "; ".join(problems), ["QA/Canton_District/Capture_Settings.json"],
             "Re-capture the pair from one viewpoint with only the damp toggle changed.")
    return {"changed_pixels_gt8": qa.get("changed_pixels_gt8"), "problems": problems}


def audit_manifest_coverage():
    """Everything the handoff and cook claim must be inside the frozen manifest."""
    rows = list(csv.DictReader((ROOT / MANIFEST).open(newline="")))
    paths = {row["path"] for row in rows}
    cooked = [m.split("/Game/")[-1] for m in read_json("QA/Canton_District/Cook_Result.json")["maps"]]
    uncovered = []
    for game_path in cooked:
        folder = "Content/" + game_path.split("/Maps/")[0].lstrip("/")
        if not any(p.startswith(folder + "/") for p in paths):
            uncovered.append(folder)
    if uncovered:
        find("MAJOR", "AUD-COVERAGE-001",
             "Cooked levels are not covered by the frozen district manifest",
             "The cook result and the handoff both cover these levels, but no package "
             "under them is hashed, so the second level cannot be reproduced or verified "
             "from the freeze: " + ", ".join(uncovered),
             ["Data/M02_M05_District_Artifacts.csv",
              "QA/Canton_District/Cook_Result.json"],
             "Extend the manifest to every cooked level, or scope the cook to the "
             "district and stop citing the other level as delivered.")
    return {"cooked_folders": ["Content/" + c.split("/Maps/")[0].lstrip("/") for c in cooked],
            "uncovered": uncovered}


def audit_build_result_consistency():
    """Build_Result is frozen evidence; it must describe the map that exists now."""
    build = read_json("QA/Canton_District/Build_Result.json")
    reload_ = read_json("QA/Canton_District/Reload_Validation.json")
    counts = build["instance_counts"]
    problems = []
    if counts["pebble"] != reload_["mixed_slabs"]:
        problems.append("instance_counts.pebble=%d but the map has %d mixed-lane slabs"
                        % (counts["pebble"], reload_["mixed_slabs"]))
    if counts["plot"] != reload_["broad_parcel_pads"]:
        problems.append("instance_counts.plot=%d but the map has %d parcel pads"
                        % (counts["plot"], reload_["broad_parcel_pads"]))
    if problems:
        find("MAJOR", "AUD-BUILD-001",
             "Frozen Build_Result.json describes a superseded build",
             "; ".join(problems) + ". The manifest hashes this file as evidence, so the "
             "package presents two different district compositions.",
             ["QA/Canton_District/Build_Result.json",
              "QA/Canton_District/Reload_Validation.json",
              "Scripts/BuildCantonDistrictPrototype.py"],
             "Regenerate Build_Result from the current script or label it explicitly as "
             "the initial-build snapshot.")
    return problems


def audit_declared_thresholds():
    """A gate that fails a day must exist in the versioned acceptance budget."""
    budget = (ROOT / "Docs/Terrain_Acceptance_Budget.md").read_text()
    plan = (ROOT / "Docs/Plans/Canton_1880_1900_UE5_Terrain_20_Day_Plan.md").read_text()
    declared = budget + plan
    # Match a numeric gate, not a bare digit: "grade" within 80 chars of a percentage,
    # and any explicit detour/directness ratio.
    grade_gate = re.search(r"grade[^.\n]{0,80}%|%[^.\n]{0,40}grade", declared, re.I)
    direct_gate = re.search(r"detour|directness", declared, re.I)
    undeclared = []
    if not direct_gate:
        undeclared.append({"gate": "navigation detour ratio <= 1.5",
                           "asserted_in": ["Scripts/ReviewCantonDistrictTraversal.py",
                                           "Scripts/run_canton_district_check.py",
                                           "Scripts/test_canton_district_contract.py"]})
    if not grade_gate:
        undeclared.append({"gate": "road step grade < 8% (and the 12% removal trigger)",
                           "asserted_in": ["Scripts/test_canton_district_contract.py",
                                           "QA/Canton_District/Slope_Refinement.json"]})
    if undeclared:
        find("MAJOR", "AUD-THRESHOLD-001",
             "Numeric gates are asserted in code but declared nowhere",
             "The acceptance budget and the 20-day plan contain no numeric navigation "
             "or road-grade gate, yet the code fails a day on them and 20 mixed-lane "
             "pieces were deleted to satisfy one: " + "; ".join(d["gate"] for d in undeclared),
             [path for d in undeclared for path in d["asserted_in"]]
             + ["Docs/Terrain_Acceptance_Budget.md",
                "QA/Canton_District/Slope_Refinement.json"],
             "Declare both gates in the versioned budget with owner approval, or drop "
             "them from the pass/fail logic and report the numbers as diagnostics.")
    return undeclared


def audit_nav_vs_road():
    """The nav surface must be the authored road top, not terrain, shoulder or a slab edge."""
    meta = read_json("Export/Provisional/Heightmap_Metadata.json")
    values = memoryview(R16.read_bytes()).cast("H")
    size = meta["vertices"]

    def z_at(x_cm, y_cm):
        fx, fy = x_cm / 200, y_cm / 200
        col, row = math.floor(fx), math.floor(fy)
        u, v = fx - col, fy - row
        sample = lambda a, b: (values[b * size + a] - meta["code_zero"]) * meta["ue_z_scale"] / 128
        return ((1-u)*(1-v)*sample(col, row) + u*(1-v)*sample(col+1, row) +
                (1-u)*v*sample(col, row+1) + u*v*sample(col+1, row+1))

    # Build-up declared by BuildCantonDistrictPrototype.place(): principal road top is
    # surface_offset 7 + full height 12 = z_at + 19; mixed lane 5 + 9 = z_at + 14.
    EXPECTED = {"principal": 19.0, "mixed": 14.0, "intersection": 19.0,
                "gate": 19.0, "gutter": 19.0, "reference_only": 14.0}
    rows = []
    for route in read_json("QA/Canton_District/Traversal_Route_Matrix.json")["routes"]:
        x, y = route["start_xy_cm"]
        nav_z = route["nav_points_cm"][0][2]
        r16_z = z_at(x, y)
        rows.append({"route": route["route"], "surface": route["surface"],
                     "nav_minus_r16_cm": round(nav_z - r16_z, 2),
                     "expected_build_up_cm": EXPECTED[route["surface"]],
                     "nav_minus_road_top_cm": round(nav_z - r16_z - EXPECTED[route["surface"]], 2)})
    worst = max(abs(r["nav_minus_road_top_cm"]) for r in rows)
    spread = max(r["nav_minus_r16_cm"] for r in rows) - min(r["nav_minus_r16_cm"] for r in rows)
    if worst > 10 or spread > 15:
        find("MAJOR", "AUD-NAVROAD-001",
             "Navigation surface does not sit on the authored road top",
             "The road top is a constant build-up above the R16 (%s cm by surface), but the "
             "nav mesh is %.1f..%.1f cm above the same R16 — a %.1f cm spread and up to "
             "%.1f cm off the designed road surface. Nothing in the package relates the two, "
             "so floor-support PASS cannot tell a road from terrain, shoulder or a slab edge. "
             "Some of this is Recast cell-height quantisation; the rest is unmeasured."
             % ({k: int(v) for k, v in EXPECTED.items()},
                min(r["nav_minus_r16_cm"] for r in rows),
                max(r["nav_minus_r16_cm"] for r in rows), spread, worst),
             ["QA/Canton_District/Traversal_Route_Matrix.json",
              "Scripts/BuildCantonDistrictPrototype.py"],
             "Add a nav-to-road check, or record the quantisation budget that explains the "
             "spread, before Day 14 is closed.")
    return rows


def audit_day14_waiver():
    """Day 14's own plan text demands fix-or-waive for relevant map warnings."""
    log = (ROOT / "Saved/Logs/CantonDistrictRunner_traversal.log")
    warnings = []
    if log.is_file():
        for line in log.read_text(errors="replace").splitlines():
            if "navigation" in line.lower() and "triangle" in line.lower():
                warnings.append(line.strip()[-200:])
    open_issue = (ROOT / "Data/Open_Issues.csv").read_text()
    waived = "waiv" in open_issue.lower()
    if warnings and not waived:
        find("MINOR", "AUD-WAIVE-001",
             "Navigation-export warnings are neither fixed nor explicitly waived",
             "Day 14 requires every relevant map warning to be fixed or explicitly waived; "
             "these are carried as an open issue instead: " + " | ".join(warnings[:3]),
             ["Data/Open_Issues.csv", "Saved/Logs/CantonDistrictRunner_traversal.log"],
             "Add an explicit waiver line with an owner and a follow-up, or fix the "
             "collision export.")
    return {"warnings": len(warnings), "waived": waived}


def audit_validator_tolerances():
    """Loose bounds in the validator let a real regression pass unnoticed."""
    source = (ROOT / "Source/AuraEditor/Private/Terrain/CantonTerrainLibrary.cpp").read_text()
    loose = []
    for pattern, label in (
        (r"MixedSlabs>=(\d+) && MixedSlabs<=(\d+)", "mixed-lane slab count"),
        (r"Gutters>=(\d+)", "covered gutter count"),
        (r"Weeds>0 && Damp>0", "detail instance counts"),
    ):
        match = re.search(pattern, source)
        if match:
            loose.append(label + ": " + match.group(0))
    actual = read_json("QA/Canton_District/Reload_Validation.json")
    notes = ("current values are %d mixed-lane, %d gutters, %d weeds, %d damp"
             % (actual["mixed_slabs"], actual["gutters"], actual["weed_instances"],
                actual["wet_patches"]))
    if loose:
        find("MINOR", "AUD-VALIDATOR-001",
             "District validator uses open-ended count bounds",
             "; ".join(loose) + ". " + notes + ". A regression that removes mixed-lane "
             "pieces or gutters still reports passed:true.",
             ["Source/AuraEditor/Private/Terrain/CantonTerrainLibrary.cpp"],
             "Assert exact counts or a +/-2 tolerance so the check can fail.")
    return loose


def audit_slab_float():
    """Road meshes float above the terrain; nothing checks the resulting lip."""
    script = (ROOT / "Scripts/BuildCantonDistrictPrototype.py").read_text()
    # place(label, x, y, length, width, height, material, surface_offset_cm=...) —
    # the two principal-road calls wrap across lines, so match across newlines.
    road = re.search(r'District_Main_Stone_%d"[\s\S]{0,80}?,\s*600,\s*200,\s*(\d+),'
                     r'\s*mats\["stone"\],\s*surface_offset_cm=(\d+)', script)
    shoulder = re.search(r'District_Main_EarthShoulder_%d_%d"[\s\S]{0,80}?,\s*250,\s*200,'
                         r'\s*(\d+),\s*mats\["earth"\],\s*surface_offset_cm=(\d+)', script)
    if road and shoulder:
        r_h, r_o = int(road.group(1)), int(road.group(2))
        s_h, s_o = int(shoulder.group(1)), int(shoulder.group(2))
        road_top, road_bottom = r_o + r_h, r_o
        shoulder_top = s_o + s_h
        find("MINOR", "AUD-SLAB-001",
             "Road slabs float above the terrain and step down to the shoulder",
             "The principal slab spans z_at+%d (underside) to z_at+%d (top), so it leaves a "
             "%d cm gap over the terrain, and the shoulder top is z_at+%d — a %d cm step at "
             "the road edge. The long-edge check compares top edges to terrain and cannot "
             "see either." % (road_bottom, road_top, road_bottom, shoulder_top,
                              road_top - shoulder_top),
             ["Scripts/BuildCantonDistrictPrototype.py",
              "Source/AuraEditor/Private/Terrain/CantonTerrainLibrary.cpp"],
             "Either close the underside with a kerb/skirt mesh or record the lip as an "
             "accepted blockout property with a view that shows it.")
        return {"road_bottom_offset_cm": road_bottom, "road_top_offset_cm": road_top,
                "shoulder_top_offset_cm": shoulder_top}
    return {}


def audit_gate_conflict():
    """The failing gate route is a footprint conflict, not a mystery detour."""
    manifest = read_json("ContentSource/GuangzhouLandmarks/Wenmingmen/asset_manifest.json")
    footprint_x_m = float(manifest["approx_footprint_m"][0])
    readme = (ROOT / "ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md").read_text()
    bounds = re.search(r"([\d.]+) × ([\d.]+) × ([\d.]+) cm", readme)
    closed_door = "closed, recessed double timber door" in readme
    traversal = read_json("QA/Canton_District/Traversal_Route_Matrix.json")
    gate = next(r for r in traversal["routes"] if r["route"] == "gate_threshold_approach")
    centre_x = 180000.0
    apex = max(abs(p[0] - centre_x) for p in gate["nav_points_cm"])
    half = footprint_x_m * 100 / 2
    agent = 35.0  # UE default nav agent radius
    gate_y = 121500.0                      # BuildCantonDistrictPrototype spawn point
    half_depth = (float(bounds.group(2)) / 2) if bounds else 2806.9 / 2
    inside = [name for name, (x, y) in
              (("start", gate["start_xy_cm"]), ("end", gate["end_xy_cm"]))
              if abs(x - centre_x) <= half and abs(y - gate_y) <= half_depth]
    problem = []
    if abs(apex - (half + agent)) < 200:
        problem.append("the detour apex %.0f cm east of the road centre matches the gate's "
                       "outer face (half-width %.0f + agent radius %.0f = %.0f cm)"
                       % (apex, half, agent, half + agent))
    if closed_door:
        problem.append("the gate package models a closed double timber door, so the "
                       "corridor is architecturally blocked, not a collision bug")
    if inside:
        problem.append("route endpoint(s) inside the gate footprint: " + ", ".join(inside))
    if problem:
        find("MAJOR", "AUD-GATE-001",
             "Gate route fails because the road runs through a 65 m closed gate",
             "; ".join(problem) + ". The road corridor is 6 m wide and the gate footprint is "
             "%.0f × %.0f m placed at (180000, 121500) with complex collision on structural "
             "meshes, so no direct route exists while the doors are shut."
             % (footprint_x_m, half_depth * 2 / 100),
             ["QA/Canton_District/Traversal_Route_Matrix.json",
              "ContentSource/GuangzhouLandmarks/Wenmingmen/asset_manifest.json",
              "ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md"],
             "Decide the intent: move the gate off the corridor, open a passage, end the "
             "route at the threshold instead of beyond it, or waive directness with an owner "
             "note. Do not keep re-running the same unsatisfiable query.")
    return {"footprint_x_m": footprint_x_m, "detour_apex_cm": apex,
            "half_width_plus_agent_cm": half + agent, "closed_door": closed_door,
            "endpoints_inside_gate": inside}


def audit_nanite_availability():
    """A subcheck the host cannot run must say so, not read as merely pending."""
    readme = (ROOT / "ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md").read_text()
    handoff = HANDOFF.read_text()
    unavailable = "Nanite is unavailable" in readme
    if unavailable and "Nanite" in handoff and "unavailable" not in handoff:
        find("MINOR", "AUD-NANITE-001",
             "Nanite subcheck is unavailable on this host, not merely not run",
             "The gate package records that on Metal SM5 Nanite is unavailable and each mesh "
             "falls back to raster, while the handoff lists 'Nanite compatibility NOT RUN' "
             "and the acceptance budget names this same Mac as the proposed target hardware.",
             ["Review/M05_Final_Handoff.md",
              "ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md",
              "Docs/Terrain_Acceptance_Budget.md"],
             "State that the host cannot evaluate the Nanite gate, and either change the "
             "target hardware or declare the gate inapplicable with owner approval.")
    return {"nanite_unavailable_on_host": unavailable}


def audit_scope_language():
    """Reported numbers must not silently replace the budget's own targets."""
    budget = (ROOT / "Docs/Terrain_Acceptance_Budget.md").read_text()
    handoff = HANDOFF.read_text()
    notes = []
    for token in ("33.3", "50 ms", "12 GiB"):
        if token in budget and token not in handoff:
            notes.append("handoff drops budget target " + token)
    if notes:
        find("INFO", "AUD-SCOPE-001", "Handoff restates the budget incompletely",
             "; ".join(notes), ["Docs/Terrain_Acceptance_Budget.md",
                                "Review/M05_Final_Handoff.md"],
             "Quote the numeric targets the runtime gate will be judged against.")
    return notes


report = {
    "scope": "Independent audit of the M05 Canton provisional district delivery",
    "reviewer": "test/QA pass; read-only; no Unreal process opened",
    "handoff_sha256": sha(HANDOFF),
    "manifest_sha256": sha(MANIFEST),
    "checks": {
        "manifest": audit_manifest(),
        "manifest_coverage": audit_manifest_coverage(),
        "freeze": audit_freeze(),
        "duplicate_evidence": sorted(audit_duplicate_evidence()),
        "handoff_links": audit_handoff_links(),
        "view_duplication": audit_view_duplication(),
        "traversal": audit_traversal(),
        "r16_cross_check": audit_r16_cross_check(),
        "nav_vs_road": audit_nav_vs_road(),
        "build_result_consistency": audit_build_result_consistency(),
        "declared_thresholds": audit_declared_thresholds(),
        "validator_tolerances": audit_validator_tolerances(),
        "slab_float": audit_slab_float(),
        "day14_waiver": audit_day14_waiver(),
        "gate_conflict": audit_gate_conflict(),
        "nanite_availability": audit_nanite_availability(),
        "drainage": audit_drainage(),
        "capture_pair": audit_capture_pair(),
        "scope_language": audit_scope_language(),
    },
    "findings": findings,
    "counts": {level: sum(1 for f in findings if f["severity"] == level)
               for level in ("BLOCKER", "MAJOR", "MINOR", "INFO")},
}
OUT.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"counts": report["counts"],
                  "findings": [(f["severity"], f["id"], f["title"]) for f in findings]},
                 indent=2))
