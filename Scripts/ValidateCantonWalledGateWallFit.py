"""Fresh-map geometry audit of the eight provisional gate/wall splices."""
import json
import math
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
PLAN = json.loads((ROOT / 'QA/Canton_Continuation/Walled_WallGate_Fit_Plan.json').read_text())
PLACEMENTS = json.loads((ROOT / 'QA/Canton_Continuation/Walled_Landmark_Placement.json').read_text())
OUT = ROOT / 'QA/Canton_Continuation/Walled_WallGate_Fit_Reload_Validation.json'


def xy(p):
    return [float(p[0]), float(p[1])]


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def endpoints(actor):
    p = actor.get_actor_location()
    theta = math.radians(actor.get_actor_rotation().yaw)
    half = actor.get_actor_scale3d().x * 50
    return ([p.x - math.cos(theta) * half, p.y - math.sin(theta) * half],
            [p.x + math.cos(theta) * half, p.y + math.sin(theta) * half])


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(PLAN['map'])
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    by_label = {a.get_actor_label(): a for a in actors}
    original = {label for label in by_label if label.startswith('PROVISIONAL_Wall_')}
    generated = {label for label in by_label if label.startswith('PROVISIONAL_WallFit_')}
    expected = {row['label'] for row in PLAN['trace_replacements'] + PLAN['bridge_segments']}
    assert len(original) == PLAN['retained_wall_marker_count'] == 375
    assert not set(PLAN['retired_wall_markers']) & original
    assert generated == expected and len(generated) == 99
    assert len(original) + len(generated) == 474
    landmark_by_key = {r['key']: r for r in PLACEMENTS['landmarks']}
    for key, row in landmark_by_key.items():
        actor = by_label['PROVISIONAL_Landmark_' + key]
        p = actor.get_actor_location()
        assert math.dist((p.x, p.y, p.z), row['location_cm']) < .05
        assert abs(((actor.get_actor_rotation().yaw - row['yaw_deg'] + 180) % 360) - 180) < .05
    for row in PLAN['trace_replacements'] + PLAN['bridge_segments']:
        actor = by_label[row['label']]
        assert actor.get_editor_property('is_spatially_loaded')
        assert 'Canton.ProvisionalWallGateFit' in [str(t) for t in actor.tags]
        assert not actor.get_actor_enable_collision()
        a, b = endpoints(actor)
        assert dist(a, row['point_a_cm']) < .1, row['label']
        assert dist(b, row['point_b_cm']) < .1, row['label']
        assert abs(actor.get_actor_scale3d().y * 100 - PLAN['wall_width_cm']) < .01
    trace_endpoints = [endpoint for label in original for endpoint in endpoints(by_label[label])]
    trace_endpoints += [endpoint for row in PLAN['trace_replacements']
                        for endpoint in endpoints(by_label[row['label']])]
    gate_checks = []
    for gate in PLAN['gates']:
        key = gate['key']
        side_reports = []
        for side in (0, 1):
            rows = sorted((r for r in PLAN['bridge_segments']
                           if r['key'] == key and r['side'] == side),
                          key=lambda r: r['part'])
            assert len(rows) == rows[0]['part_count']
            bridge_start = endpoints(by_label[rows[0]['label']])[0]
            bridge_end = endpoints(by_label[rows[-1]['label']])[1]
            trace_gap = min(dist(bridge_start, p) for p in trace_endpoints)
            cut_error = dist(bridge_start, gate['cut_points_cm'][side])
            contact_gap = dist(bridge_end, gate['join_points_cm'][side])
            assert trace_gap < .1 and cut_error < .1 and contact_gap < .1
            max_piece_gap = 0
            min_vertical_overlap = float('inf')
            for before, after in zip(rows, rows[1:]):
                a = by_label[before['label']]
                b = by_label[after['label']]
                piece_gap = dist(endpoints(a)[1], endpoints(b)[0])
                max_piece_gap = max(max_piece_gap, piece_gap)
                az, bz = a.get_actor_location().z, b.get_actor_location().z
                ah, bh = a.get_actor_scale3d().z * 50, b.get_actor_scale3d().z * 50
                min_vertical_overlap = min(min_vertical_overlap,
                                           min(az + ah, bz + bh) - max(az - ah, bz - bh))
            assert max_piece_gap < .1 and min_vertical_overlap >= 300, (
                key, side, max_piece_gap, min_vertical_overlap)
            side_reports.append({'side': side, 'trace_join_error_cm': round(trace_gap, 5),
                                 'gate_contact_error_cm': round(contact_gap, 5),
                                 'max_piece_gap_cm': round(max_piece_gap, 5),
                                 'minimum_vertical_overlap_cm': round(min_vertical_overlap, 3)})
        gate_checks.append({'key': key, 'nearest_trace_distance_cm': gate['nearest_trace_distance_cm'],
                            'sides': side_reports})
    result = {'map': PLAN['map'], 'historically_accepted': False,
              'original_markers': len(original), 'generated_segments': len(generated),
              'gate_checks': gate_checks, 'landmarks_unchanged': len(landmark_by_key),
              'passed': True}
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    unreal.log('CANTON_WALL_GATE_RELOAD ' + json.dumps({
        'passed': True, 'gates': len(gate_checks), 'wall_actors': 474}))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()},
                              indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
