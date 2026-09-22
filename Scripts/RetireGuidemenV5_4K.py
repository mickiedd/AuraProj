"""Retire BP_Guidemen_V5_4K.

The showcase level was rebuilt to place BP_Guidemen_V5_4K_PreRebuild_20260918
(the reference-repaired model) and saved, so the retired Blueprint should have no
referencers left. Deleting an asset something still points at would leave a
broken reference behind, so this refuses to delete unless the referencer list is
empty, and it verifies the asset is really gone afterwards rather than trusting
the return value alone.
"""

import json

import unreal

TARGET = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"


def package_names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return out


assert unreal.EditorAssetLibrary.does_asset_exist(TARGET), "already gone: " + TARGET

referencers = sorted(
    str(name) for name in unreal.EditorAssetLibrary.find_package_referencers_for_asset(TARGET))
print("RETIRE_REFERENCERS", json.dumps(referencers))
assert not referencers, \
    "refusing to delete " + TARGET + "; still referenced by " + ", ".join(referencers)

print("RETIRE_DIRTY_CONTENT_BEFORE",
      json.dumps(package_names(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())))
print("RETIRE_DIRTY_MAPS_BEFORE",
      json.dumps(package_names(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())))

deleted = unreal.EditorAssetLibrary.delete_asset(TARGET)
print("RETIRE_DELETE_ASSET_RETURNED", deleted)
still_there = unreal.EditorAssetLibrary.does_asset_exist(TARGET)
print("RETIRE_STILL_EXISTS", still_there)
assert deleted, "delete_asset returned False for " + TARGET
assert not still_there, "asset survived deletion: " + TARGET

print("RETIRE_OK")
