"""Read-only probe before retiring BP_Xiaobeimen_Production_V3.

Answers three questions without loading or saving anything:

  1. What level is open, and is anything dirty (the shared editor must be quiet
     before a rebuild pass switches levels)?
  2. Which landmark-tagged actors in the OPEN level come from this Blueprint?
  3. Who references the Blueprint and the rest of its folder from OUTSIDE that
     folder? A folder's preview map and its Blueprint reference each other, so
     referencers inside the folder are expected and are reported separately.

No writes, no level switches, no saves.
"""

import json

import unreal

TARGET_DIR = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"
TARGET_BP = TARGET_DIR + "/BP_Xiaobeimen_Production_V3"
TARGET_PREVIEW = TARGET_DIR + "/L_Xiaobeimen_Production_V3_Preview"
SHOWCASE_TAG = "GuangzhouLandmarkShowcase"


def names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return sorted(out)


def main():
    print("PROBE_BP_EXISTS", unreal.EditorAssetLibrary.does_asset_exist(TARGET_BP))
    print("PROBE_PREVIEW_EXISTS", unreal.EditorAssetLibrary.does_asset_exist(TARGET_PREVIEW))

    assets = sorted(unreal.EditorAssetLibrary.list_assets(
        TARGET_DIR, recursive=True, include_folder=False))
    print("PROBE_FOLDER_ASSET_COUNT", len(assets))
    for path in assets:
        print("PROBE_FOLDER_ASSET", path)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("PROBE_OPEN_LEVEL", world.get_path_name() if world else None)
    print("PROBE_DIRTY_CONTENT", json.dumps(
        names(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())))
    print("PROBE_DIRTY_MAPS", json.dumps(
        names(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())))

    # Which actors in the OPEN level are instances of the target Blueprint?
    blueprint = unreal.EditorAssetLibrary.load_asset(TARGET_BP)
    print("PROBE_BP_LOADED", blueprint.get_path_name() if blueprint else None)
    generated = blueprint.generated_class().get_path_name() if blueprint else None
    print("PROBE_GENERATED_CLASS", generated)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    print("PROBE_OPEN_LEVEL_ACTOR_COUNT", len(actors))
    matched = []
    for actor in actors:
        try:
            cls = actor.get_class()
            cls_path = cls.get_path_name()
        except Exception:
            continue
        tags = [str(tag) for tag in actor.tags]
        if generated and cls_path.startswith(generated.split(":")[0]):
            matched.append({"label": actor.get_actor_label(), "class": cls_path,
                            "tags": tags})
        elif "Xiaobeimen" in actor.get_actor_label():
            matched.append({"label": actor.get_actor_label(), "class": cls_path,
                            "tags": tags, "note": "label match only"})
    print("PROBE_OPEN_LEVEL_MATCHES", json.dumps(matched, indent=2))

    # Referencers, split into inside-folder and outside-folder.
    inside = {path.split(".")[0] for path in assets}
    outside = {}
    inside_refs = {}
    for path in assets:
        package = path.split(".")[0]
        try:
            referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
                package, load_assets_to_confirm=False)
        except Exception as exc:
            print("PROBE_REFERENCER_QUERY_FAILED", package, repr(exc))
            referencers = []
        for referencer in referencers:
            name = str(referencer)
            bucket = inside_refs if name in inside else outside
            bucket.setdefault(name, []).append(package)
    print("PROBE_INSIDE_REFERENCERS", json.dumps(inside_refs, indent=2, sort_keys=True))
    print("PROBE_OUTSIDE_REFERENCERS", json.dumps(outside, indent=2, sort_keys=True))
    print("PROBE_OK")


main()
