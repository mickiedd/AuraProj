# unpopulate_desert_houses.py
# Run inside Unreal Editor's Python (console:  py unpopulate_desert_houses.py).
# Deletes the StaticMeshActors listed in Saved/Scripts/desert_houses_manifest.json
# (created by populate_desert_houses.py). Use this to revert/clean up before
# re-running populate with different parameters.
#
# SAFE: only acts on actors whose name appears in the manifest, so it won't
# touch anything you placed manually. Actors that no longer exist in the level
# (e.g. because the level was reverted) are silently skipped.

import json
import os

import unreal

MANIFEST_FILE_NAME = "Saved/Scripts/desert_houses_manifest.json"


def _all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()


def main():
    proj = unreal.SystemLibrary.get_project_directory()
    path = os.path.join(proj, MANIFEST_FILE_NAME)
    if not os.path.exists(path):
        unreal.log_error("Manifest not found: {}".format(path))
        return
    with open(path, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    names = {e["actor"] for e in manifest}
    unreal.log("Manifest lists {} actors; searching level...".format(len(names)))

    actors = _all_actors()
    by_name = {a.get_name(): a for a in actors if a is not None}

    n_deleted = 0
    n_missing = 0
    for name in names:
        a = by_name.get(name)
        if a is None:
            n_missing += 1
            continue
        try:
            unreal.EditorLevelLibrary.destroy_actor(a)
            n_deleted += 1
        except Exception as e:
            unreal.log_error("  failed to delete {}: {}".format(name, e))

    unreal.log("Deleted {} actors ({} already missing).".format(n_deleted, n_missing))
    unreal.log("Remember to SAVE THE LEVEL (Ctrl+S) to persist.")


main()