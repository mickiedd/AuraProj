"""Derive replaceable proxy-wall openings and bridge pieces from the current gates.

This is visual engineering on a rejected map affine, not a historical wall survey.
Only the eight gate buildings are opened; Zhenhai Tower remains on the trace.
"""
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / 'Data/Canton_WalledCity_Provisional_Wall_Gate_Fit_Config.json'
CONFIG = json.loads(CONFIG_PATH.read_text())
PLACEMENTS_PATH = ROOT / CONFIG['source_landmark_placements']
REGISTER_PATH = ROOT / CONFIG['source_landmark_register']
OVERLAY_PATH = ROOT / CONFIG['source_wall_trace']
PLACEMENTS = json.loads(PLACEMENTS_PATH.read_text())
REGISTER = json.loads(REGISTER_PATH.read_text())
OVERLAY = json.loads(OVERLAY_PATH.read_text())
OUT = ROOT / 'QA/Canton_Continuation/Walled_WallGate_Fit_Plan.json'
ORIGIN = REGISTER['working_origin_m']
assert CONFIG['historically_accepted'] is False
assert PLACEMENTS['passed'] and PLACEMENTS['historically_accepted'] is False
assert REGISTER['status'] == 'PROVISIONAL_VISUAL_PLACEMENT_ONLY'


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def round2(v):
    return [round(v[0], 4), round(v[1], 4)]


features = OVERLAY['layers']['Wall']['features']
polylines = []
for fi, feature in enumerate(features):
    points = [((p[0] - ORIGIN[0]) * 100, (p[1] - ORIGIN[1]) * 100)
              for p in feature['geometry']['coordinates']]
    lengths = [dist(a, b) for a, b in zip(points, points[1:])]
    cum = [0.0]
    for length in lengths:
        cum.append(cum[-1] + length)
    polylines.append({'fi': fi, 'id': feature['properties']['id'],
                      'points': points, 'lengths': lengths, 'cum': cum})


def point_at(polyline, s):
    s = max(0, min(polyline['cum'][-1], s))
    for si, length in enumerate(polyline['lengths']):
        if s <= polyline['cum'][si + 1] or si == len(polyline['lengths']) - 1:
            t = (s - polyline['cum'][si]) / length
            a, b = polyline['points'][si:si + 2]
            return (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))
    raise AssertionError('out of polyline')


def closest(polyline, point):
    best = None
    for si, (a, b) in enumerate(zip(polyline['points'], polyline['points'][1:])):
        vx, vy = b[0] - a[0], b[1] - a[1]
        t = max(0, min(1, ((point[0] - a[0]) * vx + (point[1] - a[1]) * vy) /
                       (vx * vx + vy * vy)))
        q = (a[0] + t * vx, a[1] + t * vy)
        d = dist(point, q)
        if best is None or d < best[0]:
            best = (d, polyline['cum'][si] + t * polyline['lengths'][si], si)
    return best


by_placement = {row['key']: row for row in PLACEMENTS['landmarks']}
by_register = {row['key']: row for row in REGISTER['landmarks']}
cuts = {p['fi']: [] for p in polylines}
gates = []
bridges = []
for setting in CONFIG['joins']:
    key = setting['key']
    placement = by_placement[key]
    registered = by_register[key]
    assert placement['kind'] == registered['kind'] == 'gate'
    poly = next(p for p in polylines if p['id'] == setting['wall_feature_id'])
    center = tuple(placement['location_cm'][:2])
    d, s0, si = closest(poly, center)
    assert d < 2000, (key, d)
    half = setting['join_half_width_cm']
    assert 500 <= half < max(abs(registered['visible_local_xy_bounds_cm'][0][0]),
                             abs(registered['visible_local_xy_bounds_cm'][1][0]))
    start = s0 - half - CONFIG['transition_run_cm']
    end = s0 + half + CONFIG['transition_run_cm']
    assert start > 0 and end < poly['cum'][-1]
    cut_points = [point_at(poly, start), point_at(poly, end)]
    yaw = math.radians(placement['yaw_deg'])
    ax, ay = math.cos(yaw), math.sin(yaw)
    local_y = setting['join_local_y_cm']
    normal = (-ay, ax)
    join_points = [(center[0] + sign * half * ax + local_y * normal[0],
                    center[1] + sign * half * ay + local_y * normal[1])
                   for sign in (-1, 1)]
    if sum(dist(a, b) for a, b in zip(cut_points, join_points)) > sum(
            dist(a, b) for a, b in zip(cut_points, join_points[::-1])):
        join_points.reverse()
    connector_lengths = []
    for side, (cut_point, join_point) in enumerate(zip(cut_points, join_points)):
        cut_local_x = (cut_point[0] - center[0]) * ax + (cut_point[1] - center[1]) * ay
        join_local_x = (join_point[0] - center[0]) * ax + (join_point[1] - center[1]) * ay
        assert abs(cut_local_x) > half + 200, (key, side, cut_local_x)
        assert abs(abs(join_local_x) - half) < .01
        length = dist(cut_point, join_point)
        assert 1000 < length < 5000, (key, length)
        count = math.ceil(length / CONFIG['connector_piece_max_cm'])
        for k in range(count):
            a = tuple(cut_point[j] + (join_point[j] - cut_point[j]) * k / count
                      for j in (0, 1))
            b = tuple(cut_point[j] + (join_point[j] - cut_point[j]) * (k + 1) / count
                      for j in (0, 1))
            bridges.append({'label': 'PROVISIONAL_WallFit_Bridge_' + key + '_' +
                            str(side) + '_' + str(k), 'kind': 'bridge', 'key': key,
                            'side': side, 'part': k, 'part_count': count,
                            'point_a_cm': round2(a), 'point_b_cm': round2(b)})
        connector_lengths.append(round(length, 3))
    cuts[poly['fi']].append((start, end, key))
    gates.append({'key': key, 'wall_feature_id': poly['id'],
                  'nearest_trace_distance_cm': round(d, 3),
                  'nearest_trace_segment': si, 'trace_s_cm': round(s0, 3),
                  'opening_s_cm': [round(start, 3), round(end, 3)],
                  'join_half_width_cm': half,
                  'cut_points_cm': [round2(p) for p in cut_points],
                  'join_points_cm': [round2(p) for p in join_points],
                  'connector_lengths_cm': connector_lengths})

assert len(gates) == len(CONFIG['joins']) == 8
for fi, intervals in cuts.items():
    intervals.sort()
    for a, b in zip(intervals, intervals[1:]):
        assert a[1] < b[0], ('overlapping gate openings', fi, a, b)

retire = []
trace_replacements = []
source_count = 0
for poly in polylines:
    for si, length in enumerate(poly['lengths']):
        count = max(1, math.ceil(length / 3000))  # Original 30 m marker cadence.
        for k in range(count):
            source_count += 1
            label = 'PROVISIONAL_Wall_%d_%d_%d' % (poly['fi'], si, k)
            a = poly['cum'][si] + length * k / count
            b = poly['cum'][si] + length * (k + 1) / count
            fragments = [(a, b)]
            for start, end, _ in cuts[poly['fi']]:
                next_fragments = []
                for lo, hi in fragments:
                    if end <= lo or start >= hi:
                        next_fragments.append((lo, hi))
                    else:
                        if lo < start:
                            next_fragments.append((lo, start))
                        if end < hi:
                            next_fragments.append((end, hi))
                fragments = next_fragments
            if len(fragments) == 1 and abs(fragments[0][0] - a) < .001 and abs(
                    fragments[0][1] - b) < .001:
                continue
            retire.append(label)
            for j, (lo, hi) in enumerate(fragments):
                if hi - lo < .01:
                    continue
                trace_replacements.append({
                    'label': 'PROVISIONAL_WallFit_Trace_%d_%d_%d_%d' % (
                        poly['fi'], si, k, j), 'kind': 'trace',
                    'source_label': label, 'point_a_cm': round2(point_at(poly, lo)),
                    'point_b_cm': round2(point_at(poly, hi))})

assert source_count == 410, source_count
assert len(retire) == len(set(retire))
assert all(dist(row['point_a_cm'], row['point_b_cm']) > .01
           for row in trace_replacements + bridges)
source_hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in (CONFIG_PATH, PLACEMENTS_PATH, REGISTER_PATH, OVERLAY_PATH)}
plan = {'status': CONFIG['status'], 'historically_accepted': False,
        'map': PLACEMENTS['map'], 'source_sha256': source_hashes,
        'source_wall_marker_count': source_count,
        'retired_wall_markers': retire,
        'retired_wall_marker_count': len(retire),
        'retained_wall_marker_count': source_count - len(retire),
        'trace_replacements': trace_replacements,
        'bridge_segments': bridges, 'gates': gates,
        'wall_width_cm': CONFIG['wall_width_cm'],
        'wall_height_above_ground_cm': CONFIG['wall_height_above_ground_cm'],
        'wall_embed_below_ground_cm': CONFIG['wall_embed_below_ground_cm'],
        'passed': True}
OUT.write_text(json.dumps(plan, indent=2) + '\n')
print('planned %d gate joins: retire %d/%d source markers, %d trace fragments, %d bridge pieces' %
      (len(gates), len(retire), source_count, len(trace_replacements), len(bridges)))
