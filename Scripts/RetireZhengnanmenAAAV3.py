"""Retire the Zhengnanmen AAA V3 variant and its whole asset folder.

Same shape as RetireXiaobeimenProductionV3.py, with the lesson from that run
baked in: this never calls EditorAssetLibrary.delete_directory. On the 176-asset
Xiaobeimen folder that call froze the game thread inside "Force Deleting N
Package(s)" with 0 CPU over 15 s and no modal dialog, and the editor had to be
restarted. Deleting one asset at a time through delete_asset did the same job
with zero refusals, so that is the only path used here.

Two levels used to place this Blueprint, not one:

  * L_GuangzhouLandmarkShowcase - rebuilt from the registry with the landmark
    dropped, and saved.
  * Scifi_desert_city/Level/L_showcase_level - its one actor was removed and the
    map saved by RemoveShowcaseLevelZhengnanmenAAAV3.py.

Both had to be cleared before this runs, because deleting an asset something
still points at leaves a broken reference behind. So this refuses to delete
unless every referencer it finds is INSIDE the folder being deleted. The preview
map L_Zhengnanmen_AAA_V3_Preview and the Blueprint reference each other, which is
exactly why the check is "outside referencers only" rather than "no referencers
at all".

The folder is self-contained - its own Meshes, Materials and Textures - so
nothing outside it should reference the meshes either; the outside check covers
every asset in the folder, not just the Blueprint.

RESUMABLE: this does not assert that the Blueprint still exists at the start. It
reports what is left and finishes the job, so a run cut short can simply be
re-run until it prints RETIRE_OK. Each path is logged BEFORE it is deleted, so a
hang names its own culprit.
"""

import json

import unreal

TARGET = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3"
TARGET_BP = TARGET + "/BP_Zhengnanmen_AAA_V3"
TARGET_PREVIEW = TARGET + "/L_Zhengnanmen_AAA_V3_Preview"

# Delete the assets nothing else depends on last, so the sweep rarely has to
# retry: the two root assets reference the meshes, the meshes reference the
# materials, the materials reference the textures.
ORDER = ("/Textures/", "/Materials/", "/Meshes/")
BATCH = 200
MAX_PASSES = 12


def names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return sorted(out)


def remaining():
    return sorted(unreal.EditorAssetLibrary.list_assets(
        TARGET, recursive=True, include_folder=False))


print("RETIRE_BP_PRESENT_AT_START",
      unreal.EditorAssetLibrary.does_asset_exist(TARGET_BP))
print("RETIRE_PREVIEW_PRESENT_AT_START",
      unreal.EditorAssetLibrary.does_asset_exist(TARGET_PREVIEW))

assets = remaining()
print("RETIRE_ASSET_COUNT", len(assets))
if not assets:
    print("RETIRE_ALREADY_GONE")
    print("RETIRE_OK")
    raise SystemExit(0)

inside = {path.split(".")[0] for path in assets}

print("RETIRE_DIRTY_CONTENT_BEFORE",
      json.dumps(names(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())))
print("RETIRE_DIRTY_MAPS_BEFORE",
      json.dumps(names(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())))

outside = {}
for path in assets:
    package = path.split(".")[0]
    try:
        referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
            package, load_assets_to_confirm=False)
    except Exception as exc:
        print("RETIRE_REFERENCER_QUERY_FAILED", package, repr(exc))
        referencers = []
    for referencer in referencers:
        name = str(referencer)
        if name not in inside:
            outside.setdefault(name, []).append(package)
print("RETIRE_OUTSIDE_REFERENCERS", json.dumps(outside, indent=2, sort_keys=True))
assert not outside, \
    "refusing to delete " + TARGET + "; still referenced from outside by " + \
    ", ".join(sorted(outside))

# Delete the two assets that reference each other first, so neither can fail on
# a dangling referencer, then sweep up the rest of the folder.
for path in (TARGET_PREVIEW, TARGET_BP):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        print("RETIRE_ABOUT_TO_DELETE", path)
        deleted = unreal.EditorAssetLibrary.delete_asset(path)
        print("RETIRE_DELETE_RESULT", path, deleted)
        assert deleted, "delete_asset returned False for " + path

# Sweep the remainder, ordered so leaves go last. Passes repeat because deleting
# one asset can unblock another; the loop stops when a pass makes no progress.
total_deleted = 0
refused = []
for attempt in range(MAX_PASSES):
    queue = remaining()
    if not queue:
        break
    queue.sort(key=lambda p: next(
        (ORDER.index(prefix) for prefix in ORDER if prefix in p), len(ORDER)))
    progressed = False
    for index, path in enumerate(queue[:BATCH], start=1):
        print("RETIRE_ABOUT_TO_DELETE", path)
        if unreal.EditorAssetLibrary.delete_asset(path):
            total_deleted += 1
            progressed = True
        else:
            refused.append(path)
            print("RETIRE_DELETE_REFUSED", path)
        if index % 25 == 0:
            print("RETIRE_PROGRESS pass", attempt, "done", index, "of", len(queue[:BATCH]))
    print("RETIRE_SWEEP_PASS", attempt, "queued", len(queue),
          "deleted_so_far", total_deleted, "progressed", progressed)
    if not progressed:
        break

leftovers = remaining()
print("RETIRE_DELETED_COUNT", total_deleted)
print("RETIRE_REFUSED", json.dumps(refused))
print("RETIRE_LEFTOVERS", json.dumps(leftovers))
print("RETIRE_FOLDER_STILL_EXISTS",
      unreal.EditorAssetLibrary.does_directory_exist(TARGET))

still_there = unreal.EditorAssetLibrary.does_asset_exist(TARGET_BP)
print("RETIRE_STILL_EXISTS", still_there)
assert not still_there, "blueprint survived deletion: " + TARGET_BP
assert not leftovers, "assets survived deletion: " + ", ".join(leftovers)

# The Blueprint class must no longer resolve, or the two levels that used to
# reference it would still load something.
resolved = unreal.EditorAssetLibrary.load_asset(TARGET_BP)
print("RETIRE_RELOAD_AFTER_DELETE", resolved)
assert resolved is None, "the Blueprint still loads: " + TARGET_BP

print("RETIRE_OK")
