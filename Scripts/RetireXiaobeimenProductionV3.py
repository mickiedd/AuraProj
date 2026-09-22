"""Retire the Xiaobeimen Production V3 variant and its whole asset folder.

Two levels used to place this Blueprint, not one:

  * L_GuangzhouLandmarkShowcase - rebuilt from the registry with the landmark
    dropped, and saved.
  * Scifi_desert_city/Level/L_showcase_level - its one actor was removed and the
    map saved by RemoveShowcaseLevelXiaobeimenProductionV3.py.

Both had to be cleared before this runs, because deleting an asset something
still points at leaves a broken reference behind. So this refuses to delete
unless every referencer it finds is INSIDE the folder being deleted. The preview
map L_Xiaobeimen_Production_V3_Preview and the Blueprint reference each other,
which is exactly why the check is "outside referencers only" rather than "no
referencers at all".

Note the folder is self-contained - its own Meshes, Materials and Textures - so
nothing outside it should reference the meshes either; the outside check covers
every asset in the folder, not just the Blueprint.

The preview map is deleted first, then the Blueprint, then whatever remains, and
the asset is verified gone afterwards rather than trusting a return value alone.

RESUMABLE: the folder holds 176 assets and the referencer query plus the sweep
take long enough that a first attempt can be cut short partway (the Blueprint and
preview map go first, because they are the pair that reference each other). So
this does not assert that the Blueprint still exists at the start - it reports
what is left and finishes the job. Re-run it until it prints RETIRE_OK.
"""

import json

import unreal

TARGET = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"
TARGET_BP = TARGET + "/BP_Xiaobeimen_Production_V3"
TARGET_PREVIEW = TARGET + "/L_Xiaobeimen_Production_V3_Preview"


def names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return sorted(out)


print("RETIRE_BP_PRESENT_AT_START",
      unreal.EditorAssetLibrary.does_asset_exist(TARGET_BP))
print("RETIRE_PREVIEW_PRESENT_AT_START",
      unreal.EditorAssetLibrary.does_asset_exist(TARGET_PREVIEW))

assets = sorted(unreal.EditorAssetLibrary.list_assets(
    TARGET, recursive=True, include_folder=False))
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
        deleted = unreal.EditorAssetLibrary.delete_asset(path)
        print("RETIRE_DELETE_ASSET", path, deleted)
        assert deleted, "delete_asset returned False for " + path

swept = unreal.EditorAssetLibrary.delete_directory(TARGET)
print("RETIRE_DELETE_DIRECTORY_RETURNED", swept)

# delete_directory can decline assets that reference each other inside the
# folder. Sweep the remainder in passes, deleting whatever will go and letting
# the next pass pick up whatever that unblocked, until nothing more moves.
for attempt in range(8):
    leftovers = sorted(unreal.EditorAssetLibrary.list_assets(
        TARGET, recursive=True, include_folder=False))
    if not leftovers:
        break
    progressed = False
    for path in leftovers:
        if unreal.EditorAssetLibrary.delete_asset(path):
            progressed = True
    print("RETIRE_SWEEP_PASS", attempt, "remaining", len(leftovers),
          "progressed", progressed)
    if not progressed:
        break

leftovers = sorted(unreal.EditorAssetLibrary.list_assets(
    TARGET, recursive=True, include_folder=False))
print("RETIRE_LEFTOVERS", json.dumps(leftovers))
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
