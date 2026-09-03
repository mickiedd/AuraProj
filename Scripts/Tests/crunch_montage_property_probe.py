import json
import unreal

path = '/Game/Characters/Crunch/GameplayAbility/Combo/Animations/AM_Combo_Crunch.AM_Combo_Crunch'
asset = unreal.EditorAssetLibrary.load_asset(path)
names = ['CompositeSections', 'composite_sections', 'Notifies', 'notifies', 'SlotAnimTracks', 'slot_anim_tracks', 'SequenceLength', 'sequence_length', 'BranchingPointMarkers', 'branching_point_markers', 'BlendIn', 'blend_in']
out = []
for name in names:
    try:
        value = asset.get_editor_property(name)
        out.append({'name': name, 'ok': True, 'value': str(value)[:10000]})
    except Exception as exc:
        out.append({'name': name, 'ok': False, 'error': str(exc)})
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/montage-property-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=2)
unreal.log('CrunchMontagePropertyProbeOK')
