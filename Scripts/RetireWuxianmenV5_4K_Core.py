"""Retire the Wuxianmen V5 4K Core variant and its whole asset folder.

The showcase level was rebuilt to place only BP_Wuxianmen_V5_FullPBR (the variant
carrying the 2026-09-22 reference repair) and saved, so nothing outside the Core
folder should still point at it.

Deleting an asset something still points at would leave a broken reference
behind, so this refuses to delete unless every referencer found is INSIDE the
folder being deleted. The preview map L_Wuxianmen_V5_4K_Core_Preview and the
Core Blueprint reference each other, which is exactly why the check is
"outside referencers only" rather than "no referencers at all".

The preview map is deleted first, then the Blueprint, then whatever remains, and
the asset is verified gone afterwards rather than trusting a return value alone.
"""

import json

import unreal

TARGET = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core"
TARGET_BP = TARGET + "/BP_Wuxianmen_V5_4K_Core"
TARGET_PREVIEW = TARGET + "/L_Wuxianmen_V5_4K_Core_Preview"


def names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return sorted(out)


assert unreal.EditorAssetLibrary.does_asset_exist(TARGET_BP), "already gone: " + TARGET_BP

assets = sorted(unreal.EditorAssetLibrary.list_assets(
    TARGET, recursive=True, include_folder=False))
print("RETIRE_ASSET_COUNT", len(assets))
assert assets, "no assets found under " + TARGET

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

print("RETIRE_OK")
