import json
import unreal

out = {}
for name in ('AnimationLibrary', 'AnimMontage', 'AnimSequence', 'AnimSequenceLibrary', 'EditorAssetLibrary'):
    try:
        cls = getattr(unreal, name)
        out[name] = [n for n in dir(cls) if not n.startswith('_')]
    except Exception as exc:
        out[name] = '<error:%s>' % exc
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/animation-library-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=2)
unreal.log('CrunchAnimationLibraryProbeOK')
