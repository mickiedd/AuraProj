"""Read-only probe for the Guidemen swap in the showcase level.

Reports the editor's current world, whether any package is dirty, the showcase
actors actually placed, and whether both Guidemen Blueprints load as Blueprints
with a usable generated class. Writes nothing and saves nothing.
"""

import json

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"
CANDIDATES = [
    ("current", ROOT + "/V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("repair", ROOT + "/V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918"),
]


def package_names(packages):
    out = []
    for package in packages:
        try:
            out.append(str(package.get_name()))
        except Exception:
            out.append(str(package))
    return out


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("PROBE_WORLD", world.get_path_name() if world else None)

    for label, getter in (("CONTENT", "get_dirty_content_packages"),
                          ("MAPS", "get_dirty_map_packages")):
        try:
            packages = getattr(unreal.EditorLoadingAndSavingUtils, getter)()
            print("PROBE_DIRTY_" + label, json.dumps(package_names(packages)))
        except Exception as exc:
            print("PROBE_DIRTY_" + label + "_FAILED", repr(exc))

    actors = []
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        try:
            tags = [str(tag) for tag in actor.tags]
        except Exception:
            tags = []
        if "GuangzhouLandmarkShowcase" not in tags:
            continue
        location = actor.get_actor_location()
        actors.append({
            "label": actor.get_actor_label(),
            "class": actor.get_class().get_name(),
            "location": [round(float(location.x), 2), round(float(location.y), 2),
                         round(float(location.z), 2)],
            "tags": tags,
        })
    print("PROBE_SHOWCASE_ACTOR_COUNT", len(actors))
    print("PROBE_SHOWCASE_ACTORS", json.dumps(actors))

    for key, path in CANDIDATES:
        info = {"key": key, "path": path}
        info["exists"] = bool(unreal.EditorAssetLibrary.does_asset_exist(path))
        asset = unreal.EditorAssetLibrary.load_asset(path) if info["exists"] else None
        info["class"] = asset.get_class().get_name() if asset else None
        info["is_blueprint"] = bool(isinstance(asset, unreal.Blueprint))
        if info["is_blueprint"]:
            try:
                info["generated_class"] = asset.generated_class().get_name()
            except Exception as exc:
                info["generated_class"] = "ERR " + repr(exc)
            try:
                referenced = unreal.EditorAssetLibrary.find_package_referencers_for_asset(path)
                info["referencers"] = [str(name) for name in referenced]
            except Exception as exc:
                info["referencers"] = "ERR " + repr(exc)
        print("PROBE_CANDIDATE", json.dumps(info))

    print("PROBE_DONE")


main()
