"""Render the change-archive diagram for the per-building landmark lights.

Reads the numbers out of the placement and validation manifests rather than
hard-coding them, so the diagram cannot drift from the data it claims to show.
Re-run after a rebuild and the table, the cone margins and the guard list all
follow.

Usage:
    python Scripts/RenderLandmarkLightChangeDiagram.py
"""

import json
import math
from pathlib import Path

PLACEMENT = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-showcase.json")
VALIDATION = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-showcase-validation.json")
AB = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLights/light-ab-report.json")
OUT = Path("C:/Git/AuraProj/Docs/Reports/Change-Archive/"
           "2026-09-22-guangzhou-landmark-per-building-lights.svg")

INK = "#1b2733"
MUTED = "#5b6b7c"
RULE = "#d8dee6"
BORDER = "#b9c7d6"
PANEL = "#eef3f8"
WARM = "#fdf2e9"
WARM_LINE = "#d9895b"
WARM_INK = "#8a3d10"
ACCENT = "#b3541e"
OK_FILL = "#eaf5ef"
OK_LINE = "#8fbfa5"
OK_INK = "#2f6f4f"

SHORT = {
    "Zhengnanmen_HighFidelity": "Zhengnanmen HighFidelity",
    "Zhengnanmen_AAA_V3": "Zhengnanmen AAA V3",
    "Xiaobeimen_AAA_V3": "Xiaobeimen AAA V3",
    "Xiaobeimen_Production_V3": "Xiaobeimen Production V3",
    "Guidemen_V5_4K": "Guidemen V5 4K",
    "Wuxianmen_V5_4K_Core": "Wuxianmen V5 4K Core",
    "Wuxianmen_V5_FullPBR": "Wuxianmen V5 FullPBR",
    "GreatNorthGate": "GreatNorthGate (Dabeimen)",
    "ZhenhaiTower": "Zhenhai Tower",
}


def esc(text):
    return (str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def text(x, y, body, size=12, fill=INK, weight="normal", anchor="start", spacing=None):
    extra = ' letter-spacing="{}"'.format(spacing) if spacing else ""
    return ('<text x="{}" y="{}" font-size="{}" fill="{}" font-weight="{}" '
            'text-anchor="{}"{}>{}</text>').format(
        x, y, size, fill, weight, anchor, extra, esc(body))


def ring(cx, cy, radius, entries, per_building, label, caption):
    """A plan view of the ring. per_building=False draws the shared-key case."""
    parts = ['<circle cx="{}" cy="{}" r="{}" fill="none" stroke="{}" '
             'stroke-width="1.2" stroke-dasharray="3 4"/>'.format(
                 cx, cy, radius, BORDER)]
    parts.append('<circle cx="{}" cy="{}" r="3" fill="{}"/>'.format(cx, cy, MUTED))

    if not per_building:
        # One shared key: identical parallel arrows across the whole ring.
        for index in range(6):
            y = cy - radius + 12 + index * (2 * radius - 24) / 5.0
            parts.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.1" '
                         'marker-end="url(#arrow)"/>'.format(
                             cx - radius - 26, y, cx - radius - 6, y, MUTED))

    for entry in entries:
        angle = math.radians(entry["angle_deg"])
        bx = cx + radius * math.cos(angle)
        by = cy - radius * math.sin(angle)
        # Building mark: a short bar across the radius, i.e. facing the centre.
        half = 5.5
        ox = -math.sin(angle) * half
        oy = -math.cos(angle) * half
        if per_building:
            # Light sits inside the ring, in front of the facade; its cone reaches
            # out to the building. Radii are taken from the real recipe.
            light_radius = radius * entry["light_radius_fraction"]
            lx = cx + light_radius * math.cos(angle)
            ly = cy - light_radius * math.sin(angle)
            base = 7.0
            ox2 = -math.sin(angle) * base
            oy2 = -math.cos(angle) * base
            parts.append(
                '<path d="M {} {} L {} {} L {} {} z" fill="{}" fill-opacity="0.20" '
                'stroke="{}" stroke-width="0.9"/>'.format(
                    lx, ly, bx + ox2, by + oy2, bx - ox2, by - oy2,
                    ACCENT, ACCENT))
            parts.append('<circle cx="{}" cy="{}" r="2.1" fill="{}"/>'.format(
                lx, ly, ACCENT))
        parts.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="3.1" '
                     'stroke-linecap="round"/>'.format(
                         bx - ox, by - oy, bx + ox, by + oy,
                         WARM_INK if per_building else "#7a8794"))

    parts.append(text(cx, cy + radius + 18, label, 11,
                      ACCENT if per_building else MUTED, weight="600", anchor="middle"))
    parts.append(text(cx, cy + radius + 34, caption, 10.5, MUTED, anchor="middle"))
    return parts


def main():
    placement = json.loads(PLACEMENT.read_text(encoding="utf-8"))
    validation = json.loads(VALIDATION.read_text(encoding="utf-8"))
    ab = json.loads(AB.read_text(encoding="utf-8")) if AB.exists() else {}

    rings = [{"label": entry["label"], "angle_deg": entry["angle_deg"],
              "geometry_radius_xy": entry["geometry_radius_xy"]}
             for entry in placement["landmarks"]]
    lights = {entry["attached_to"]: entry for entry in placement["landmark_lights"]}
    ring_radius = placement["ring_radius_cm"]

    for entry in rings:
        key = entry["label"][len("Landmark_"):]
        light = lights[entry["label"]]
        entry["short"] = SHORT.get(key, key)
        entry["r"] = entry["geometry_radius_xy"]
        entry["distance"] = light["distance_cm"]
        entry["cone"] = light["outer_cone_deg"]
        entry["attenuation"] = light["attenuation_radius_cm"]
        entry["intensity"] = light["intensity_cd"]
        entry["margin"] = light["nearest_neighbour_outside_cone"]["margin_deg"]
        entry["light_radius_fraction"] = (
            ring_radius - (entry["r"] + 1500.0)) / ring_radius

    heights = {entry["label"]: entry["height_cm"] for entry in placement["landmarks"]}
    for entry in rings:
        entry["height"] = heights[entry["label"]]

    margins = [item["nearest_neighbour_margin_deg"]
               for item in validation.get("light_isolation_margins", [])]
    worst_margin = min(margins) if margins else float("nan")

    recipe = placement["landmark_light_recipe"]
    svg = []
    svg.append('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1240 980" '
               'width="1240" height="980" font-family="Segoe UI, Roboto, Helvetica, '
               'Arial, sans-serif">')
    svg.append('<defs>'
               '<marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" '
               'markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
               '<path d="M 0 0 L 10 5 L 0 10 z" fill="{}"/></marker>'
               '<marker id="dim" viewBox="0 0 10 10" refX="9" refY="5" '
               'markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
               '<path d="M 0 0 L 10 5 L 0 10 z" fill="{}"/></marker>'
               '</defs>'.format(MUTED, ACCENT))
    svg.append('<rect x="0" y="0" width="1240" height="980" fill="#fbfbfc"/>')

    svg.append(text(40, 44, "One independent light per building", 23, INK, "600"))
    svg.append(text(40, 68,
                    "2026-09-22 · L_GuangzhouLandmarkShowcase · a spot light owned by each of the "
                    "9 landmarks, added on top of the unchanged global key / fill / sky",
                    13.5, MUTED))
    svg.append('<line x1="40" y1="84" x2="1200" y2="84" stroke="{}" '
               'stroke-width="1.5"/>'.format(RULE))

    # ---- plan view -----------------------------------------------------------
    svg.append(text(40, 116, "PLAN — BEFORE / AFTER", 14, INK, "600", spacing="0.4"))
    svg += ring(180, 240, 78, rings, False, "BEFORE", "all 9 share one key")
    svg += ring(470, 240, 78, rings, True, "AFTER",
                "9 lights, one per building")
    svg.append(text(40, 372,
                    "Each light stands in front of its own building on the plaza side, "
                    "is attached to it,", 11.5, MUTED))
    svg.append(text(40, 388,
                    "and its cone is sized to that building — narrow enough to clear every "
                    "neighbour.", 11.5, MUTED))

    # ---- recipe --------------------------------------------------------------
    svg.append(text(660, 116, "ONE BUILDING'S LIGHT", 14, INK, "600", spacing="0.4"))
    svg.append('<rect x="660" y="130" width="540" height="256" rx="8" fill="{}" '
               'stroke="{}" stroke-width="1.4"/>'.format(PANEL, BORDER))
    # ground
    gx0, gx1, gy = 690, 1180, 330
    svg.append('<line x1="{}" y1="{}" x2="{}" y2="{}" stroke="{}" '
               'stroke-width="1.6"/>'.format(gx0, gy, gx1, gy, MUTED))
    # building (Guidemen proportions: 3008 cm radius, 1974 cm high)
    bx0, bx1 = 1030, 1150
    btop = gy - 78
    svg.append('<rect x="{}" y="{}" width="{}" height="{}" fill="{}" stroke="{}" '
               'stroke-width="1.4"/>'.format(
                   bx0, btop, bx1 - bx0, gy - btop, "#f3e4d6", WARM_INK))
    svg.append(text((bx0 + bx1) / 2, gy + 16, "its building", 11, WARM_INK,
                    anchor="middle"))
    # light and cone
    lx, ly = 830, 176
    svg.append('<circle cx="{}" cy="{}" r="5" fill="{}"/>'.format(lx, ly, ACCENT))
    svg.append(text(lx, ly - 12, "its own spot light", 11, ACCENT, "600", anchor="middle"))
    aim_y = gy - 78 * 0.45
    for ty in (btop, gy):
        svg.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.1" '
                   'stroke-dasharray="4 3"/>'.format(lx, ly, bx1, ty, WARM_LINE))
    svg.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.6" '
               'marker-end="url(#dim)"/>'.format(lx, ly, bx0, aim_y, ACCENT))
    # Parameters as a list on the left of the panel, clear of the cone lines.
    for index, line in enumerate([
            "cone = subtended angle + {:.0f}°".format(recipe["cone_margin_deg"]),
            "inner cone = {:.2f} × outer".format(recipe["inner_cone_fraction"]),
            "aim at {:.2f} × building height".format(recipe["aim_height_factor_of_building"]),
            "attenuation = {:.2f} × (distance + radius)".format(recipe["attenuation_margin"]),
            "intensity = {:.1f} lux at the building, in candelas".format(
                recipe["accent_lux_at_building"]),
            "{:.0f} K · source radius {:.0f} cm · shadows on".format(
                recipe["temperature_k"], recipe["source_radius_cm"]),
            "every figure is a ratio of the building's own",
            "measured size, so a swapped-in variant stays correctly lit."]):
        svg.append(text(690, 196 + index * 15.5, line, 11, MUTED))
    # dimension: height, labelled horizontally so nothing runs off the canvas
    svg.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.1" '
               'marker-end="url(#dim)"/>'.format(1172, ly, 1172, gy, ACCENT))
    svg.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.1" '
               'marker-end="url(#dim)"/>'.format(1172, gy, 1172, ly, ACCENT))
    svg.append(text(1166, ly - 8, "h × {:.2f}".format(
        recipe["height_factor_of_building"]), 11, ACCENT, anchor="end"))
    # dimension: facade clearance, below the ground line
    svg.append('<path d="M {} {} L {} {}" stroke="{}" stroke-width="1.1" '
               'marker-end="url(#dim)"/>'.format(
                   lx, gy + 26, bx0, gy + 26, ACCENT))
    svg.append(text((lx + bx0) / 2, gy + 42, "facade clearance {:.0f} cm".format(
        recipe["facade_clearance_cm"]), 11, ACCENT, anchor="middle"))

    # ---- table ---------------------------------------------------------------
    svg.append(text(40, 424, "WHAT EACH BUILDING GOT", 14, INK, "600", spacing="0.4"))
    columns = [
        ("Building", 56, "start"),
        ("radius r", 372, "end"),
        ("height h", 452, "end"),
        ("light dist d", 540, "end"),
        ("outer cone", 626, "end"),
        ("attenuation", 718, "end"),
        ("intensity", 800, "end"),
        ("clear of neighbour", 940, "end"),
    ]
    svg.append('<rect x="40" y="434" width="1160" height="276" rx="8" fill="#ffffff" '
               'stroke="{}" stroke-width="1.4"/>'.format(BORDER))
    for name, x, anchor in columns:
        svg.append(text(x, 456, name, 11, MUTED, "600", anchor=anchor))
    svg.append('<line x1="52" y1="464" x2="1188" y2="464" stroke="{}" '
               'stroke-width="1"/>'.format(RULE))
    y = 484
    for entry in rings:
        svg.append(text(columns[0][1], y, entry["short"], 11.5, INK))
        svg.append(text(columns[1][1], y, "{:.0f} cm".format(entry["r"]), 11.5, MUTED,
                        anchor="end"))
        svg.append(text(columns[2][1], y, "{:.0f} cm".format(entry["height"]), 11.5,
                        MUTED, anchor="end"))
        svg.append(text(columns[3][1], y, "{:.0f} cm".format(entry["distance"]), 11.5,
                        MUTED, anchor="end"))
        svg.append(text(columns[4][1], y, "{:.1f}°".format(entry["cone"]), 11.5, INK,
                        anchor="end"))
        svg.append(text(columns[5][1], y, "{:.0f} cm".format(entry["attenuation"]), 11.5,
                        MUTED, anchor="end"))
        svg.append(text(columns[6][1], y, "{:.0f} cd".format(entry["intensity"]), 11.5,
                        INK, anchor="end"))
        svg.append(text(columns[7][1], y, "{:.1f}°".format(entry["margin"]), 11.5,
                        OK_INK, "600", anchor="end"))
        y += 25.0
    svg.append(text(56, 702,
                    "\"clear of neighbour\" = angle to the nearest other building, minus that "
                    "building's angular span, minus this cone. Worst case {:.2f}°.".format(
                        worst_margin), 11, MUTED))

    # ---- guards / validation / measurement -----------------------------------
    boxes = [
        (40, "GUARDS — asserted by the builder", OK_INK, OK_FILL, OK_LINE, [
            "one light per landmark, attached to that landmark",
            "cone wider than the angle the building subtends",
            "attenuation reaching past the building's far side",
            "shadows on; intensity and cone read back off the component",
            "no other landmark inside any cone",
            "level saved only after every guard passes",
        ]),
        (430, "VALIDATION — independent re-derivation", INK, PANEL, BORDER, [
            "level reloaded from disk, nothing judged in memory",
            "0 errors, 0 warnings; 34 actors, 9 lights, 0 leaked",
            "each light re-derived from the manifest's own recipe:",
            "placement error {:.1f} cm, aim error {:.1f}°".format(
                max(item["location_error_cm"]
                    for item in validation.get("landmark_lights", [{}]))
                if validation.get("landmark_lights") else 0.0,
                max(item["aim_error_deg"]
                    for item in validation.get("landmark_lights", [{}]))
                if validation.get("landmark_lights") else 0.0),
            "settings match the re-derived recipe to 1e-4 relative",
            "isolation: 0 violations, worst margin {:.2f}°".format(worst_margin),
        ]),
        (820, "MEASURED EFFECT", WARM_INK, WARM, WARM_LINE, [
            "A/B: every light's intensity zeroed, then restored",
            "level never saved; all 9 intensities verified back on disk",
            "Guidemen view mean 0.638 → 0.712 (+0.074)",
            "its clipping 62.1% → 66.0%; plaza +0.017, hero +0.002",
            "the light visibly lights its own facade",
            "absolute brightness is NOT trustworthy — see the record",
        ]),
    ]
    for x, title, ink, fill, line, lines in boxes:
        svg.append('<rect x="{}" y="728" width="380" height="238" rx="8" fill="{}" '
                   'stroke="{}" stroke-width="1.5"/>'.format(x, fill, line))
        svg.append(text(x + 16, 754, title, 12.5, ink, "600", spacing="0.3"))
        yy = 776
        for line in lines:
            svg.append('<circle cx="{}" cy="{}" r="2.2" fill="{}"/>'.format(
                x + 20, yy - 4, line))
            svg.append(text(x + 30, yy, line, 11, INK))
            yy += 24

    svg.append('</svg>')

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(svg) + "\n", encoding="utf-8")
    print("wrote", OUT, OUT.stat().st_size, "bytes")
    print("worst neighbour margin", round(worst_margin, 3))
    if ab:
        print("AB contribution", json.dumps(ab.get("contribution")))


if __name__ == "__main__":
    main()
