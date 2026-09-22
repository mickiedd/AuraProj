"""Render the Change-Archive diagram for the Zhengnanmen AAA V3 retirement.

Geometry is computed from the same formula the builder uses rather than drawn by
hand, so the before/after rings are where the level actually put them:
radius = max(8000, sum(2 x radius_xy + 3000) / 2pi), then each landmark sits at
the angle its own radius and gap accumulate to.

The numbers in the side panels come from the run's own output:
  ring      - CreateGuangzhouLandmarkShowcase.py manifest (actors_removed_before_rebuild,
              landmark_light_recipe, landmarks) and ValidateGuangzhouLandmarkShowcase.py
  second    - RemoveShowcaseLevelZhengnanmenAAAV3.py
  deletion  - RetireZhengnanmenAAAV3.py
"""

import math
from pathlib import Path

GAP_CM = 3000.0
MIN_RADIUS_CM = 8000.0

# radius_xy per landmark, as the showcase manifest measured them. This is the
# 7-landmark ring this job started from, i.e. after Xiaobeimen Production V3 was
# retired earlier the same day.
BEFORE = [
    ("Zhengnanmen HighFidelity", 1595.31, "survivor"),
    ("Zhengnanmen AAA V3", 1504.16, "removed"),
    ("Xiaobeimen AAA V3", 2704.24, "plain"),
    ("Guidemen", 3008.32, "plain"),
    ("Wuxianmen FullPBR", 1864.67, "plain"),
    ("GreatNorthGate", 2120.00, "plain"),
    ("ZhenhaiTower", 2459.54, "plain"),
]
REMOVED = "Zhengnanmen AAA V3"
AFTER = [entry for entry in BEFORE if entry[0] != REMOVED]

RING_ACTORS_BEFORE, RING_ACTORS_AFTER = 28, 25
SECOND_LEVEL_BEFORE, SECOND_LEVEL_AFTER = 1785, 1784
DELETED_ASSETS = 113
MESHES, TEXTURES, MATERIALS = 61, 43, 7


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
    out = ['  <circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="%s" '
           'stroke-width="2" stroke-dasharray="6 6"/>' % (cx, cy, radius, accent)]
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
    before_theta = dict((n, t) for n, t, _ in before_rows)
    removed_theta = before_theta[REMOVED]

    parts = []
    parts.append('<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="800" '
                 'viewBox="0 0 1200 800">')
    parts.append('  <rect width="1200" height="800" fill="#eef1f3"/>')
    parts.append(text(60, 52,
                      "Zhengnanmen AAA V3 removed from the showcase, "
                      "asset folder deleted", 27, "700", "#20252a"))
    parts.append(text(60, 84,
                      "The second near-duplicate variant retired the same day, and again the "
                      "Blueprint was placed in TWO levels &#8212; both cleared first", 16))

    # ---------------- panels ----------------
    parts.append('  <rect x="55" y="110" width="515" height="330" rx="16" '
                 'fill="#d9dcd9" stroke="#8c949b" stroke-width="2"/>')
    parts.append(text(82, 146, "Before", 21, "700", "#31363b"))
    parts.append(text(82, 170, "7 landmarks &#183; 7 lights &#183; %d actors"
                                % RING_ACTORS_BEFORE, 13))
    parts.append(text(82, 189, "ring radius %.1f cm" % before_radius, 13))
    parts.append('  <rect x="630" y="110" width="515" height="330" rx="16" '
                 'fill="#d9dcd9" stroke="#4e6f63" stroke-width="3"/>')
    parts.append(text(657, 146, "After", 21, "700", "#27483e"))
    parts.append(text(657, 170, "6 landmarks &#183; 6 lights &#183; %d actors"
                                % RING_ACTORS_AFTER, 13))
    parts.append(text(657, 189, "ring radius %.1f cm &#8212; at the 8000 floor"
                                % after_radius, 13))

    lbx, lby, lbr = 312.0, 300.0, 122.0
    rbx, rby, rbr = 887.0, 300.0, 122.0
    parts += ring(lbx, lby, lbr, before_rows, "#8c949b")
    parts += markers(lbx, lby, lbr, before_rows)
    parts += ring(rbx, rby, rbr, after_rows, "#4e6f63")
    parts += markers(rbx, rby, rbr, after_rows)
    parts += gone_marker(rbx, rby, rbr, removed_theta)

    # the two Zhengnanmen slots, called out on the left
    x1, y1 = project(lbx, lby, lbr, before_theta["Zhengnanmen HighFidelity"])
    x2, y2 = project(lbx, lby, lbr, removed_theta)
    parts.append(text(x1 - 4, y1 - 20, "HighFidelity", 12, "700", "#27483e", "end"))
    parts.append(text(x1 - 4, y1 - 7, "stays", 11, "400", "#27483e", "end"))
    parts.append(text(x2 + 16, y2 + 4, "AAA V3", 12, "700", "#7a2f2f", "start"))
    parts.append(text(x2 + 16, y2 + 18, "the duplicate", 11, "400", "#7a2f2f", "start"))

    xg, yg = project(rbx, rby, rbr, removed_theta)
    parts.append(text(xg + 18, yg + 4, "slot gone", 12, "700", "#7a2f2f", "start"))

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
    parts.append(text(76, 592, "%d &#8594; %d actors &#183; validator 0 errors / 0 warnings"
                                % (RING_ACTORS_BEFORE, RING_ACTORS_AFTER), 13))

    parts.append('  <rect x="630" y="502" width="515" height="118" rx="12" '
                 'fill="#dfe3e0" stroke="#4e6f63" stroke-width="2"/>')
    parts.append(text(651, 530, "Scifi_desert_city/L_showcase_level", 15, "700",
                      "#27483e"))
    parts.append(text(651, 552, "the second owner &#8212; found by the referencer query,", 13))
    parts.append(text(651, 572, "not by assumption; it is not the ring level", 13))
    parts.append(text(651, 592, "%d &#8594; %d actors &#183; actor removed &#183; map saved"
                                % (SECOND_LEVEL_BEFORE, SECOND_LEVEL_AFTER), 13))

    # ---------------- deletion ----------------
    parts.append(text(60, 656, "Deletion", 17, "700", "#20252a"))
    parts.append('  <rect x="55" y="672" width="1090" height="104" rx="12" '
                 'fill="#efe0df" stroke="#a06a6a" stroke-width="2"/>')
    parts.append(text(76, 700,
                      "Safety gate passed: find_package_referencers_for_asset over all %d assets "
                      "&#8594; outside referencers {} (empty)" % DELETED_ASSETS,
                      14, "700", "#7a2f2f"))
    parts.append(text(76, 724,
                      "%d assets deleted &#183; preview map then Blueprint, then %d leaf-first "
                      "(textures %d &#8594; materials %d &#8594; meshes %d) &#183; 0 refusals"
                      % (DELETED_ASSETS, MESHES + TEXTURES + MATERIALS,
                         TEXTURES, MATERIALS, MESHES), 14))
    parts.append(text(76, 748,
                      "delete_directory deliberately NOT used &#8212; it wedged the editor on the "
                      "176-asset Xiaobeimen folder earlier today; delete_asset did not", 14))
    parts.append(text(76, 768,
                      "Blueprint, preview map and folder verified absent in BOTH levels &#183; "
                      "Zhengnanmen_HighFidelity untouched", 14))
    parts.append('</svg>')
    return "\n".join(parts) + "\n"


if __name__ == "__main__":
    target = Path("Docs/Reports/Change-Archive/"
                  "2026-09-22-retire-zhengnanmen-aaa-v3.svg")
    target.write_text(build(), encoding="utf-8")
    print("wrote", target, target.stat().st_size, "bytes")
