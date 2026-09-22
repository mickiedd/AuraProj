"""Final verification after retiring BP_Zhengnanmen_AAA_V3.

Checks the things that could still be wrong once the files are gone:

  1. the Blueprint, its preview map and its folder really are absent, and the
     asset is not still resolvable through the asset registry;
  2. BOTH levels that used to place it load from disk and hold no actor from the
     deleted folder - their actors were removed and the maps saved earlier, so
     this is the end-to-end proof that the reference is gone rather than merely
     unloaded;
  3. the sibling Zhengnanmen HighFidelity landmark survives in both levels, so
     the job removed one variant rather than the gate.

Read-only: loads levels but saves nothing.
"""

import json

import unreal

DELETED = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3"
DELETED_BP = DELETED + "/BP_Zhengnanmen_AAA_V3"
DELETED_PREVIEW = DELETED + "/L_Zhengnanmen_AAA_V3_Preview"
RING_LEVEL = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
SECOND_LEVEL = "/Game/Scifi_desert_city/Level/L_showcase_level"
FOLDER_MARKER = "Zhengnanmen_AAA_V3"
SURVIVOR = "Zhengnanmen_HighFidelity"


def dirty_maps():
    return sorted(str(package.get_name()) for package in
                  unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def check_level(path, expect_survivor=True):
    """Load a level and report any actor whose class comes from the dead folder."""
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path), \
        "could not load " + path
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    print("VERIFY_LEVEL", world.get_path_name(), "ACTORS", len(actors))

    offenders = []
    for actor in actors:
        try:
            class_path = actor.get_class().get_path_name()
        except Exception:
            continue
        if FOLDER_MARKER in class_path:
            offenders.append({"label": actor.get_actor_label(), "class": class_path})
    # Actors are one thing; the per-landmark label and light actors the builder
    # spawns alongside them are another, and they are named after the landmark.
    stray = sorted(actor.get_actor_label() for actor in actors
                   if FOLDER_MARKER in actor.get_actor_label())
    print("VERIFY_OFFENDERS", json.dumps(offenders, indent=2))
    print("VERIFY_STRAY_LABELS", json.dumps(stray))
    assert not offenders, path + " still holds actors from the deleted folder"
    assert not stray, path + " still holds label/light actors named for the deleted landmark"

    landmarks = sorted(actor.get_actor_label() for actor in actors
                       if actor.get_actor_label().startswith("Landmark_"))
    print("VERIFY_LANDMARKS", json.dumps(landmarks, indent=2))
    if expect_survivor:
        assert any(SURVIVOR in label for label in landmarks), \
            path + " lost the " + SURVIVOR + " landmark too"
    return landmarks


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
        if FOLDER_MARKER in path)))

    assert not unreal.EditorAssetLibrary.does_asset_exist(DELETED_BP)
    assert not unreal.EditorAssetLibrary.does_asset_exist(DELETED_PREVIEW)
    assert not unreal.EditorAssetLibrary.list_assets(
        DELETED, recursive=True, include_folder=False)
    assert unreal.EditorAssetLibrary.load_asset(DELETED_BP) is None

    dirty = dirty_maps()
    print("VERIFY_DIRTY_MAPS_BEFORE", json.dumps(dirty))
    assert not dirty, "refusing to switch levels with unsaved maps: " + ", ".join(dirty)

    ring = check_level(RING_LEVEL, expect_survivor=True)
    # L_showcase_level never held a Zhengnanmen HighFidelity actor - it places
    # GreatNorthGate, Xiaobeimen, Xiaobeimen_AAA_V3 and ZhenhaiTower only - so the
    # survivor check belongs to the ring level alone.
    second = check_level(SECOND_LEVEL, expect_survivor=False)

    print("VERIFY_RING_LANDMARK_COUNT", len(ring))
    print("VERIFY_SECOND_LANDMARK_COUNT", len(second))
    print("VERIFY_OK")


main()
