"""Build segregated, coarse map traces for review; never publish accepted GIS layers."""

import csv
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw
from pyproj import Transformer
from shapely.geometry import LineString, Point, Polygon


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "GIS/Provisional"
OUT.mkdir(parents=True, exist_ok=True)
traces = json.loads((ROOT / "Data/Map_Pixel_Traces_Provisional.json").read_text())
transform = json.loads((ROOT / "Data/Map_Transform_Provisional.json").read_text())
assert traces["map_source_id"] == transform["map_source_id"] == "MAP-001"
assert transform["acceptance"]["passed"] is False
to_wgs84 = Transformer.from_crs("EPSG:32649", "EPSG:4326", always_xy=True)
sx = traces["source_image_size_px"][0] / traces["reference_preview_size_px"][0]
sy = traces["source_image_size_px"][1] / traces["reference_preview_size_px"][1]


def locate(point, preview=True):
    x, y = point
    if preview:
        x, y = x * sx, y * sy
    e = transform["pixel_to_projected"]["easting_m"]
    n = transform["pixel_to_projected"]["northing_m"]
    east = e["pixel_x"] * x + e["pixel_y"] * y + e["offset"]
    north = n["pixel_x"] * x + n["pixel_y"] * y + n["offset"]
    lon, lat = to_wgs84.transform(east, north)
    return [lon, lat], [east, north]


layers = {name: [] for name in ("Wall", "Gates", "Roads_Main", "Waterways", "Landforms", "Landmarks")}
wall_projected = []
for item in traces["features"]:
    pixels = item["preview_pixels"] if item["geometry"] != "Point" else [item["preview_pixels"]]
    located = [locate(p) for p in pixels]
    coords = [entry[0] for entry in located]
    metric = [entry[1] for entry in located]
    geom = {"type": item["geometry"], "coordinates": coords[0] if item["geometry"] == "Point" else [coords] if item["geometry"] == "Polygon" else coords}
    if item["geometry"] == "Polygon":
        assert Polygon(metric).is_valid
    if item["geometry"] == "LineString":
        assert LineString(metric).is_simple
    if item["id"] == "WALL-OUTER-01":
        wall_projected = metric
        assert metric[0] == metric[-1]
        assert Polygon(metric).is_valid
    layers[item["layer"]].append({"type": "Feature", "geometry": geom, "properties": {
        "id": item["id"], "name": item["name"], "source_id": "MAP-001", "source_date": "1880",
        "georef_status": "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE", "historical_confidence": item["confidence"],
        "digitization": "coarse visual trace", "preview_pixel_coordinates": item["preview_pixels"],
        "metric_crs": "EPSG:32649", "metric_bounds_m": [round(min(p[0] for p in metric), 1), round(min(p[1] for p in metric), 1), round(max(p[0] for p in metric), 1), round(max(p[1] for p in metric), 1)]
    }})

with (ROOT / "Data/Map_Control_Candidates.csv").open(newline="", encoding="utf-8") as stream:
    for row in csv.DictReader(stream):
        coord, metric = locate((float(row["pixel_x"]), float(row["pixel_y"])), preview=False)
        layers["Landmarks"].append({"type": "Feature", "geometry": {"type": "Point", "coordinates": coord}, "properties": {
            "id": row["point_id"], "name": row["landmark"], "source_id": "MAP-001", "source_date": "1880",
            "georef_status": "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE", "historical_confidence": "D",
            "digitization": "map icon centre", "source_pixel_coordinates": [float(row["pixel_x"]), float(row["pixel_y"])],
            "location_source_id": row["location_source_id"], "metric_crs": "EPSG:32649",
            "metric_coordinates_m": [round(n, 1) for n in metric]
        }})

for name, features in layers.items():
    output = {"type": "FeatureCollection", "name": name, "metadata": {
        "status": "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE", "map_source_id": "MAP-001",
        "coordinates": "RFC 7946 WGS84 lon/lat", "working_metric_crs": "EPSG:32649",
        "source_pixel_trace": "Data/Map_Pixel_Traces_Provisional.json",
        "transform": "Data/Map_Transform_Provisional.json"
    }, "features": features}
    (OUT / f"{name}.geojson").write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")

east = [p[0] for p in wall_projected]
north = [p[1] for p in wall_projected]
bounds = [min(east), min(north), max(east), max(north)]
margin = 200
required = [bounds[0] - margin, bounds[1] - margin, bounds[2] + margin, bounds[3] + margin]
size = 4032
origin_e = math.floor(((required[0] + required[2] - size) / 2) / 100) * 100
origin_n = math.floor(((required[1] + required[3] - size) / 2) / 100) * 100
fits = required[0] >= origin_e and required[1] >= origin_n and required[2] <= origin_e + size and required[3] <= origin_n + size
assert fits, "Proposed 4032 m square does not fit the wall and 200 m buffer"
report = {"status": "PROVISIONAL_GEOMETRIC_FIT_ONLY", "wall_bounds_epsg32649_m": [round(v, 1) for v in bounds],
          "required_with_200m_buffer_epsg32649_m": [round(v, 1) for v in required],
          "proposed_origin_epsg32649_m": [origin_e, origin_n], "landscape_extent_m": size,
          "landscape_bounds_epsg32649_m": [origin_e, origin_n, origin_e + size, origin_n + size],
          "clearance_from_wall_m": [round(bounds[0]-origin_e, 1), round(bounds[1]-origin_n, 1), round(origin_e+size-bounds[2], 1), round(origin_n+size-bounds[3], 1)],
          "fits_with_200m_buffer": fits, "historically_accepted": False}
(ROOT / "Data/Extent_Provisional.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

# Human-checkable alignment image, drawn directly from the same stored pixels.
with Image.open(ROOT / "Sources/HistoricMaps/Canton_Vrooman_BritishLibrary_001954731.jpg") as original:
    overlay = original.convert("RGB")
overlay.thumbnail(tuple(traces["reference_preview_size_px"]))
draw = ImageDraw.Draw(overlay)
colors = {"Wall": "#e01f19", "Gates": "#a000c0", "Roads_Main": "#ff8200",
          "Waterways": "#1879c4", "Landforms": "#609b2c"}
for item in traces["features"]:
    points = item["preview_pixels"]
    color = colors[item["layer"]]
    if item["geometry"] == "Point":
        x, y = points
        draw.ellipse((x - 6, y - 6, x + 6, y + 6), fill=color, outline="white", width=2)
    else:
        draw.line([tuple(point) for point in points], fill=color,
                  width=4 if item["layer"] == "Wall" else 3)
overlay.save(ROOT / "QA/Canton_Provisional_Trace_Overlay.jpg", quality=88)
print(json.dumps({"feature_counts": {name: len(items) for name, items in layers.items()}, "extent": report}, indent=2))
