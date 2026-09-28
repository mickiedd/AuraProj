"""Check that the district remains a bounded, traversable engineering prototype."""
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
read = lambda path: json.loads((ROOT / path).read_text())
bounds = read("Maps/District_Test_Bounds.json")
roads = read("GIS/Road_Classifications.geojson")
drainage = read("GIS/Drainage_Locations.geojson")
build = read("QA/Canton_District/Build_Result.json")
refinement = read("QA/Canton_District/Slope_Refinement.json")
conformance = read("QA/Canton_District/Surface_Conformance.json")
material = read("QA/Canton_District/Landscape_Material.json")
wetness = read("QA/Canton_District/Wetness_Refinement.json")
capture_qa = read("QA/Canton_District/Capture_QA.json")
cook = read("QA/Canton_District/Cook_Result.json")
traversal = read("QA/Canton_District/Traversal_Route_Matrix.json")
assert bounds["historically_accepted"] is False
assert bounds["historical_metric_bounds"] is None
assert bounds["H2_status"].startswith("BLOCKED")
assert bounds["diagnostic_size_m"] == [200, 200]
assert build["historically_accepted"] is False
assert build["evidence_stage"] == "INITIAL_PRE_REFINEMENT_SNAPSHOT"
assert build["superseded_for_current_counts_by"] == "QA/Canton_District/Reload_Validation.json"
assert build["edit_layers_are_unmodified"] is True
assert build["gate_asset"].endswith("/BP_Wenmingmen")
assert refinement["removed_west_end_lane_slabs"] == 20
assert conformance["conformed_meshes"] == 476
assert conformance["removed_unsupported_large_parcel_pads"] == 16
assert material["paintable_layers"] == ["Soil", "Earth", "Pebble", "Grass", "Damp", "Stone"]
assert wetness["wet_patches"] == 14 and wetness["saved_actor_packages"] == 14
assert wetness["card_bottom_offset_cm"] <= 1 and wetness["cast_shadow"] is False
assert capture_qa["matched_camera"] and capture_qa["matched_target"]
assert capture_qa["changed_pixels_gt8"] > 20000
assert cook["passed"] and cook["packages_remaining"] == 0
assert len(cook["cooked_umaps"]) == 2
assert traversal["historically_accepted"] is False
assert traversal["pawn_traversal_performed"] is False
assert traversal["route_count"] == 7
assert traversal["route_check_pass_count"] == sum(
    route["route_check_pass"] for route in traversal["routes"])
assert traversal["all_route_checks_pass"] == all(
    route["route_check_pass"] for route in traversal["routes"])
assert all(route["floor_support_pass"] for route in traversal["routes"])
assert all(route["nav_corner_floor_samples"] for route in traversal["routes"])
assert all(sample["floor_z_cm"] is not None
           for route in traversal["routes"]
           for sample in route["nav_corner_floor_samples"])
gate_route = next(route for route in traversal["routes"]
                  if route["route"] == "gate_threshold_approach")
assert gate_route["end_xy_cm"] == [180000, 123500]
assert "staging" in gate_route["endpoint_role"]
assert gate_route["route_check_pass"] == (
    gate_route["floor_support_pass"] and gate_route["nav_path_valid"]
    and gate_route["nav_detour_ratio"] is not None
    and gate_route["nav_detour_ratio"] <= 1.5)
closed_gate = traversal["closed_gate_transit_diagnostic"]
assert closed_gate["route_check_pass"] is False
assert closed_gate["nav_detour_ratio"] > 1.5
assert (ROOT / "Content/Canton/DistrictPrototype/Terrain/M_Canton_Landscape_PROVISIONAL.uasset").is_file()
assert all(f["properties"]["historical_status"].startswith("D_") for f in roads["features"])
assert all(f["properties"]["historical_source"] is None for f in roads["features"])
assert len(drainage["features"]) == 3
assert all("not a historical canal" in f["properties"]["reason"] for f in drainage["features"])
assert {f["properties"]["flow_to"] for f in drainage["features"]} == {
    "ENG-OUTLET-SOUTH", "ENG-OUTLET-CENTRAL"}
grades = list(csv.DictReader((ROOT / "QA/Street_Grade_Profiles.csv").open()))
assert {row["road_id"] for row in grades} == {"ENG-ROAD-01", "ENG-ROAD-02", "ENG-DRAIN-01"}
assert max(abs(float(row["step_slope_percent"])) for row in grades if row["step_slope_percent"]) < 8
gutter = {int(row["y_ue_cm"]): float(row["modern_surface_m_egm2008"])
          for row in grades if row["road_id"] == "ENG-DRAIN-01"}
for feature in drainage["features"]:
    start_y, end_y = [130000 + int(point[1] * 100) for point in feature["geometry"]["coordinates"]]
    step = 400 if end_y > start_y else -400
    heights = [gutter[y] for y in range(start_y, end_y + step, step)]
    assert all(second <= first for first, second in zip(heights, heights[1:])), feature["properties"]["id"]
review_path = ROOT / "QA/Canton_District/Reload_Validation.json"
if review_path.exists():
    review = json.loads(review_path.read_text())
    assert review["passed"], review["errors"]
    assert review["historically_accepted"] is False
    assert review["main_slabs"] == 100 and review["mixed_slabs"] == 76
    assert review["gutters"] == 48 and review["weed_instances"] == 38
    assert review["wet_patches"] == 14
    assert review["road_collision_checks"] == 100
    assert review["road_joint_comparisons"] == 198
    assert build["instance_counts"]["pebble"] - refinement["removed_west_end_lane_slabs"] == review["mixed_slabs"]
    assert build["instance_counts"]["plot"] - conformance["removed_unsupported_large_parcel_pads"] == review["broad_parcel_pads"]
    assert review["broad_parcel_pads"] == 0
print("Canton district contract: pass (historical acceptance remains blocked)")
