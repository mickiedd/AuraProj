"""Splice the coarse provisional wall markers into the eight saved gate buildings.

This edits only the visual proxy wall, retaining gate transforms. All generated
actors are labeled and replaceable when surveyed wall/gate controls exist.
"""
import hashlib
import json
import math
import struct
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
PLAN = json.loads((ROOT / 'QA/Canton_Continuation/Walled_WallGate_Fit_Plan.json').read_text())
CONFIG = json.loads((ROOT / 'Data/Canton_WalledCity_Provisional_Wall_Gate_Fit_Config.json').read_text())
REGISTER = json.loads((ROOT / 'Data/Canton_WalledCity_Provisional_Landmark_Placements.json').read_text())
PLACEMENTS = json.loads((ROOT / 'QA/Canton_Continuation/Walled_Landmark_Placement.json').read_text())
OUT = ROOT / 'QA/Canton_Continuation/Walled_WallGate_Fit_Application.json'
HEIGHTS = (ROOT / REGISTER['raw_height']).read_bytes()
assert PLAN['passed'] and PLAN['historically_accepted'] is False
for relative, digest in PLAN['source_sha256'].items():
    assert hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() == digest, relative
assert len(PLAN['gates']) == 8


def ground_z(x, y):
    col = max(0, min(2016, round(x / 200)))
    row = max(0, min(2016, round(y / 200)))
    code = struct.unpack_from('<H', HEIGHTS, 2 * (row * 2017 + col))[0]
    return (code - 32768) * 50 / 128


def segment_geometry(row):
    a, b = row['point_a_cm'], row['point_b_cm']
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy)
    assert length > .01
    x, y = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    elevations = [ground_z(a[0], a[1]), ground_z(x, y), ground_z(b[0], b[1])]
    if row['kind'] == 'bridge':
        # At the gate the proxy wall overlaps the same modern R16 base used to
        # seat the building. Embed beneath all three samples to prevent a
        # daylight seam on the sloped context terrain.
        gate_base = next(p['terrain_anchor_z_cm'] for p in PLACEMENTS['landmarks']
                         if p['key'] == row['key'])
        gate = next(g for g in PLAN['gates'] if g['key'] == row['key'])
        cut_ground = ground_z(*gate['cut_points_cm'][row['side']])
        spine = [cut_ground + (gate_base - cut_ground) * part / row['part_count']
                 for part in (row['part'], row['part'] + 1)]
        lo = min(elevations + spine) - CONFIG['wall_embed_below_ground_cm']
        hi = max(elevations + spine) + CONFIG['wall_height_above_ground_cm']
    else:
        lo = min(elevations) - CONFIG['wall_embed_below_ground_cm']
        hi = max(elevations) + CONFIG['wall_height_above_ground_cm']
    return (unreal.Vector(x, y, (lo + hi) / 2),
            unreal.Rotator(pitch=0, yaw=math.degrees(math.atan2(dy, dx)), roll=0),
            unreal.Vector(length / 100, CONFIG['wall_width_cm'] / 100,
                          (hi - lo) / 100), lo, hi)


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(PLAN['map'])
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    retired = set(PLAN['retired_wall_markers'])
    generated = PLAN['trace_replacements'] + PLAN['bridge_segments']
    expected = {row['label'] for row in generated}
    assert len(expected) == len(generated)
    existing_original = {label for label in by_label if label.startswith('PROVISIONAL_Wall_')}
    retained = existing_original - retired
    assert len(retained) == PLAN['retained_wall_marker_count'], (
        len(retained), PLAN['retained_wall_marker_count'])
    preexisting_fit = {label for label in by_label if label.startswith('PROVISIONAL_WallFit_')}
    assert preexisting_fit <= expected
    cube = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
    material = unreal.EditorAssetLibrary.load_asset(
        '/Game/Canton/Provisional/Terrain/M_WallMarkers_PROVISIONAL')
    assert cube and material
    removed_now = []
    for label in sorted(retired & existing_original):
        assert actors.destroy_actor(by_label[label]), label
        removed_now.append(label)
    created = 0
    updated = 0
    spans = []
    for row in generated:
        location, rotation, scale, bottom, top = segment_geometry(row)
        actor = by_label.get(row['label'])
        if actor:
            assert isinstance(actor, unreal.StaticMeshActor)
            actor.modify(True)
            updated += 1
        else:
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                                  location, rotation,
                                                  transient=False)
            assert actor
            created += 1
        actor.set_actor_label(row['label'])
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        actor.set_actor_scale3d(scale)
        actor.static_mesh_component.set_static_mesh(cube)
        actor.static_mesh_component.set_material(0, material)
        actor.set_actor_enable_collision(False)
        actor.set_editor_property('is_spatially_loaded', True)
        actor.tags = ['Canton.ProvisionalWallGateFit',
                      'Canton.HistoricalXY.Unverified'] + (
                          ['Canton.GateWallJoin.' + row['key']]
                          if row['kind'] == 'bridge' else [])
        actor.set_folder_path('Canton/PROVISIONAL_Wall_Gate_Fit')
        spans.append({'label': row['label'], 'kind': row['kind'],
                      'bottom_z_cm': round(bottom, 3), 'top_z_cm': round(top, 3)})
    assert len(retained) + len(generated) == 474
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, PLAN['map'])
    packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    packages = [p for p in set(packages) if '/Canton/Provisional/' in p.get_name()]
    if packages:
        assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, False)
    result = {'map': PLAN['map'], 'historically_accepted': False,
              'provisional_wall_fit': True, 'gate_count': 8,
              'original_wall_markers': PLAN['source_wall_marker_count'],
              'retired_original_markers': PLAN['retired_wall_marker_count'],
              'removed_this_run': removed_now,
              'retained_original_markers': len(retained),
              'trace_replacement_count': len(PLAN['trace_replacements']),
              'bridge_segment_count': len(PLAN['bridge_segments']),
              'generated_created': created, 'generated_updated': updated,
              'total_wall_actors': len(retained) + len(generated),
              'vertical_spans': spans, 'passed': True}
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    unreal.log('CANTON_WALL_GATE_FIT ' + json.dumps({
        'passed': True, 'gates': 8, 'retired': len(retired),
        'bridge_pieces': len(PLAN['bridge_segments'])}))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()},
                              indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
