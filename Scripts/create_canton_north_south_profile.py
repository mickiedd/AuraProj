"""Export a reproducible south-to-north profile of the provisional M01 R16."""
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
metadata = json.loads((ROOT / "Export/Provisional/Heightmap_Metadata.json").read_text())
raw = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
values = memoryview(raw.read_bytes()).cast("H")
size = metadata["vertices"]
assert len(values) == size * size and size == 2017
col = size // 2
points = []
for row in range(size):
    code = values[row * size + col]
    z = (code - metadata["code_zero"]) * metadata["metres_per_code"]
    north_m = row * metadata["spacing_m"]
    points.append((north_m, metadata["origin_northing_m"] + north_m,
                   col * metadata["spacing_m"], code, z))

folder = ROOT / "QA/Provisional"
with (folder / "North_South_Modern_Context_Profile.csv").open("w", newline="") as stream:
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["local_north_m", "utm_northing_m", "local_east_m",
                     "r16_code", "modern_context_z_m_egm2008", "historical_confidence"])
    for north, utm, east, code, z in points:
        writer.writerow([north, utm, east, code, f"{z:.6f}", "D_PROVISIONAL"])

zmin = min(point[4] for point in points)
zmax = max(point[4] for point in points)
x0, x1, y0, y1 = 88, 1050, 420, 105
def xy(row):
    north, _, _, _, z = row
    return f"{x0+(x1-x0)*north/4032:.2f},{y0-(y0-y1)*(z-zmin)/(zmax-zmin):.2f}"
polyline = " ".join(xy(point) for point in points)
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="520" viewBox="0 0 1120 520" role="img" aria-labelledby="title desc">
<title id="title">Canton provisional modern-context north-south terrain profile</title>
<desc id="desc">A center-column R16 profile from south to north, derived from modern EGM2008 context. It is D-confidence and does not represent accepted late-Qing elevations.</desc>
<rect width="1120" height="520" fill="#101929"/>
<text x="88" y="49" fill="#f3f2e9" font-family="sans-serif" font-size="25" font-weight="bold">Canton · provisional terrain profile</text>
<text x="88" y="79" fill="#b9cad6" font-family="sans-serif" font-size="15">Center column, x = 2016 m local · modern EGM2008 context · D-confidence</text>
<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y0}" stroke="#758b9c" stroke-width="2"/>
<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{y1}" stroke="#758b9c" stroke-width="2"/>
<polyline points="{polyline}" fill="none" stroke="#64d0d6" stroke-width="2.5"/>
<text x="{x0}" y="450" fill="#d2dce4" font-family="sans-serif" font-size="14">South · 0 m</text>
<text x="{x1-130}" y="450" fill="#d2dce4" font-family="sans-serif" font-size="14">North · 4032 m</text>
<text x="24" y="{y1+6}" fill="#d2dce4" font-family="sans-serif" font-size="13">{zmax:.1f} m</text>
<text x="24" y="{y0+5}" fill="#d2dce4" font-family="sans-serif" font-size="13">{zmin:.1f} m</text>
<rect x="88" y="471" width="962" height="31" rx="6" fill="#513342"/>
<text x="101" y="492" fill="#ffcfba" font-family="sans-serif" font-size="14">Historical walking surface and vertical datum remain blocked; this is not an 1880–1900 terrain profile.</text>
</svg>'''
(folder / "North_South_Modern_Context_Profile.svg").write_text(svg)
print("wrote", len(points), "profile stations", f"z {zmin:.2f}..{zmax:.2f} m")
