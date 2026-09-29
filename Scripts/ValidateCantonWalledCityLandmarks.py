"""Fresh-editor audit of all nine saved provisional landmark actors and labels."""
import json
import math
import struct
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DATA = json.loads((ROOT / 'Data/Canton_WalledCity_Provisional_Landmark_Placements.json').read_text())
PLACED = json.loads((ROOT / 'QA/Canton_Continuation/Walled_Landmark_Placement.json').read_text())
OUT = ROOT / 'QA/Canton_Continuation/Walled_Landmark_Reload_Validation.json'
RAW = (ROOT / DATA['raw_height']).read_bytes()


def height_at(x, y):
    row = max(0, min(2016, round(y / 200)))
    col = max(0, min(2016, round(x / 200)))
    code = struct.unpack_from('<H', RAW, 2 * (row * 2017 + col))[0]
    return (code - 32768) * 50 / 128


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(DATA['target_map'])
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    landmarks = {a.get_actor_label(): a for a in actors
                 if 'Canton.ProvisionalLandmarkPlacement' in [str(t) for t in a.tags]}
    labels = {a.get_actor_label(): a for a in actors
              if 'Canton.ProvisionalLandmarkLabel' in [str(t) for t in a.tags]}
    assert len(landmarks) == len(labels) == 9
    assert not any(a.get_actor_label().startswith('PROVISIONAL_Gate_') for a in actors)
    assert len([a for a in actors if isinstance(a, unreal.PlayerStart)]) == 1
    assert world.get_world_settings().get_editor_property('default_game_mode').get_name() == 'CantonWalledCityGameMode'
    report = {'map': DATA['target_map'], 'historically_accepted': False,
              'placement_status': DATA['status'], 'landmarks': [], 'labels': len(labels),
              'old_gate_markers': 0}
    expected = {row['key']: row for row in DATA['landmarks']}
    planned = {row['key']: row for row in PLACED['landmarks']}
    assert set(expected) == set(planned)
    for key, row in expected.items():
        label = 'PROVISIONAL_Landmark_' + key
        actor = landmarks[label]
        text = labels['PROVISIONAL_Label_' + key]
        asset = unreal.EditorAssetLibrary.load_asset(row['blueprint'])
        assert asset and actor.get_class() == asset.generated_class()
        assert actor.get_editor_property('is_spatially_loaded')
        assert text.get_editor_property('is_spatially_loaded')
        assert 'Canton.HistoricalXY.Unverified' in [str(t) for t in actor.tags]
        text_component = text.get_component_by_class(unreal.TextRenderComponent)
        assert text_component
        assert str(text_component.get_editor_property('text')) == 'PROVISIONAL / ' + row['display_name']
        assert abs(text_component.get_editor_property('world_size') - 240.0) < .01
        text_yaw = text.get_actor_rotation().yaw
        assert abs(((text_yaw - row['label_yaw_deg'] + 180) % 360) - 180) < .05
        p = actor.get_actor_location()
        target = planned[key]['location_cm']
        error = math.dist((p.x, p.y, p.z), target)
        assert error < .05, (key, error)
        assert abs((p.z + row['visible_local_z_min_cm']) - height_at(p.x, p.y)) < .05
        r = actor.get_actor_rotation()
        assert abs(((r.yaw - row['yaw_deg'] + 180) % 360) - 180) < .05
        assert abs(r.pitch) < .05 and abs(r.roll) < .05
        scale = actor.get_actor_scale3d()
        assert all(abs(v - 1) < .0001 for v in (scale.x, scale.y, scale.z))
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        assert components, key + ' has no mesh components'
        report['landmarks'].append({'key': key, 'blueprint': row['blueprint'],
                                    'location_error_cm': round(error, 5),
                                    'mesh_components': len(components),
                                    'spatially_loaded': True})
    assert sum(row['kind'] == 'gate' for row in DATA['landmarks']) == 8
    assert sum(row['kind'] == 'tower_not_gate' for row in DATA['landmarks']) == 1
    report['passed'] = True
    OUT.write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('CANTON_WALLED_LANDMARK_RELOAD ' + json.dumps({
        'passed': True, 'landmarks': len(report['landmarks']), 'labels': len(labels)}))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({'passed': False, 'error': traceback.format_exc()},
                              indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
