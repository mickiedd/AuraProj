import json
import unreal

path = '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch'
asset = unreal.EditorAssetLibrary.load_asset(path)
out = {'sections': [], 'notifies': [], 'calls': {}}
for i in range(asset.get_num_sections()):
    row = {'index': i, 'name': str(asset.get_section_name(i))}
    for method in ('get_section_start_time', 'get_section_end_time', 'get_section_length'):
        try:
            row[method] = str(getattr(asset, method)(i))
        except Exception as exc:
            row[method + '_error'] = str(exc)
    for fn in ('GetSectionStartTime', 'GetSectionEndTime', 'GetSectionLength'):
        try:
            row[fn] = str(asset.call_method(fn, (i,)))
        except Exception as exc:
            row[fn + '_error'] = str(exc)
    out['sections'].append(row)
lib = unreal.AnimationLibrary
try:
    events = lib.get_animation_notify_events(asset)
    for idx, event in enumerate(events):
        row = {'index': idx, 'event': str(event)}
        for method in ('get_anim_notify_event_trigger_time', 'get_anim_notify_event_duration'):
            try:
                row[method] = str(getattr(lib, method)(event))
            except Exception as exc:
                row[method + '_error'] = str(exc)
        out['notifies'].append(row)
except Exception as exc:
    out['notifies_error'] = str(exc)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/montage-timing-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=2)
unreal.log('CrunchMontageTimingProbeOK')
