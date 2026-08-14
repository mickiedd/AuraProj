"""Inspect BungeeMan weapon/body socket axes in the Unreal editor."""

import unreal


def call(label, fn):
    try:
        value = fn()
        print("{}: {}".format(label, value))
        return value
    except Exception as exc:
        print("{} ERROR: {}".format(label, exc))
        return None


def inspect_asset(path, socket_names):
    asset = call("load {}".format(path), lambda: unreal.EditorAssetLibrary.load_asset(path))
    if not asset:
        return
    print("asset class={} name={}".format(asset.get_class().get_name(), asset.get_name()))
    print("asset transform/bone API: {}".format([name for name in dir(asset) if any(token in name.lower() for token in ("bone", "bounds", "transform", "socket"))]))
    for name in socket_names:
        socket = call("find_socket {}".format(name), lambda n=name: asset.find_socket(unreal.Name(n)))
        if not socket:
            continue
        call("find_socket_info {}".format(name), lambda n=name, a=asset: a.find_socket_info(unreal.Name(n)))
        for prop in ("socket_name", "bone_name", "relative_location", "relative_rotation", "relative_scale"):
            call("  {}.{}".format(name, prop), lambda s=socket, p=prop: s.get_editor_property(p))
    for prop in ("bounds", "skeleton"):
        call("asset.{}".format(prop), lambda a=asset, p=prop: a.get_editor_property(p))
    skeleton = call("asset.skeleton", lambda a=asset: a.get_editor_property("skeleton"))
    if skeleton:
        print("skeleton API: {}".format([name for name in dir(skeleton) if any(token in name.lower() for token in ("bone", "socket", "transform", "reference"))]))
        call("skeleton.get_reference_pose", skeleton.get_reference_pose)
    call("asset.get_bounds", asset.get_bounds)


def main():
    print("==== _diag_weapon_attachment start ====")
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    call("is_in_play_in_editor", level_editor.is_in_play_in_editor)
    call("editor_request_end_play", level_editor.editor_request_end_play)
    call("editor_end_play", unreal.EditorLevelLibrary.editor_end_play)
    print("EditorLevelLibrary methods: {}".format(
        [name for name in dir(unreal.EditorLevelLibrary) if "play" in name.lower() or "world" in name.lower()]))
    print("LevelEditorSubsystem methods: {}".format(
        [name for name in dir(unreal.LevelEditorSubsystem) if "play" in name.lower() or "level" in name.lower()]))
    print("Automation screenshot API: {}".format(
        [name for name in dir(unreal.AutomationLibrary) if "screenshot" in name.lower() or "image" in name.lower()]))
    print("take_automation_screenshot doc: {}".format(unreal.AutomationLibrary.take_automation_screenshot.__doc__))
    print("take_high_res_screenshot doc: {}".format(unreal.AutomationLibrary.take_high_res_screenshot.__doc__))
    print("UnrealEditorSubsystem camera API: {}".format(
        [name for name in dir(unreal.UnrealEditorSubsystem) if "camera" in name.lower() or "viewport" in name.lower()]))
    print("Actor component API: {}".format(
        [name for name in dir(unreal.Actor) if "component" in name.lower() or "actor" in name.lower()]))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for existing in actor_subsystem.get_all_level_actors():
        if existing.get_actor_label().startswith("BungeeWeaponPreview_"):
            actor_subsystem.destroy_actor(existing)
    probe_actor = call("spawn probe actor", lambda: actor_subsystem.spawn_actor_from_class(unreal.Actor, unreal.Vector(0, 0, 0)))
    if probe_actor:
        print("spawned actor component API: {}".format([name for name in dir(probe_actor) if "component" in name.lower()]))
        call("destroy probe actor", lambda a=probe_actor: actor_subsystem.destroy_actor(a))
    call("editor world", lambda: unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world())
    inspect_asset("/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan", ("WeaponHandSocket",))
    inspect_asset("/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B.Assault_Rifle_B", ("WeaponHandSocket", "Muzzle"))
    print("==== _diag_weapon_attachment done ====")


main()
