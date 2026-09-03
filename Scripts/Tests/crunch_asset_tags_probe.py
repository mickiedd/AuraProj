import json
import unreal

TARGETS = [
    '/Game/Characters/Crunch/GameplayAbility/Combo/GA_Combo_Crunch_BP.GA_Combo_Crunch_BP',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/GA_UpperCut_BP.GA_UpperCut_BP',
    '/Game/Characters/Crunch/GameplayAbility/Dash/GA_Dash_BP.GA_Dash_BP',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/GA_GroundBlast_BP.GA_GroundBlast_BP',
    '/Game/Characters/Crunch/GameplayAbility/Turnado/GA_Tornado_BP.GA_Tornado_BP',
]
reg = unreal.AssetRegistryHelpers.get_asset_registry()
rows = []
for path in TARGETS:
    package, name = path.rsplit('.', 1)
    data = reg.get_asset_by_object_path(path)
    row = {'path': path, 'found': bool(data)}
    if data:
        row['asset_class'] = str(data.asset_class_path)
        try:
            row['dir'] = [n for n in dir(data) if not n.startswith('_')]
            names = []
            for method in ('get_tag_names', 'get_tags'):
                try:
                    names = list(getattr(data, method)())
                    break
                except Exception:
                    pass
            row['tag_names'] = [str(n) for n in names]
            row['tags'] = []
            for n in names:
                try:
                    row['tags'].append({'name': str(n), 'value': str(data.get_tag_value(n))})
                except Exception as exc:
                    row['tags'].append({'name': str(n), 'error': str(exc)})
        except Exception as exc:
            row['tags_error'] = str(exc)
    rows.append(row)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/asset-tags-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchAssetTagsProbeCount=%d' % len(rows))
