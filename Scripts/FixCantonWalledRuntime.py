"""Give the provisional walled-city terrain its own playable runtime entry point.

Run with a fresh UnrealEditor -ExecutePythonScript invocation after building
AuraEditor. This only edits the existing provisional map and its external actors.
"""
import hashlib
import json
import struct
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = '/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL'
OUT = ROOT / 'QA/Canton_Continuation/Walled_Runtime_Map_Setup.json'
CONTRACT = json.loads((ROOT / 'Data/Canton_Prototype_Contract.json').read_text())
RAW = ROOT / CONTRACT['raw_height_path']
assert CONTRACT['map'] == MAP and not CONTRACT['historically_accepted']
assert hashlib.sha256(RAW.read_bytes()).hexdigest() == CONTRACT['raw_sha256']


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and world.get_name() == 'L_Canton_WalledCity_PROVISIONAL'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mode = unreal.load_class(None, '/Script/Aura.CantonWalledCityGameMode')
    assert mode, 'Build AuraEditor before running this script'
    settings = world.get_world_settings()
    settings.set_editor_property('default_game_mode', mode)

    x, y = 173000, 121000
    raw = RAW.read_bytes()
    row, col = round(y / 200), round(x / 200)
    height_code = struct.unpack_from('<H', raw, 2 * (row * 2017 + col))[0]
    ground_z = (height_code - 32768) * 50 / 128
    spawn = unreal.Vector(x, y, ground_z + 130)
    player_starts = [a for a in actors.get_all_level_actors()
                     if isinstance(a, unreal.PlayerStart)]
    if player_starts:
        assert len(player_starts) == 1, 'Unexpected multiple PlayerStarts'
        start = player_starts[0]
        start.modify(True)
        start.set_actor_location(spawn, False, False)
        start.set_actor_rotation(unreal.Rotator(pitch=0, yaw=58, roll=0), False)
    else:
        start = actors.spawn_actor_from_class(
            unreal.PlayerStart, spawn, unreal.Rotator(pitch=0, yaw=58, roll=0))
    assert start
    start.set_actor_label('Canton_WalledCity_Runtime_Start')
    start.set_editor_property('is_spatially_loaded', False)

    # The original suns are already non-spatial. Keep that invariant explicit:
    # a streaming source near the gate must not determine whether daylight exists.
    suns = [a for a in actors.get_all_level_actors()
            if isinstance(a, unreal.DirectionalLight)]
    assert len(suns) >= 1
    for sun in suns:
        sun.set_editor_property('is_spatially_loaded', False)

    atmospheres = [a for a in actors.get_all_level_actors()
                   if isinstance(a, unreal.SkyAtmosphere)]
    assert len(atmospheres) <= 1
    if not atmospheres:
        atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere,
                                                    unreal.Vector(0, 0, 0))
        assert atmosphere
        atmosphere.set_actor_label('Canton_WalledCity_Daylight_Atmosphere')
    else:
        atmosphere = atmospheres[0]
    atmosphere.set_editor_property('is_spatially_loaded', False)

    skylights = [a for a in actors.get_all_level_actors()
                 if isinstance(a, unreal.SkyLight)]
    assert len(skylights) <= 1
    if not skylights:
        skylight = actors.spawn_actor_from_class(unreal.SkyLight,
                                                 unreal.Vector(0, 0, 5000))
        assert skylight
        skylight.set_actor_label('Canton_WalledCity_Daylight_SkyLight')
    else:
        skylight = skylights[0]
    skylight.set_editor_property('is_spatially_loaded', False)
    skylight.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    skylight.light_component.set_editor_property('real_time_capture', True)
    skylight.light_component.set_intensity(1.0)

    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    packages = [p for p in packages if '/Canton/Provisional/' in p.get_name()]
    if packages:
        assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
    report = {
        'map': MAP, 'game_mode': mode.get_name(),
        'player_start': [x, y, spawn.z], 'source_ground_z_cm': ground_z,
        'player_start_non_spatial': not start.get_editor_property('is_spatially_loaded'),
        'directional_lights': len(suns),
        'atmosphere_non_spatial': not atmosphere.get_editor_property('is_spatially_loaded'),
        'skylight_non_spatial': not skylight.get_editor_property('is_spatially_loaded'),
        'historically_accepted': False,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('CANTON_WALLED_RUNTIME_SETUP ' + json.dumps(report))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({'error': traceback.format_exc()}, indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
