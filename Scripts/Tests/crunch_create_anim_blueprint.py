import json
import unreal

out = {'created': False, 'errors': []}
target_skeleton = unreal.EditorAssetLibrary.load_asset('/Game/Assets/Characters/Crunch/Meshes/Crunch_SkeletonV4.Crunch_SkeletonV4')
if not target_skeleton:
    out['errors'].append('target skeleton failed to load')
else:
    try:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.AnimInstance.static_class())
        factory.set_editor_property('target_skeleton', target_skeleton)
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset = tools.create_asset('ABP_Crunch_AuraV4', '/Game/Blueprints/Character/Crunch', unreal.AnimBlueprint, factory)
        out['created'] = bool(asset)
        out['asset'] = str(asset)
        if asset:
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV4', only_if_is_dirty=False)
            out['generated_class'] = str(unreal.BlueprintEditorLibrary.generated_class(asset))
    except Exception as exc:
        out['errors'].append(str(exc))
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/anim-blueprint-create.json', 'w', encoding='utf-8') as stream:
    json.dump(out, stream, indent=2)
unreal.log('CrunchAnimBlueprintCreated=%s' % out['created'])
if out['errors']:
    unreal.log_error('CrunchAnimBlueprintCreateErrors=%s' % ';'.join(out['errors']))
