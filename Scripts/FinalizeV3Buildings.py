"""Apply recorded source rotations with named Rotator arguments to V3 assets.

UE's reflected Rotator positional ordering differs from pitch/yaw/roll.
This repair touches only the newly imported V3 Blueprint component templates.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'


def main():
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for cfg in json.loads((ROOT / 'packages.json').read_text()):
        r = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        records = {i['mesh']: i for entry in r['imports'] for i in entry['instances']}
        bp = unreal.load_asset(r['blueprint'])
        fixed = 0
        seen = set()
        for h in ss.k2_gather_subobject_data_for_blueprint(bp):
            component = lib.get_object(ss.k2_find_subobject_data_from_handle(h))
            if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
                continue
            if component.get_path_name() in seen:
                continue
            seen.add(component.get_path_name())
            record = records[component.static_mesh.get_path_name()]
            p, y, roll = record['rotation']
            component.set_editor_property('relative_rotation', unreal.Rotator(pitch=p, yaw=y, roll=roll))
            fixed += 1
        assert fixed == r['visible_components'] + r['collision_components']
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        print('V3_ROTATIONS_VERIFIED', cfg['name'], fixed)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    current_world = editor.get_editor_world()
    level_path = current_world.get_path_name().split('.')[0]
    if level_path.startswith('/Game/Assets/Environment/GuangzhouLandmarks/V3/'):
        assert unreal.EditorLoadingAndSavingUtils.save_map(current_world, level_path)


if __name__ == '__main__':
    main()
