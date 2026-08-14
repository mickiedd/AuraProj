"""Create a temporary editor preview of the BungeeMan body and rifle."""

import unreal


BODY = "/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan"
WEAPON = "/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B.Assault_Rifle_B"
ANIM = "/Game/BungeeMan/Blueprints/ABP_Bungee.ABP_Bungee_C"
PREVIEW_OFFSET = unreal.Vector(0.0, 0.0, 0.0)
PREVIEW_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.is_in_play_in_editor():
        unreal.EditorLevelLibrary.editor_end_play()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("world={}".format(world))
    body_mesh = unreal.EditorAssetLibrary.load_asset(BODY)
    weapon_mesh = unreal.EditorAssetLibrary.load_asset(WEAPON)
    anim_class = unreal.load_class(None, ANIM)
    print("assets body={} weapon={} anim={}".format(body_mesh, weapon_mesh, anim_class))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for existing in actors.get_all_level_actors():
        if existing.get_actor_label().startswith("BungeeWeaponPreview_"):
            actors.destroy_actor(existing)
    body_actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    weapon_actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    print("body_actor={} weapon_actor={}".format(body_actor, weapon_actor))
    try:
        body = body_actor.get_component_by_class(unreal.SkeletalMeshComponent)
        weapon = weapon_actor.get_component_by_class(unreal.SkeletalMeshComponent)
        body.set_skeletal_mesh_asset(body_mesh)
        if anim_class:
            body.set_anim_instance_class(anim_class)
        body.set_component_tick_enabled(True)
        weapon.set_skeletal_mesh_asset(weapon_mesh)
        weapon.attach_to_component(body, "WeaponHandSocket", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, False)
        weapon.set_relative_transform(unreal.Transform(PREVIEW_OFFSET, PREVIEW_ROTATION, unreal.Vector(1, 1, 1)), False, False)
        print("body={} weapon={} weapon_transform={}".format(body, weapon, weapon.get_world_transform()))
        body_actor.set_actor_label("BungeeWeaponPreview_Body")
        weapon_actor.set_actor_label("BungeeWeaponPreview_Weapon_Default")
        camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(450, -500, 180))
        target = unreal.Vector(0, 0, 95)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), target), False)
        camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(50.0)
        camera.set_actor_label("BungeeWeaponPreview_Camera")
        light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(300, -300, 250))
        light_component = light.get_component_by_class(unreal.PointLightComponent)
        light_component.set_intensity(100000.0)
        light_component.set_attenuation_radius(1000.0)
        light.set_actor_label("BungeeWeaponPreview_Light")
        task = unreal.AutomationLibrary.take_high_res_screenshot(
            800, 600, "C:/Git/AuraProj/Saved/WeaponPreview_Default.png", camera,
            False, False, unreal.ComparisonTolerance.LOW, "weapon preview", 0.0, False)
        print("screenshot task={}".format(task))
    except Exception as exc:
        print("preview ERROR: {}".format(exc))
        actors.destroy_actor(body_actor)
        actors.destroy_actor(weapon_actor)


main()
