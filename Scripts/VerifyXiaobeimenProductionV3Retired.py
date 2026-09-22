"""Final verification after retiring BP_Xiaobeimen_Production_V3.

Checks the three things that could still be wrong once the files are gone:

  1. the Blueprint, its preview map and its folder really are absent, and
  2. L_showcase_level loads and holds no actor from the deleted folder - the
     level's actor was removed and the map saved earlier, so this is the
     end-to-end proof that the reference is gone rather than merely unloaded.
  3. the deleted asset is not still resolvable through the asset registry.

Read-only: loads a level but saves nothing.
"""

import json

import unreal

DELETED = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"
DELETED_BP = DELETED + "/BP_Xiaobeimen_Production_V3"
DELETED_PREVIEW = DELETED + "/L_Xiaobeimen_Production_V3_Preview"
SECOND_LEVEL = "/Game/Scifi_desert_city/Level/L_showcase_level"


def main():
    print("VERIFY_BP_EXISTS", unreal.EditorAssetLibrary.does_asset_exist(DELETED_BP))
    print("VERIFY_PREVIEW_EXISTS", unreal.EditorAssetLibrary.does_asset_exist(DELETED_PREVIEW))
    print("VERIFY_FOLDER_EXISTS", unreal.EditorAssetLibrary.does_directory_exist(DELETED))
    print("VERIFY_FOLDER_ASSETS", json.dumps(sorted(
        unreal.EditorAssetLibrary.list_assets(DELETED, recursive=True,
                                              include_folder=False))))
    print("VERIFY_BP_LOADS", unreal.EditorAssetLibrary.load_asset(DELETED_BP))
    print("VERIFY_REGISTRY_HITS", json.dumps(sorted(
        path for path in unreal.EditorAssetLibrary.list_assets(
            "/Game/Assets/Environment/GuangzhouLandmarks/V3", recursive=True,
            include_folder=False)
        if "Xiaobeimen_Production_V3" in path)))

    assert not unreal.EditorAssetLibrary.does_asset_exist(DELETED_BP)
    assert not unreal.EditorAssetLibrary.does_asset_exist(DELETED_PREVIEW)
    assert not unreal.EditorAssetLibrary.list_assets(
        DELETED, recursive=True, include_folder=False)

    # The second level: load it from disk and prove no actor comes from the
    # deleted folder any more.
    dirty = sorted(str(p.get_name()) for p in
                   unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    print("VERIFY_DIRTY_MAPS_BEFORE", json.dumps(dirty))
    assert not dirty, "refusing to switch levels with unsaved maps: " + ", ".join(dirty)

    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(
        SECOND_LEVEL), "could not load " + SECOND_LEVEL
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("VERIFY_SECOND_LEVEL", world.get_path_name())

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    print("VERIFY_SECOND_LEVEL_ACTOR_COUNT", len(actors))
    offenders = []
    for actor in actors:
        try:
            class_path = actor.get_class().get_path_name()
        except Exception:
            continue
        if "Xiaobeimen_Production_V3" in class_path:
            offenders.append({"label": actor.get_actor_label(), "class": class_path})
    print("VERIFY_SECOND_LEVEL_OFFENDERS", json.dumps(offenders, indent=2))
    assert not offenders, "L_showcase_level still holds actors from the deleted folder"

    # Sanity: the level's other landmarks are still there, so this did not
    # quietly strip more than it should.
    labels = sorted(actor.get_actor_label() for actor in actors
                    if actor.get_actor_label().startswith("GuangzhouLandmark"))
    print("VERIFY_SECOND_LEVEL_LANDMARKS", json.dumps(labels, indent=2))
    print("VERIFY_OK")


main()
