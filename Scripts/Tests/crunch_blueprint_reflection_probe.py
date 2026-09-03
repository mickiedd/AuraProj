import json
import unreal

TARGETS = [
    '/Game/Characters/Crunch/GameplayAbility/Combo/GA_Combo_Crunch_BP.GA_Combo_Crunch_BP',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/GA_UpperCut_BP.GA_UpperCut_BP',
    '/Game/Characters/Crunch/GameplayAbility/Dash/GA_Dash_BP.GA_Dash_BP',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/GA_GroundBlast_BP.GA_GroundBlast_BP',
    '/Game/Characters/Crunch/GameplayAbility/Turnado/GA_Tornado_BP.GA_Tornado_BP',
]

def s(v):
    try:
        return str(v)
    except Exception as e:
        return '<error:%s>' % e

rows = []
for path in TARGETS:
    bp = unreal.EditorAssetLibrary.load_asset(path)
    row = {'path': path, 'loaded': bool(bp)}
    if not bp:
        rows.append(row)
        continue
    try:
        gc = bp.generated_class()
        cdo = unreal.get_default_object(gc)
        row['generated_class_dir'] = [x for x in dir(gc) if not x.startswith('_')]
        row['cdo_dir'] = [x for x in dir(cdo) if not x.startswith('_')]
        for obj, key in ((gc, 'class'), (cdo, 'cdo')):
            vals = []
            for n in dir(obj):
                if n.startswith('_') or n in ('get_editor_property', 'set_editor_property'):
                    continue
                try:
                    vals.append({'name': n, 'value': s(obj.get_editor_property(n))})
                except Exception:
                    pass
            row[key + '_readable'] = vals
    except Exception as exc:
        row['error'] = s(exc)
    rows.append(row)

with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/blueprint-reflection-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchBlueprintReflectionProbeCount=%d' % len(rows))
