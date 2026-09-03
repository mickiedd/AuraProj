import json
import unreal

path = '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch'
asset = unreal.EditorAssetLibrary.load_asset(path)
out = {'path': path}
for method, args in [
    ('get_num_sections', ()), ('get_play_length', ()), ('get_default_blend_in_time', ()), ('get_default_blend_out_time', ()),
    ('get_section_name', (0,)), ('get_section_index', ('Default',)),
]:
    try:
        out[method] = str(getattr(asset, method)(*args))
    except Exception as exc:
        out[method + '_error'] = str(exc)
lib = unreal.AnimationLibrary
for method, args in [
    ('get_montage_slot_names', (asset,)), ('get_animation_notify_events', (asset,)),
    ('get_animation_notify_track_names', (asset,)), ('get_rate_scale', (asset,)),
    ('get_num_frames', (asset,)), ('get_sequence_length', (asset,)),
]:
    try:
        value = getattr(lib, method)(*args)
        out[method] = str(value)[:20000]
    except Exception as exc:
        out[method + '_error'] = str(exc)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/montage-api-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=2)
unreal.log('CrunchMontageApiProbeOK')
