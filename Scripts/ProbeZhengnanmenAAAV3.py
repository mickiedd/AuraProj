"""Read-only probe before retiring BP_Zhengnanmen_AAA_V3.

Same questions as ProbeXiaobeimenProductionV3.py:

  1. What level is open, and is anything dirty (the shared editor must be quiet
     before a rebuild pass switches levels)?
  2. Which actors in the OPEN level come from this Blueprint?
  3. Who references the folder from OUTSIDE it? A folder's preview map and its
     Blueprint reference each other, so inside-folder referencers are expected
     and are reported separately.
  4. What is actually in the folder - including the preview map's name, which is
     discovered here rather than guessed, because the Xiaobeimen folder's naming
     may not generalise.

No writes, no level switches, no saves.
"""

import json

import unreal

TARGET_DIR = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3"
TARGET_BP = TARGET_DIR + "/BP_Zhengnanmen_AAA_V3"
LABEL_HINT = "Zhengnanmen"


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
    print("PROBE_FOLDER_EXISTS",
          unreal.EditorAssetLibrary.does_directory_exist(TARGET_DIR))

    assets = sorted(unreal.EditorAssetLibrary.list_assets(
        TARGET_DIR, recursive=True, include_folder=False))
    print("PROBE_FOLDER_ASSET_COUNT", len(assets))
    for path in assets:
        print("PROBE_FOLDER_ASSET", path)

    # Discover the preview map rather than assuming a name for it.
    previews = [path for path in assets
                if path.rsplit("/", 1)[-1].split(".")[0].startswith("L_")]
    print("PROBE_PREVIEW_CANDIDATES", json.dumps(previews))

    # Which subfolders the assets live in, and how many each holds. If the
    # variant shares a subfolder with a sibling variant, the outside-referencer
    # check below is what catches it.
    subfolders = {}
    for path in assets:
        parts = path[len(TARGET_DIR) + 1:].split("/")
        key = parts[0] if len(parts) > 1 else "(root)"
        subfolders[key] = subfolders.get(key, 0) + 1
    print("PROBE_SUBFOLDERS", json.dumps(subfolders, indent=2, sort_keys=True))

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
            class_path = actor.get_class().get_path_name()
        except Exception:
            continue
        tags = [str(tag) for tag in actor.tags]
        if generated and class_path.startswith(generated.split(":")[0]):
            matched.append({"label": actor.get_actor_label(), "class": class_path,
                            "tags": tags})
        elif LABEL_HINT in actor.get_actor_label():
            # A label-only hit is a lead, not a match - the sibling
            # Zhengnanmen_HighFidelity actor will show up here too.
            matched.append({"label": actor.get_actor_label(), "class": class_path,
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
