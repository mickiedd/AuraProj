import json
import unreal

TARGETS = [
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch.Crunch',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch_Skeleton.Crunch_Skeleton',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_01.Ability_Combo_01',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_02.Ability_Combo_02',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_03.Ability_Combo_03',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_04.Ability_Combo_04',
]
rows = []
for path in TARGETS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    row = {'path': path, 'loaded': bool(asset), 'type': str(type(asset)) if asset else None}
    if asset:
        try:
            row['skeleton'] = str(asset.get_skeleton())
        except Exception:
            pass
        for name in ('sequence_length', 'get_play_length', 'get_num_frames'):
            try:
                value = getattr(asset, name)() if callable(getattr(asset, name, None)) else asset.get_editor_property(name)
                row[name] = str(value)
            except Exception as exc:
                row[name + '_error'] = str(exc)
    rows.append(row)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/target-asset-probe.json', 'w', encoding='utf-8') as stream:
    json.dump(rows, stream, indent=2)
unreal.log('CrunchTargetAssetProbeLoaded=%d' % sum(1 for row in rows if row['loaded']))
