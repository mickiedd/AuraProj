import json
import unreal

path = '/Game/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchComboV4.AM_CrunchComboV4'
montage = unreal.EditorAssetLibrary.load_asset(path)
rows = {'loaded': bool(montage), 'sections': [], 'notifies': [], 'slotNames': []}
if montage:
    try:
        rows['slotNames'] = [str(value) for value in unreal.AnimationLibrary.get_montage_slot_names(montage)]
    except Exception as exc:
        rows['slotNamesError'] = str(exc)
    for index in range(montage.get_num_sections()):
        rows['sections'].append({'index': index, 'name': str(montage.get_section_name(index))})
    try:
        events = unreal.AnimationLibrary.get_animation_notify_events(montage)
        for index, event in enumerate(events):
            row = {'index': index, 'event': str(event), 'time': unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event)}
            try:
                notify = event.get_editor_property('notify')
                row['notify'] = str(notify)
                row['notify_dir'] = [n for n in dir(notify) if not n.startswith('_')]
                for name in ('event_tag', 'EventTag'):
                    try:
                        value = notify.get_editor_property(name)
                        row[name] = str(value)
                        row[name + '_dir'] = [n for n in dir(value) if not n.startswith('_')]
                        for method in ('is_valid', 'to_string', 'get_editor_property'):
                            try:
                                result = getattr(value, method)() if method != 'get_editor_property' else value.get_editor_property('tag_name')
                                row[name + '_' + method] = str(result)
                            except Exception as exc:
                                row[name + '_' + method + '_error'] = str(exc)
                    except Exception as exc:
                        row[name + '_error'] = str(exc)
            except Exception as exc:
                row['notify_error'] = str(exc)
            rows['notifies'].append(row)
    except Exception as exc:
        rows['error'] = str(exc)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/target-montage-probe.json', 'w', encoding='utf-8') as stream:
    json.dump(rows, stream, indent=2)
unreal.log('CrunchTargetMontageProbeOK')
