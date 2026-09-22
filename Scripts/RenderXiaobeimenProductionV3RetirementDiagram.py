"""Render the Change-Archive diagram for the Xiaobeimen Production V3 retirement.

Geometry is computed from the same formula the builder uses rather than drawn by
hand, so the before/after rings and the freed slot are where the level actually
put them: radius = sum(2 x radius_xy + 3000) / 2pi, then each landmark sits at
the angle its own radius and gap accumulate to.
"""

import math
from pathlib import Path

GAP_CM = 3000.0
MIN_RADIUS_CM = 8000.0

# radius_xy per landmark, as the showcase manifest measured them.
BEFORE = [
    ("Zhengnanmen HighFidelity", 1595.31, "plain"),
    ("Zhengnanmen AAA V3", 1504.16, "plain"),
    ("Xiaobeimen AAA V3", 2704.24, "survivor"),
    ("Xiaobeimen Production V3", 2718.00, "removed"),
    ("Guidemen", 3008.32, "plain"),
    ("Wuxianmen FullPBR", 1864.67, "plain"),
    ("GreatNorthGate", 2120.00, "plain"),
    ("ZhenhaiTower", 2459.54, "plain"),
]
REMOVED = "Xiaobeimen Production V3"
AFTER = [entry for entry in BEFORE if entry[0] != REMOVED]


def layout(entries):
    radius = max(MIN_RADIUS_CM,
                 sum(2.0 * r + GAP_CM for _, r, _ in entries) / (2.0 * math.pi))
    rows = []
    arc = 0.0
    for name, r, role in entries:
        arc += r
        rows.append((name, math.degrees(arc / radius), role))
        arc += r + GAP_CM
    return radius, rows


def gap_stats(rows):
    angles = sorted(theta for _, theta, _ in rows)
    gaps = [angles[i + 1] - angles[i] for i in range(len(angles) - 1)]
    gaps.append(360.0 - angles[-1] + angles[0])
    return min(gaps), max(gaps)


def project(cx, cy, radius, theta_deg):
    theta = math.radians(theta_deg)
    return cx + radius * math.cos(theta), cy - radius * math.sin(theta)


def ring(cx, cy, radius, rows, accent):
    out = []
    out.append('  <circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="%s" '
               'stroke-width="2" stroke-dasharray="6 6"/>' % (cx, cy, radius, accent))
    for name, theta, role in rows:
        x, y = project(cx, cy, radius, theta)
        out.append('  <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#c8ced3" '
                   'stroke-width="1"/>' % (cx, cy, x, y))
    return out


def markers(cx, cy, radius, rows):
    out = []
    for name, theta, role in rows:
        x, y = project(cx, cy, radius, theta)
        if role == "removed":
            out.append('  <circle cx="%.1f" cy="%.1f" r="11" fill="#efe0df" '
                       'stroke="#a06a6a" stroke-width="3"/>' % (x, y))
        elif role == "survivor":
            out.append('  <circle cx="%.1f" cy="%.1f" r="11" fill="#dfe6e1" '
                       'stroke="#4e6f63" stroke-width="3"/>' % (x, y))
        else:
            out.append('  <circle cx="%.1f" cy="%.1f" r="10" fill="#c3c8cb" '
                       'stroke="#8c949b" stroke-width="2"/>' % (x, y))
    return out


def gone_marker(cx, cy, radius, theta):
    x, y = project(cx, cy, radius, theta)
    return [
        '  <circle cx="%.1f" cy="%.1f" r="11" fill="none" stroke="#a06a6a" '
        'stroke-width="2" stroke-dasharray="4 4"/>' % (x, y),
        '  <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#a06a6a" '
        'stroke-width="2"/>' % (x - 7, y - 7, x + 7, y + 7),
        '  <line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#a06a6a" '
        'stroke-width="2"/>' % (x + 7, y - 7, x - 7, y + 7),
    ]


def text(x, y, body, size=13, weight="400", fill="#4b5560", anchor="start"):
    return ('  <text x="%.1f" y="%.1f" font-family="Arial" font-size="%d" '
            'font-weight="%s" fill="%s" text-anchor="%s">%s</text>'
            % (x, y, size, weight, fill, anchor, body))


def build():
    before_radius, before_rows = layout(BEFORE)
    after_radius, after_rows = layout(AFTER)
    before_gaps = gap_stats(before_rows)
    after_gaps = gap_stats(after_rows)
    removed_theta = dict((n, t) for n, t, _ in before_rows)[REMOVED]

    parts = []
    parts.append('<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="800" '
                 'viewBox="0 0 1200 800">')
    parts.append('  <rect width="1200" height="800" fill="#eef1f3"/>')
    parts.append(text(60, 52,
                      "Xiaobeimen Production V3 removed from the showcase, "
                      "asset folder deleted", 27, "700", "#20252a"))
    parts.append(text(60, 84,
                      "The Blueprint was placed in TWO levels, not one &#8212; both had to be "
                      "cleared before the asset could go", 16))

    # ---------------- panels ----------------
    parts.append('  <rect x="55" y="110" width="515" height="330" rx="16" '
                 'fill="#d9dcd9" stroke="#8c949b" stroke-width="2"/>')
    parts.append(text(82, 146, "Before", 21, "700", "#31363b"))
    parts.append(text(82, 170, "8 landmarks &#183; 8 lights &#183; 31 actors", 13))
    parts.append(text(82, 189, "ring radius %.1f cm" % before_radius, 13))
    parts.append('  <rect x="630" y="110" width="515" height="330" rx="16" '
                 'fill="#d9dcd9" stroke="#4e6f63" stroke-width="3"/>')
    parts.append(text(657, 146, "After", 21, "700", "#27483e"))
    parts.append(text(657, 170, "7 landmarks &#183; 7 lights &#183; 28 actors", 13))
    parts.append(text(657, 189, "ring radius %.1f cm" % after_radius, 13))

    lbx, lby, lbr = 312.0, 300.0, 122.0
    rbx, rby, rbr = 887.0, 300.0, 122.0
    parts += ring(lbx, lby, lbr, before_rows, "#8c949b")
    parts += markers(lbx, lby, lbr, before_rows)
    parts += ring(rbx, rby, rbr, after_rows, "#4e6f63")
    parts += markers(rbx, rby, rbr, after_rows)
    parts += gone_marker(rbx, rby, rbr, removed_theta)

    # the two Xiaobeimen slots, called out on the left
    x1, y1 = project(lbx, lby, lbr, dict((n, t) for n, t, _ in before_rows)
                     ["Xiaobeimen AAA V3"])
    x2, y2 = project(lbx, lby, lbr, removed_theta)
    parts.append(text(x1 - 4, y1 - 20, "AAA V3", 12, "700", "#27483e", "end"))
    parts.append(text(x1 - 4, y1 - 7, "stays", 11, "400", "#27483e", "end"))
    parts.append(text(x2 - 16, y2 - 6, "Production V3", 12, "700", "#7a2f2f", "end"))
    parts.append(text(x2 - 16, y2 + 8, "the duplicate", 11, "400", "#7a2f2f", "end"))

    xg, yg = project(rbx, rby, rbr, removed_theta)
    parts.append(text(xg - 18, yg + 4, "slot gone", 12, "700", "#7a2f2f", "end"))

    parts.append(text(lbx, lby + 4, "%.0f cm" % before_radius, 14, "700",
                      "#31363b", "middle"))
    parts.append(text(rbx, rby + 4, "%.0f cm" % after_radius, 14, "700",
                      "#27483e", "middle"))
    parts.append(text(lbx, 424,
                      "angular gap %.1f&#176;&#8211;%.1f&#176;" % before_gaps, 12,
                      "400", "#4b5560", "middle"))
    parts.append(text(rbx, 424,
                      "angular gap %.1f&#176;&#8211;%.1f&#176;" % after_gaps, 12,
                      "400", "#4b5560", "middle"))

    # ---------------- the two levels ----------------
    parts.append(text(60, 486, "Both owning levels, cleared before the asset was "
                               "deleted", 17, "700", "#20252a"))
    parts.append('  <rect x="55" y="502" width="515" height="118" rx="12" '
                 'fill="#dfe3e0" stroke="#4e6f63" stroke-width="2"/>')
    parts.append(text(76, 530, "L_GuangzhouLandmarkShowcase", 15, "700", "#27483e"))
    parts.append(text(76, 552, "ring showcase &#183; rebuilt from LANDMARKS", 13))
    parts.append(text(76, 572, "registry entry dropped + EXPECTED_LABELS entry", 13))
    parts.append(text(76, 592, "31 &#8594; 28 actors &#183; validator 0 errors / 0 warnings", 13))

    parts.append('  <rect x="630" y="502" width="515" height="118" rx="12" '
                 'fill="#dfe3e0" stroke="#4e6f63" stroke-width="2"/>')
    parts.append(text(651, 530, "Scifi_desert_city/L_showcase_level", 15, "700",
                      "#27483e"))
    parts.append(text(651, 552, "the second owner &#8212; found by the referencer query,", 13))
    parts.append(text(651, 572, "not by assumption; it is not the ring level", 13))
    parts.append(text(651, 592, "1786 &#8594; 1785 actors &#183; actor removed &#183; map saved", 13))

    # ---------------- deletion ----------------
    parts.append(text(60, 656, "Deletion", 17, "700", "#20252a"))
    parts.append('  <rect x="55" y="672" width="1090" height="104" rx="12" '
                 'fill="#efe0df" stroke="#a06a6a" stroke-width="2"/>')
    parts.append(text(76, 700,
                      "Safety gate passed: find_package_referencers_for_asset over all 176 assets "
                      "&#8594; outside referencers {} (empty)", 14, "700", "#7a2f2f"))
    parts.append(text(76, 724,
                      "176 assets deleted &#183; preview map then Blueprint, then 174 leaf-first "
                      "(textures &#8594; materials &#8594; meshes) &#183; 0 refusals", 14))
    parts.append(text(76, 748,
                      "delete_directory wedged the editor (game thread 0% CPU); per-asset "
                      "delete_asset did not &#8212; that is why the sweep is batched", 14))
    parts.append(text(76, 768,
                      "Blueprint, preview map and folder verified absent &#183; "
                      "Xiaobeimen_AAA_V3 (324 files) untouched", 14))
    parts.append('</svg>')
    return "\n".join(parts) + "\n"


if __name__ == "__main__":
    target = Path("Docs/Reports/Change-Archive/"
                  "2026-09-22-retire-xiaobeimen-production-v3.svg")
    target.write_text(build(), encoding="utf-8")
    print("wrote", target, target.stat().st_size, "bytes")
