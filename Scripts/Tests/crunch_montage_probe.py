import json
import unreal

TARGETS = [
    '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch',
    '/Game/Characters/Crunch/GameplayAbility/UpperCut/Animation/AM_UpperCut.AM_UpperCut',
    '/Game/Characters/Crunch/GameplayAbility/Dash/AM_Dash.AM_Dash',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/Animation/AM_GroundBlast_Casting.AM_GroundBlast_Casting',
    '/Game/Characters/Crunch/GameplayAbility/GroundBlast/Animation/AM_GroundBlast_Targetting.AM_GroundBlast_Targetting',
    '/Game/Characters/Crunch/GameplayAbility/Turnado/Animation/AM_Tornado.AM_Tornado',
]

def s(v):
    try:
        return str(v)
    except Exception as exc:
        return '<error:%s>' % exc

def unpack(value, depth=0):
    if depth > 3:
        return s(value)
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple)):
        return [unpack(v, depth + 1) for v in value]
    out = {'text': s(value)}
    try:
        for name in dir(value):
            if name.startswith('_') or name in ('get_editor_property', 'set_editor_property'):
                continue
            try:
                child = value.get_editor_property(name)
            except Exception:
                continue
            out[name] = unpack(child, depth + 1)
    except Exception:
        pass
    return out

rows = []
for path in TARGETS:
    montage = unreal.EditorAssetLibrary.load_asset(path)
    row = {'path': path, 'loaded': bool(montage)}
    if montage:
        row['dir'] = [n for n in dir(montage) if not n.startswith('_')]
        for prop in ('sequence_length', 'composite_sections', 'notifies', 'branching_point_markers', 'slot_anim_tracks', 'blend_in', 'blend_out'):
            try:
                row[prop] = unpack(montage.get_editor_property(prop))
            except Exception as exc:
                row[prop + '_error'] = s(exc)
    rows.append(row)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/montage-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchMontageProbeCount=%d' % len(rows))
