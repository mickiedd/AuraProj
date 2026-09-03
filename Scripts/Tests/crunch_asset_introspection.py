import json
import unreal

TARGETS = [
    '/Game/Characters/Crunch/Crunch_BP.Crunch_BP',
    '/Game/Characters/Crunch/GameplayAbility/Combo/GA_Combo_Crunch_BP.GA_Combo_Crunch_BP',
    '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/GA_UpperCut_BP.GA_UpperCut_BP',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/Animation/AM_UpperCut.AM_UpperCut',
    '/Game/Characters/Crunch/GameplayAbility/Dash/GA_Dash_BP.GA_Dash_BP',
    '/Game/Characters/Crunch/GameplayAbility/Dash/AM_Dash.AM_Dash',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/GA_GroundBlast_BP.GA_GroundBlast_BP',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/Animation/AM_GroundBlast_Casting.AM_GroundBlast_Casting',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/Animation/AM_GroundBlast_Targetting.AM_GroundBlast_Targetting',
    '/Game/Characters/Crunch/GameplayAbility/Turnado/GA_Tornado_BP.GA_Tornado_BP',
    '/Game/Characters/Crunch/GameplayAbility/Turnado/Animation/AM_Tornado.AM_Tornado',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch.Crunch',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch_Skeleton.Crunch_Skeleton',
]

def safe(value):
    try:
        return str(value)
    except Exception as exc:
        return '<str-error:%s>' % exc

def props(obj):
    names = []
    try:
        names = [n for n in dir(obj) if not n.startswith('_')]
    except Exception:
        return []
    out = []
    for name in names:
        if name in ('get_editor_property', 'set_editor_property') or name.startswith('call_'):
            continue
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue
        text = safe(value)
        if len(text) > 240:
            text = text[:237] + '...'
        out.append({'name': name, 'value': text})
    return out[:200]

rows = []
for path in TARGETS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    row = {'object_path': path, 'loaded': bool(asset), 'type': safe(type(asset))}
    if asset:
        row['properties'] = props(asset)
        try:
            row['class'] = safe(asset.get_class())
        except Exception:
            pass
        try:
            generated = asset.generated_class()
            row['generated_class'] = safe(generated)
            cdo = unreal.get_default_object(generated)
            row['cdo_properties'] = props(cdo)
        except Exception as exc:
            row['generated_class_error'] = safe(exc)
    rows.append(row)

out = 'C:/Git/AuraProj/Saved/Reports/CrunchMigration/source-asset-introspection.json'
with open(out, 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchAssetIntrospectionCount=%d' % len(rows))
