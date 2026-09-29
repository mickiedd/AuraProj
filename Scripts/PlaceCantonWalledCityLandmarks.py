"""Place the nine current showcase landmarks on the provisional walled-city trace.

The source affine fails historical XY acceptance. These tagged actors are a visual
layout only; replace the JSON coordinates after surveyed gate and ground controls.
Run in a fresh full UnrealEditor with -ExecutePythonScript.
"""
import hashlib
import json
import math
import struct
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
PLAN = ROOT / 'Data/Canton_WalledCity_Provisional_Landmark_Placements.json'
OUT = ROOT / 'QA/Canton_Continuation/Walled_Landmark_Placement.json'
CONTRACT = json.loads((ROOT / 'Data/Canton_Prototype_Contract.json').read_text())
DATA = json.loads(PLAN.read_text())
TRANSFORM = json.loads((ROOT / DATA['map_transform']).read_text())
TRACE = json.loads((ROOT / DATA['map_trace']).read_text())
RAW = ROOT / DATA['raw_height']
HEIGHTS = RAW.read_bytes()
MAP = DATA['target_map']
ACTOR_TAG = 'Canton.ProvisionalLandmarkPlacement'
LABEL_TAG = 'Canton.ProvisionalLandmarkLabel'

assert DATA['status'] == 'PROVISIONAL_VISUAL_PLACEMENT_ONLY'
assert DATA['historically_accepted'] is False
assert MAP == CONTRACT['map']
assert not TRANSFORM['acceptance']['passed']
assert DATA['horizontal_uncertainty_floor_m'] >= TRANSFORM['independent_check_max_m']
assert hashlib.sha256(HEIGHTS).hexdigest() == CONTRACT['raw_sha256']
assert TRACE['source_image_size_px'] == [7017, 4384]
assert TRACE['reference_preview_size_px'] == [1800, 1125]
assert len(DATA['landmarks']) == 9
assert len({row['key'] for row in DATA['landmarks']}) == 9


def xy_from_preview(pixel):
    px = pixel[0] * TRACE['source_image_size_px'][0] / TRACE['reference_preview_size_px'][0]
    py = pixel[1] * TRACE['source_image_size_px'][1] / TRACE['reference_preview_size_px'][1]
    affine = TRANSFORM['pixel_to_projected']
    east = affine['easting_m']['pixel_x'] * px + affine['easting_m']['pixel_y'] * py + affine['easting_m']['offset']
    north = affine['northing_m']['pixel_x'] * px + affine['northing_m']['pixel_y'] * py + affine['northing_m']['offset']
    x = (east - DATA['working_origin_m'][0]) * 100
    y = (north - DATA['working_origin_m'][1]) * 100
    assert 10000 < x < 393200 and 10000 < y < 393200, (pixel, x, y)
    return x, y


def ground_z(x, y):
    col = max(0, min(2016, round(x / 200)))
    row = max(0, min(2016, round(y / 200)))
    code = struct.unpack_from('<H', HEIGHTS, 2 * (row * 2017 + col))[0]
    return (code - 32768) * 50 / 128


def footprint_heights(row, x, y):
    lower, upper = row['visible_local_xy_bounds_cm']
    theta = math.radians(row['yaw_deg'])
    heights = []
    for local_x in (lower[0], (lower[0] + upper[0]) / 2, upper[0]):
        for local_y in (lower[1], (lower[1] + upper[1]) / 2, upper[1]):
            xx = x + local_x * math.cos(theta) - local_y * math.sin(theta)
            yy = y + local_x * math.sin(theta) + local_y * math.cos(theta)
            heights.append(ground_z(xx, yy))
    return min(heights), max(heights)


def find_tagged(actors, tag):
    return {a.get_actor_label(): a for a in actors.get_all_level_actors()
            if tag in [str(t) for t in a.tags]}


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and world.get_name() == 'L_Canton_WalledCity_PROVISIONAL'
    assert unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed = find_tagged(actors, ACTOR_TAG)
    labels = find_tagged(actors, LABEL_TAG)
    expected_actor_labels = {'PROVISIONAL_Landmark_' + row['key'] for row in DATA['landmarks']}
    expected_text_labels = {'PROVISIONAL_Label_' + row['key'] for row in DATA['landmarks']}
    assert set(placed) <= expected_actor_labels, 'Unknown tagged landmark exists'
    assert set(labels) <= expected_text_labels, 'Unknown tagged label exists'
    report = {'status': DATA['status'], 'historically_accepted': False,
              'map': MAP, 'horizontal_uncertainty_floor_m': DATA['horizontal_uncertainty_floor_m'],
              'source_transform_acceptance_passed': False, 'landmarks': [],
              'retired_diagnostic_gate_markers': []}
    positions = []
    for row in DATA['landmarks']:
        asset = unreal.EditorAssetLibrary.load_asset(row['blueprint'])
        assert isinstance(asset, unreal.Blueprint), row['blueprint']
        x, y = xy_from_preview(row['preview_pixel'])
        z_surface = ground_z(x, y)
        z = z_surface - row['visible_local_z_min_cm']
        yaw = row['yaw_deg']
        rotation = unreal.Rotator(pitch=0, yaw=yaw, roll=0)
        location = unreal.Vector(x, y, z)
        label = 'PROVISIONAL_Landmark_' + row['key']
        actor = placed.get(label)
        if actor:
            assert actor.get_class() == asset.generated_class(), label + ' has wrong Blueprint'
            actor.modify(True)
            actor.set_actor_location(location, False, False)
            actor.set_actor_rotation(rotation, False)
        else:
            actor = actors.spawn_actor_from_class(asset.generated_class(), location,
                                                  rotation, transient=False)
            assert actor, label
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
        actor.set_editor_property('is_spatially_loaded', True)
        actor.tags = [ACTOR_TAG, 'Canton.HistoricalXY.Unverified',
                      'Canton.ShowcaseLandmark.' + row['key']]
        actor.set_folder_path('Canton/PROVISIONAL_Landmarks')
        actual = actor.get_actor_location()
        assert math.dist((actual.x, actual.y, actual.z), (x, y, z)) < .01
        applied = actor.get_actor_rotation()
        assert abs(((applied.yaw - yaw + 180) % 360) - 180) < .01
        assert abs(applied.pitch) < .01 and abs(applied.roll) < .01

        text_label = 'PROVISIONAL_Label_' + row['key']
        text_location = unreal.Vector(x, y, z_surface + row['visible_height_cm'] + 450)
        text_rotation = unreal.Rotator(pitch=0, yaw=row['label_yaw_deg'], roll=0)
        text_actor = labels.get(text_label)
        if text_actor:
            text_actor.modify(True)
            text_actor.set_actor_location(text_location, False, False)
            text_actor.set_actor_rotation(text_rotation, False)
        else:
            text_actor = actors.spawn_actor_from_class(unreal.TextRenderActor,
                                                       text_location, text_rotation,
                                                       transient=False)
            assert text_actor, text_label
        text_actor.set_actor_label(text_label)
        text_actor.set_editor_property('is_spatially_loaded', True)
        text_actor.tags = [LABEL_TAG, 'Canton.HistoricalXY.Unverified',
                           'Canton.ShowcaseLandmark.' + row['key']]
        text_actor.set_folder_path('Canton/PROVISIONAL_Landmarks/Labels')
        text_component = text_actor.get_component_by_class(unreal.TextRenderComponent)
        assert text_component
        text_component.set_text(unreal.Text('PROVISIONAL / ' + row['display_name']))
        text_component.set_editor_property('world_size', 240.0)
        text_component.set_text_render_color(unreal.Color(255, 186, 56, 255))

        low, high = footprint_heights(row, x, y)
        assert high - low < 1000, row['key'] + ' needs a different temporary site'
        positions.append((row['key'], x, y))
        report['landmarks'].append({
            'key': row['key'], 'kind': row['kind'], 'asset': row['blueprint'],
            'actor_label': label, 'text_label': text_label,
            'preview_pixel': row['preview_pixel'], 'location_cm': [round(x, 3), round(y, 3), round(z, 3)],
            'yaw_deg': yaw, 'terrain_anchor_z_cm': round(z_surface, 3),
            'label_yaw_deg': row['label_yaw_deg'], 'display_name': row['display_name'],
            'footprint_terrain_range_cm': [round(low, 3), round(high, 3)],
            'footprint_terrain_span_cm': round(high - low, 3),
            'scale': [1, 1, 1], 'is_spatially_loaded': True,
            'historically_accepted': False, 'sector': row['sector'],
        })
    for i, (name, x, y) in enumerate(positions):
        for other, xx, yy in positions[i + 1:]:
            assert math.hypot(x - xx, y - yy) > 10000, (name, other)

    # The four blue cubes were point markers for the rejected transform. The
    # landmark actors and this register replace them, so they must not overlap.
    for marker in list(actors.get_all_level_actors()):
        if marker.get_actor_label().startswith('PROVISIONAL_Gate_'):
            report['retired_diagnostic_gate_markers'].append(marker.get_actor_label())
            assert actors.destroy_actor(marker)
    assert len(report['retired_diagnostic_gate_markers']) in (0, 4)

    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    packages = [p for p in set(packages) if '/Canton/Provisional/' in p.get_name()]
    if packages:
        assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, False)
    report['actor_count'] = len(report['landmarks'])
    report['label_count'] = len(report['landmarks'])
    report['passed'] = report['actor_count'] == report['label_count'] == 9
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('CANTON_WALLED_LANDMARK_PLACEMENT ' + json.dumps({
        'passed': report['passed'], 'count': report['actor_count'],
        'retired_markers': report['retired_diagnostic_gate_markers']}))


try:
    run()
except Exception:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()},
                              indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
