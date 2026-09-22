"""Measure each landmark light's strength on the pixels it actually lights.

Why this exists: the per-building report compares lights with the MEAN luminance
delta over the whole frame. That figure is contaminated by framing - a light on a
short building covers fewer pixels, so it scores lower even when its facade is lit
just as hard. Zhengnanmen HighFidelity (2302 cm tall) fills far more of its frame
than Wuxianmen FullPBR (1721 cm), so their whole-frame deltas are not comparable,
and the earlier reading that "the two Wuxianmen lights are weakest" may be that
artefact rather than a property of the lights.

This recomputes the comparison on the right basis, from the same frames:

  coverage_pct        share of the frame the light measurably brightens - how much
                      of the picture the light reaches
  mean_delta_lit      mean brightening ON THOSE PIXELS - how hard it lights them,
                      which is the figure comparable across differently sized
                      buildings
  max_delta           the strongest single-pixel brightening

Run with an interpreter that has PIL:
    <venv>/Scripts/python.exe Scripts/AnalyzeLandmarkLightContribution.py
"""

import json
from pathlib import Path

from PIL import Image

SRC = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLightsPerBuilding")
REPORT = SRC / "per-building-report.json"
OUT = SRC / "light-strength-analysis.json"
THRESHOLD = 8.0 / 255.0


def luminance_map(path):
    image = Image.open(path).convert("L")
    return [value / 255.0 for value in image.getdata()]


def main():
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    rows = []
    for entry in report["buildings"]:
        on = luminance_map(entry["on_file"])
        off = luminance_map(entry["off_file"])
        deltas = [a - b for a, b in zip(on, off)]
        lit = [value for value in deltas if value > THRESHOLD]
        count = len(deltas)
        rows.append({
            "key": entry["key"],
            "frame_mean_delta": entry["mean_delta"],
            "coverage_pct": round(len(lit) / count * 100.0, 2),
            "mean_delta_lit": round(sum(lit) / len(lit), 4) if lit else 0.0,
            "max_delta": round(max(deltas), 4),
            "p90_delta_lit": round(sorted(lit)[int(len(lit) * 0.9)], 4) if lit else 0.0,
        })

    # The fair comparison: strength on lit pixels, not on the whole frame.
    strengths = [row["mean_delta_lit"] for row in rows]
    coverages = [row["coverage_pct"] for row in rows]
    summary = {
        "basis": "mean brightening on the pixels the light measurably brightens "
                 "(threshold {:.4f} luminance)".format(THRESHOLD),
        "mean_delta_lit_min": min(strengths),
        "mean_delta_lit_max": max(strengths),
        "mean_delta_lit_spread": round(max(strengths) / min(strengths), 2)
        if min(strengths) > 0 else None,
        "coverage_pct_min": min(coverages),
        "coverage_pct_max": max(coverages),
        "coverage_spread": round(max(coverages) / min(coverages), 2)
        if min(coverages) > 0 else None,
    }

    print("%-30s %10s %10s %14s %10s" % ("building", "frame Δ", "coverage%",
                                         "Δ on lit px", "max Δ"))
    for row in sorted(rows, key=lambda item: -item["mean_delta_lit"]):
        print("%-30s %10.4f %10.2f %14.4f %10.4f" % (
            row["key"], row["frame_mean_delta"], row["coverage_pct"],
            row["mean_delta_lit"], row["max_delta"]))
    print()
    print("strength spread (max/min on lit pixels): {:.2f}x".format(
        summary["mean_delta_lit_spread"]))
    print("coverage spread (max/min of frame share): {:.2f}x".format(
        summary["coverage_spread"]))

    (OUT).write_text(json.dumps({"summary": summary, "buildings": rows}, indent=2),
                     encoding="utf-8")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
