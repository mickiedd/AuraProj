import json
import unreal
names = [name for name in dir(unreal) if 'AnimGraph' in name or 'AnimationGraph' in name or 'AnimBlueprint' in name or 'AnimationBlueprint' in name or 'BlueprintFactory' in name]
out = {}
for name in names:
    try:
        cls = getattr(unreal, name)
        out[name] = [n for n in dir(cls) if not n.startswith('_')]
    except Exception as exc:
        out[name] = str(exc)
for name in ('EdGraph', 'EdGraphNode', 'EdGraphPin', 'AnimGraphNode_Root', 'AnimGraphNode_Slot'):
    if hasattr(unreal, name):
        try:
            out[name] = [n for n in dir(getattr(unreal, name)) if not n.startswith('_')]
        except Exception as exc:
            out[name] = str(exc)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/anim-blueprint-factory-probe.json', 'w', encoding='utf-8') as stream:
    json.dump(out, stream, indent=2)
unreal.log('CrunchAnimBlueprintFactoryProbeOK')
