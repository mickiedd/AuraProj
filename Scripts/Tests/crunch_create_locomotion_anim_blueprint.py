import json
import unreal


ASSET_NAME = 'ABP_Crunch_AuraV5'
ASSET_FOLDER = '/Game/Blueprints/Character/Crunch'
ASSET_PATH = f'{ASSET_FOLDER}/{ASSET_NAME}'
REPORT_PATH = 'C:/Git/AuraProj/Saved/Reports/CrunchMigration/locomotion-anim-blueprint-create.json'

result = {'created': False, 'asset': ASSET_PATH, 'errors': []}
asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if not asset:
    skeleton = unreal.EditorAssetLibrary.load_asset(
        '/Game/Assets/Characters/Crunch/Meshes/Crunch_SkeletonV4.Crunch_SkeletonV4')
    parent_class = unreal.load_class(None, '/Script/Aura.AuraCharacterAnimInstance')
    if not skeleton:
        result['errors'].append('target Crunch skeleton failed to load')
    if not parent_class:
        result['errors'].append('AuraCharacterAnimInstance class failed to load')
    if skeleton and parent_class:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', parent_class)
        factory.set_editor_property('target_skeleton', skeleton)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, ASSET_FOLDER, unreal.AnimBlueprint, factory)
        result['created'] = bool(asset)

if asset:
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    result['saved'] = unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False)
    result['generated_class'] = str(unreal.BlueprintEditorLibrary.generated_class(asset))
else:
    result['errors'].append('locomotion AnimBlueprint is unavailable')

with open(REPORT_PATH, 'w', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)

if result['errors']:
    unreal.log_error('CrunchLocomotionAnimBlueprintCreateErrors=' + ';'.join(result['errors']))
else:
    unreal.log('CrunchLocomotionAnimBlueprintReady=' + ASSET_PATH)
