"""Force-save the provisional walled-city map and its external actor packages."""
import json
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = '/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL'
OUT = ROOT / 'QA/Canton_Continuation/Walled_Disk_Save.json'


def package_name(package):
    return str(package.get_name())


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and world.get_name() == 'L_Canton_WalledCity_PROVISIONAL'
    assert unreal.CantonTerrainLibrary.load_provisional_region(world)
    dirty_before = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    dirty_before = sorted({package_name(package) for package in dirty_before
                           if '/Canton/Provisional/' in package_name(package)})
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    dirty_after_map = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    dirty_after_content = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    packages = {package_name(package): package
                for package in dirty_after_map + dirty_after_content
                if '/Canton/Provisional/' in package_name(package)}
    saved_names = sorted(packages)
    if packages:
        assert unreal.EditorLoadingAndSavingUtils.save_packages(
            list(packages.values()), False)
    remaining = sorted({
        package_name(package)
        for package in (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        if '/Canton/Provisional/' in package_name(package)
    })
    assert not remaining, remaining
    reloaded = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert reloaded and reloaded.get_name() == 'L_Canton_WalledCity_PROVISIONAL'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    landmarks = [a for a in actors
                 if 'Canton.ProvisionalLandmarkPlacement' in [str(t) for t in a.tags]]
    fit_walls = [a for a in actors
                 if 'Canton.ProvisionalWallAdaptive' in [str(t) for t in a.tags]]
    wall_validation = json.loads(unreal.CantonTerrainLibrary.validate_provisional_wall_foundation(reloaded))
    assert wall_validation['passed'], wall_validation
    assert len(landmarks) == 9
    assert len(fit_walls) > 0
    result = {
        'map': MAP,
        'saved_to_disk': True,
        'dirty_provisional_packages_before': dirty_before,
        'dirty_provisional_packages_saved': saved_names,
        'dirty_provisional_packages_remaining': remaining,
        'reloaded_landmarks': len(landmarks),
        'reloaded_adaptive_wall_actors': len(fit_walls),
        'reloaded_wall_validation': wall_validation,
        'passed': True,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    unreal.log('CANTON_WALLED_DISK_SAVE ' + json.dumps({
        'passed': True, 'saved_packages': len(saved_names),
        'landmarks': len(landmarks), 'fit_walls': len(fit_walls)}))


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
