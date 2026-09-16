"""Read-only UE Python pin enum/version discovery."""
import unreal
print('MATERIAL_PROPERTIES',[(k,str(getattr(unreal.MaterialProperty,k))) for k in dir(unreal.MaterialProperty) if k.startswith('MP_')])
print('AUTO_EXPOSURE',[(k,str(getattr(unreal.AutoExposureMethod,k))) for k in dir(unreal.AutoExposureMethod) if k.startswith('AEM')])
for v in (16,17):
    try:print('HIDDEN_PROPERTY',v,unreal.MaterialProperty(v))
    except Exception as exc:print('HIDDEN_PROPERTY_UNAVAILABLE',v,str(exc))
root='/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Manual4K_20260916/Materials/'
for kind in ('Stone','Glaze','RedTimber','DarkTimber','Interior','BronzeGold','Plaque'):
    m=unreal.EditorAssetLibrary.load_asset(root+'M_ZNM_'+kind+'_Manual4K')
    if m:print('EXPRESSION_COUNT',kind,unreal.MaterialEditingLibrary.get_num_material_expressions(m),'TEX',len(unreal.MaterialEditingLibrary.get_used_textures(m)))
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,unreal.DirectionalLight):
        print('EXISTING_DIRECTIONAL_LIGHT',actor.get_actor_label(),actor.get_component_by_class(unreal.DirectionalLightComponent).get_editor_property('intensity'))
