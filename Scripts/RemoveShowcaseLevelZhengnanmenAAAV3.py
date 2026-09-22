"""Remove BP_Zhengnanmen_AAA_V3's actor from Scifi_desert_city/Level/L_showcase_level.

Exactly the situation Xiaobeimen_Production_V3 was in: the ring showcase is not
the only level holding this Blueprint. L_showcase_level carries an actor spawned
from it, and deleting the Blueprint while that actor still exists would leave the
level with a dangling reference, so the actor goes first and the level is saved.

Deliberately narrow: this only destroys actors whose class actually comes from
the target Blueprint. The other GuangzhouLandmark_* actors in the level belong to
different assets and are left alone. Note that the sibling Zhengnanmen
HighFidelity actor lives in this level too and must survive.

Anything attached to the doomed actor is reported before it is destroyed, since
destroying a parent takes its children with it.

Loads the level, so it must not run while the editor has unsaved work.
"""

import json

import unreal

LEVEL = "/Game/Scifi_desert_city/Level/L_showcase_level"
BP = ("/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3"
      "/BP_Zhengnanmen_AAA_V3")


def dirty_maps():
    return sorted(str(package.get_name()) for package in
                  unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def main():
    print("REMOVE_DIRTY_BEFORE", json.dumps(dirty_maps()))
    assert not dirty_maps(), "refusing to switch levels with unsaved maps: " + \
        ", ".join(dirty_maps())

    assert unreal.EditorAssetLibrary.does_asset_exist(LEVEL), "missing " + LEVEL
    assert unreal.EditorAssetLibrary.does_asset_exist(BP), "missing " + BP

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not (world and world.get_path_name().startswith(LEVEL)):
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL), \
            "could not load " + LEVEL
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL), world
    print("REMOVE_LEVEL", world.get_path_name())

    blueprint = unreal.EditorAssetLibrary.load_asset(BP)
    assert blueprint, BP
    # Class path prefix is the asset path; the generated class appends _C, so a
    # prefix match catches the actor class and any subclass of it.
    class_prefix = BP
    print("REMOVE_CLASS_PREFIX", class_prefix)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    print("REMOVE_ACTOR_COUNT_BEFORE", len(actors))

    targets = []
    for actor in actors:
        try:
            class_path = actor.get_class().get_path_name()
        except Exception as exc:
            print("REMOVE_CLASS_READ_FAILED", repr(exc))
            continue
        if class_path.startswith(class_prefix):
            targets.append(actor)

    print("REMOVE_TARGETS", json.dumps([
        {"label": actor.get_actor_label(), "name": actor.get_name(),
         "class": actor.get_class().get_path_name(),
         "tags": [str(tag) for tag in actor.tags],
         "location": [round(float(actor.get_actor_location().x), 2),
                      round(float(actor.get_actor_location().y), 2),
                      round(float(actor.get_actor_location().z), 2)],
         "attached_children": [child.get_actor_label()
                               for child in actor.get_attached_actors()]}
        for actor in targets], indent=2))
    assert len(targets) == 1, \
        "expected exactly 1 actor from {} in {}, found {}".format(
            BP, LEVEL, len(targets))

    target = targets[0]
    for child in target.get_attached_actors():
        print("REMOVE_CHILD_GOES_TOO", child.get_actor_label())

    assert actor_subsystem.destroy_actor(target), \
        "destroy_actor refused " + target.get_actor_label()

    remaining = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
                 if actor.get_class().get_path_name().startswith(class_prefix)]
    print("REMOVE_REMAINING", json.dumps(remaining))
    assert not remaining, "actors from the Blueprint survived: " + ", ".join(remaining)

    print("REMOVE_ACTOR_COUNT_AFTER", len(actor_subsystem.get_all_level_actors()))

    # The sibling HighFidelity Zhengnanmen must still be here - this job removes
    # one variant, not the gate.
    survivors = sorted(actor.get_actor_label()
                       for actor in actor_subsystem.get_all_level_actors()
                       if actor.get_actor_label().startswith("GuangzhouLandmark_"))
    print("REMOVE_SURVIVING_LANDMARKS", json.dumps(survivors))

    assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL), "save failed"
    print("REMOVE_SAVED", LEVEL)
    print("REMOVE_DIRTY_AFTER", json.dumps(dirty_maps()))
    print("REMOVE_OK")


main()
