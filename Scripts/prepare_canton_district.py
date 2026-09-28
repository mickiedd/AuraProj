"""Version the local engineering layout and derive its modern-context grade checks."""
import csv
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = json.loads((ROOT / "Data/Canton_Prototype_Contract.json").read_text())
RAW = ROOT / CONTRACT["raw_height_path"]
HEIGHTS = memoryview(RAW.read_bytes()).cast("H")
CENTER = (180000, 130000)  # centimetres in the M01 diagnostic UE frame


def save_json(relative, value):
    path = ROOT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def height_m(x_cm, y_cm):
    col = max(0, min(2016, round(x_cm / 200)))
    row = max(0, min(2016, round(y_cm / 200)))
    return (HEIGHTS[row * 2017 + col] - 32768) * 50 / 12800


def line(name, surface, points, reason):
    return {
        "type": "Feature",
        "properties": {
            "id": name, "surface_class": surface,
            "historical_status": "D_PROVISIONAL_ENGINEERING_LAYOUT",
            "historical_source": None, "source_date": None,
            "reason": reason,
            "coordinate_frame": "local_diagnostic_metres_about_180000_130000_cm",
        },
        "geometry": {"type": "LineString", "coordinates": points},
    }


def drain_segment(name, points, outlet, high_y, low_y):
    feature = line(name, "covered_gutter", points,
                   "Unverified engineering flow test; placement is not a historical canal")
    feature["properties"].update({"flow_to": outlet,
                                   "high_y_local_m": high_y, "low_y_local_m": low_y})
    return feature


save_json("GIS/Road_Classifications.geojson", {
    "type": "FeatureCollection",
    "name": "Canton_District_Diagnostic_Roads",
    "description": "Engineering corridors only; not a surveyed or traced late-Qing street plan.",
    "features": [
        line("ENG-ROAD-01", "principal_stone_slab", [[0, -100], [0, 100]],
             "Tests 6 m gate-to-interior traversal and paving cadence"),
        line("ENG-ROAD-02", "mixed_pebble_earth_lane", [[-60, 20], [100, 20]],
             "Tests 3.2 m junction and earth transition"),
        line("ENG-DRAIN-01", "covered_gutter", [[6.5, -90], [6.5, 100]],
             "Tests side-drain clearance; no historical drain is claimed"),
    ],
})
save_json("GIS/Drainage_Locations.geojson", {
    "type": "FeatureCollection", "name": "Canton_Diagnostic_Drainage",
    "features": [
        drain_segment("ENG-DRAIN-01A", [[6.5, -22], [6.5, -90]],
                      "ENG-OUTLET-SOUTH", -22, -90),
        drain_segment("ENG-DRAIN-01B", [[6.5, -22], [6.5, 34]],
                      "ENG-OUTLET-CENTRAL", -22, 34),
        drain_segment("ENG-DRAIN-01C", [[6.5, 98], [6.5, 34]],
                      "ENG-OUTLET-CENTRAL", 98, 34),
    ],
})
save_json("GIS/District_Parcel_Reference.geojson", {
    "type": "FeatureCollection", "name": "Canton_Diagnostic_Parcel_References",
    "description": "Non-rendered engineering plots; no claim of a traced late-Qing parcel boundary.",
    "features": [
        {"type": "Feature", "properties": {
            "id": "ENG-PARCEL-%02d" % i,
            "historical_status": "D_PROVISIONAL_ENGINEERING_LAYOUT",
            "historical_source": None,
            "coordinate_frame": "local_diagnostic_metres_about_180000_130000_cm",
        }, "geometry": {"type": "Polygon", "coordinates": [[
            [xmin, ymin], [xmax, ymin], [xmax, ymax], [xmin, ymax], [xmin, ymin],
        ]]}}
        for i, (xmin, ymin, xmax, ymax) in enumerate([
            (-90, -75, -12, 5), (12, -75, 90, 5),
            (-90, 35, -12, 90), (12, 35, 90, 90),
        ], 1)
    ],
})
save_json("Maps/District_Test_Bounds.json", {
    "name": "Wenmingmen named gate-interface study on an unrelated diagnostic terrain sample",
    "map": "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL",
    "historically_accepted": False,
    "H2_status": "BLOCKED_NO_SURVEYED_LOCAL_FRAME",
    "historical_metric_bounds": None,
    "diagnostic_center_ue_cm": list(CENTER),
    "diagnostic_size_m": [200, 200],
    "diagnostic_modern_raster_epsg32649_bounds_m": [
        CONTRACT["origin_easting_m"] + (CENTER[0] - 10000) / 100,
        CONTRACT["origin_northing_m"] + (CENTER[1] - 10000) / 100,
        CONTRACT["origin_easting_m"] + (CENTER[0] + 10000) / 100,
        CONTRACT["origin_northing_m"] + (CENTER[1] + 10000) / 100,
    ],
    "warning": "The diagnostic bounds do not locate the historical Wenmingmen district.",
})
save_json("Data/Foliage_SpawnRules.json", {
    "mode": "ordinary_static_mesh_instances_PC G_preflight_unavailable".replace(" ", ""),
    "historical_status": "D_PROVISIONAL",
    "placement": "deterministic_sparse_road_edge",
    "grass_count_target": 40,
    "exclusions_cm": {"main_road_from_center_x": 800,
                       "mixed_lane_from_y_132000": 300,
                       "gate_opening": "no growth within paved corridor"},
    "collision": False,
})

surface_rows = [
    ("principal_stone_slab", "M_District_Stone", "none; neutral color only", "D"),
    ("compacted_earth", "M_District_Earth", "none; neutral color only", "D"),
    ("mixed_pebble_earth", "M_District_Pebble", "none; neutral color only", "D"),
    ("bare_soil", "M_District_Soil", "none; neutral color only", "D"),
    ("weed_soil", "M_District_Grass", "none; neutral color only", "D"),
    ("damp_earth", "M_District_Damp", "none; neutral color only", "D"),
]
for relative, header, rows in [
    ("Data/Road_Surface_Register.csv",
     ["road_id", "surface_class", "source_id", "source_date", "confidence", "status"],
     [["ENG-ROAD-01", "principal_stone_slab", "", "", "D", "engineering_only"],
      ["ENG-ROAD-02", "mixed_pebble_earth_lane", "", "", "D", "engineering_only"],
      ["ENG-DRAIN-01A", "covered_gutter", "", "", "D", "engineering_only"],
      ["ENG-DRAIN-01B", "covered_gutter", "", "", "D", "engineering_only"],
      ["ENG-DRAIN-01C", "covered_gutter", "", "", "D", "engineering_only"]]),
    ("Data/Material_Evidence_Register.csv",
     ["surface_family", "ue_material", "evidence", "historical_confidence"], surface_rows),
]:
    path = ROOT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(header)
        writer.writerows(rows)

profile_path = ROOT / "QA/Street_Grade_Profiles.csv"
profile_path.parent.mkdir(parents=True, exist_ok=True)
with profile_path.open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(["road_id", "station_m", "x_ue_cm", "y_ue_cm", "modern_surface_m_egm2008",
                     "step_slope_percent", "historical_status"])
    for road_id, stations in [
        ("ENG-ROAD-01", [(CENTER[0], y) for y in range(CENTER[1]-10000, CENTER[1]+10001, 200)]),
        ("ENG-ROAD-02", [(x, CENTER[1]+2000) for x in range(CENTER[0]-6000, CENTER[0]+10001, 200)]),
        ("ENG-DRAIN-01", [(CENTER[0]+650, y) for y in range(CENTER[1]-9000, CENTER[1]+10001, 400)]),
    ]:
        previous = None
        for station, (x, y) in enumerate(stations):
            h = height_m(x, y)
            grade = None if previous is None else 100 * (h - previous[2]) / (math.hypot(x-previous[0], y-previous[1]) / 100)
            writer.writerow([road_id, station * (2 if road_id != "ENG-DRAIN-01" else 4), x, y,
                             f"{h:.4f}", "" if grade is None else f"{grade:.3f}", "modern_diagnostic_only"])
            previous = (x, y, h)

print("Prepared explicit provisional district layout and modern-context grade profiles")
