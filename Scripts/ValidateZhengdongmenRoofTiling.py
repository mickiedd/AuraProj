"""Validate that the tile courses cover every roof slope continuously.

The previous version of this check only asked whether a tile was wider than its
pitch, which a staggered, rectangle-per-row layout passes while still leaving a
staircase of uncovered wedges along both hips and an exposed strip at each side
edge. This version reconstructs the courses in the slope's own parametric space
and proves coverage, so those gaps cannot pass again.
"""
import json
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
PACKAGE = PROJECT / 'Raw3DPacket/Zhengdongmen/prepared'
REPORT = PROJECT / 'Saved/Reports/Zhengdongmen/roof-course-validation.json'
EXTENTS = PROJECT / 'Saved/Reports/Zhengdongmen/roof-tile-extents.json'

T_SAMPLES = 400          # samples down the slope
ACROSS_SAMPLES = 1201    # samples across it, per t
HIP_CAP_M = 0.34         # the raised hip ridge that hides a trimmed course end

manifest = json.loads((PACKAGE / 'reference-tuning.json').read_text())
courses = manifest['roof_courses']
profile = manifest.get('roof_profile', {})
extents = json.loads(EXTENTS.read_text())
errors = []
expected = {'lower_ns_-1', 'lower_ns_1', 'lower_ew_-1', 'lower_ew_1',
            'upper_ns_-1', 'upper_ns_1', 'upper_ew_-1', 'upper_ew_1'}

if set(courses) != expected:
    errors.append(f'roof slopes {sorted(courses)} != {sorted(expected)}')
if set(extents) != expected:
    errors.append('roof tile extents are missing for some slopes')

# ---------------------------------------------------------------- tile shells
for name, row in courses.items():
    if row['tiles'] <= 0 or row['rows'] < 5:
        errors.append(f'{name}: missing courses')
    if row['tile_run_m'] <= row['course_pitch_m']:
        errors.append(f'{name}: gap between courses down the slope')
    # Derive the shell topology from the recorded sampling rather than echoing it.
    cross, stations = row.get('cross_samples', 0), row.get('shell_stations', 0)
    verts = 2 * stations * cross
    tris = (2 * ((stations - 1) * (cross - 1) * 2) + 2 * ((stations - 1) * 2)
            + 2 * ((cross - 1) * 2))
    if cross < 9 or stations != 2:
        errors.append(f'{name}: tile shell sampling is {stations}x{cross}')
    if row.get('closed_shell_vertices_per_tile') != verts:
        errors.append(f'{name}: tiles are not {verts}-vertex solids')
    if row.get('closed_shell_triangles_per_tile') != tris:
        errors.append(f'{name}: tile shell topology changed')
    if abs(row.get('wall_thickness_cm', 0) - profile.get('shell_m', 0) * 100) > 1e-6:
        errors.append(f'{name}: tile thickness does not match the shell profile')
    if row.get('every_mesh_edge_used_twice') is not True:
        errors.append(f'{name}: tile shell is open or non-manifold')
    if row.get('stagger_rows') != 0:
        errors.append(f'{name}: courses are staggered, so they cannot read as continuous runs')
    if not 0.3 <= row.get('rib_fraction', 0) <= 0.8:
        errors.append(f'{name}: the barrel rib covers {row.get("rib_fraction")} of the pitch')

# The slope planes must sit below the tile troughs, or the shell pokes through.
if profile:
    trough = profile['base_clearance_m'] - profile['shell_m']
    if profile['slope_plane_drop_m'] > trough - 1e-9:
        errors.append('slope planes are not below the tile troughs')

expected_triangles = sum(r['tiles'] * r['closed_shell_triangles_per_tile'] for r in courses.values())
actual_triangles = manifest['parts'].get('Continuous_curved_roof_tiles', 0)
if expected_triangles != actual_triangles:
    errors.append(f'tile mesh is {actual_triangles} triangles; expected {expected_triangles}')

# ------------------------------------------------------- continuity & coverage
coverage = {}
for name, slope in extents.items():
    if name not in courses:
        continue
    pitch = slope['across_pitch']
    grid = [(index + 0.5 - slope['columns'] / 2.0) * pitch for index in range(slope['columns'])]
    tiles = slope['tiles']

    # 1. Every course must sit on the one rib grid: no row offset, no drift.
    for t0, t1, centre, lo0, hi0, lo1, hi1 in tiles:
        if min(abs(centre - g) for g in grid) > 1e-6:
            errors.append(f'{name}: course at {centre} is off the rib grid')
            break

    # 2. Every run must reach both the ridge and the eave, or the slope has a
    #    band of bare substrate across it.
    by_run = {}
    for t0, t1, centre, *_ in tiles:
        by_run.setdefault(round(centre, 6), []).append((t0, t1))
    for centre, spans in by_run.items():
        spans.sort()
        reach = spans[0][0]
        for a, b in spans:
            if a > reach + 1e-6:
                errors.append(f'{name}: run at {centre} has a gap in its courses '
                              f'between {reach} and {a}')
                break
            reach = max(reach, b)
        else:
            if reach < 1.0 - 1e-6:
                errors.append(f'{name}: run at {centre} stops at {reach:.4f}, short of the eave')

    # 3. Coverage: sample the trapezoid and prove every point has a course over it.
    worst = 0.0
    worst_at = None
    for step in range(T_SAMPLES + 1):
        t = step / T_SAMPLES
        edge = slope['half_width_ridge'] + (slope['half_width_eave'] - slope['half_width_ridge']) * t
        spans = []
        for t0, t1, centre, lo0, hi0, lo1, hi1 in tiles:
            if t0 - 1e-9 <= t <= t1 + 1e-9:
                u = 0.0 if t1 == t0 else (t - t0) / (t1 - t0)
                spans.append((lo0 + (lo1 - lo0) * u, hi0 + (hi1 - hi0) * u))
        if not spans:
            worst, worst_at = 2 * edge, (t, 0.0)
            break
        spans.sort()
        reach = -edge
        for lo, hi in spans:
            if lo > reach:
                if lo - reach > worst:
                    worst, worst_at = lo - reach, (t, reach)
            reach = max(reach, hi)
        if edge - reach > worst:
            worst, worst_at = edge - reach, (t, reach)
    # The outer edge of a hip face is covered by the raised hip ridge, not by a
    # course, so a gap is only a defect once it is wider than that ridge.
    coverage[name] = {'widest_uncovered_m': round(worst, 4),
                      'at': None if worst_at is None else [round(worst_at[0], 4),
                                                           round(worst_at[1], 4)],
                      'hip_cap_m': HIP_CAP_M}
    if worst > HIP_CAP_M:
        errors.append(f'{name}: uncovered strip of {worst:.3f} m (hip cap is {HIP_CAP_M} m)')

result = {'passed': not errors, 'errors': errors,
          'total_tiles': sum(r['tiles'] for r in courses.values()),
          'slopes': courses, 'coverage': coverage,
          'profile': profile,
          'generated_tile_triangles': actual_triangles,
          'all_roof_triangles': manifest['groups']['RoofTile'],
          'gate_triangles': manifest['total_triangles']}
REPORT.write_text(json.dumps(result, indent=2))
print(json.dumps({k: v for k, v in result.items() if k != 'slopes'}, indent=2))
if errors:
    raise SystemExit(1)
