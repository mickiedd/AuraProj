import json
import unreal

TARGETS = [
    '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/Animation/AM_UpperCut.AM_UpperCut',
]
rows = []
for path in TARGETS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    row = {'path': path, 'loaded': bool(asset)}
    if asset:
        for method in ('export_text', 'get_full_name', 'get_path_name'):
            try:
                value = getattr(asset, method)()
                row[method] = str(value)[:500000]
            except Exception as exc:
                row[method + '_error'] = str(exc)
    rows.append(row)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/montage-export-text.json', 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchExportTextProbeOK')
