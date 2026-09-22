"""Finish the Xiaobeimen Production V3 folder deletion, leaf-first and batched.

The first attempt got the two assets that reference each other - the preview map
and the Blueprint - deleted, then wedged the editor inside
EditorAssetLibrary.delete_directory: the editor log stops at "Force Deleting 174
Package(s)" and the game thread then sat at 0% CPU until the editor was
restarted. So this deliberately does NOT call delete_directory at all. It deletes
one asset at a time instead, and logs each path BEFORE deleting it, so if the
editor wedges again the log names the asset responsible instead of going silent.

Order is leaf-first - textures, then materials, then meshes - so each deletion
targets something nothing else depends on, which is also the order that avoids
the "still referenced" refusals.

Batched on purpose: it stops after BATCH assets and reports what is left, so a
wedge costs at most one batch of progress and re-running simply continues. The
folder is git-tracked, so any asset this removes is recoverable from git.
"""

import json

import unreal

TARGET = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"
BATCH = 200

# Leaf-first. Textures are referenced by materials, materials by meshes, meshes
# by the (already deleted) Blueprint, so deleting in this order never removes
# something that still has a dependant above it.
ORDER = ("/Textures/", "/Materials/", "/Meshes/")


def rank(path):
    for index, marker in enumerate(ORDER):
        if marker in path:
            return index
    return len(ORDER)


def leftovers():
    return sorted(unreal.EditorAssetLibrary.list_assets(
        TARGET, recursive=True, include_folder=False))


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("FINISH_OPEN_LEVEL", world.get_path_name() if world else None)
    print("FINISH_DIRTY_CONTENT", json.dumps(sorted(
        str(p.get_name()) for p in
        unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())))
    print("FINISH_DIRTY_MAPS", json.dumps(sorted(
        str(p.get_name()) for p in
        unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())))

    remaining = leftovers()
    print("FINISH_REMAINING_AT_START", len(remaining))
    if not remaining:
        print("FINISH_NOTHING_TO_DO")
        print("FINISH_OK")
        return

    # Leaf-first, and stable within a group so re-runs proceed predictably.
    queue = sorted(remaining, key=lambda path: (rank(path), path))[:BATCH]
    print("FINISH_BATCH_SIZE", len(queue))

    deleted = []
    refused = []
    for path in queue:
        print("FINISH_ABOUT_TO_DELETE", path)
        try:
            ok = unreal.EditorAssetLibrary.delete_asset(path)
        except Exception as exc:
            print("FINISH_DELETE_RAISED", path, repr(exc))
            ok = False
        if ok:
            deleted.append(path)
        else:
            refused.append(path)
        print("FINISH_DELETE_RESULT", path, ok)

    print("FINISH_DELETED_COUNT", len(deleted))
    print("FINISH_REFUSED", json.dumps(refused, indent=2))

    remaining = leftovers()
    print("FINISH_REMAINING_AFTER", len(remaining))
    print("FINISH_OK")


main()
