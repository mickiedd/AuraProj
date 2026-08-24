"""One-shot, idempotent editor migration for the Enemy nearest-hostile service.

Run with UnrealEditor-Cmd and PythonScriptPlugin enabled. The script reparents
the existing service Blueprint to the native hostile implementation, renames it,
loads both referring Behavior Trees before the rename so references are fixed,
and saves every touched package.
"""

import unreal


OLD_PATH = "/Game/Blueprints/AI/Services/BTS_FindNearestPlayer"
NEW_PATH = "/Game/Blueprints/AI/Services/BTS_FindNearestHostile"
TREE_PATHS = (
    "/Game/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree",
    "/Game/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree_Elementalist",
)


def main():
    trees = [unreal.EditorAssetLibrary.load_asset(path) for path in TREE_PATHS]
    if any(tree is None for tree in trees):
        raise RuntimeError("One or more Enemy Behavior Trees could not be loaded")

    if (unreal.EditorAssetLibrary.does_asset_exist(OLD_PATH)
            and unreal.EditorAssetLibrary.does_asset_exist(NEW_PATH)):
        if not unreal.EditorAssetLibrary.delete_asset(NEW_PATH):
            raise RuntimeError("Failed to remove the incomplete nearest-hostile migration asset")

    blueprint = unreal.EditorAssetLibrary.load_asset(NEW_PATH)
    if blueprint is None:
        blueprint = unreal.EditorAssetLibrary.load_asset(OLD_PATH)
        if blueprint is None:
            raise RuntimeError("Nearest-player service Blueprint could not be loaded")
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, unreal.BTService_FindNearestHostile)
        rename = unreal.AssetRenameData(
            asset=blueprint,
            new_package_path="/Game/Blueprints/AI/Services",
            new_name="BTS_FindNearestHostile",
        )
        if not unreal.AssetToolsHelpers.get_asset_tools().rename_assets([rename]):
            raise RuntimeError("Failed to rename BTS_FindNearestPlayer")
        blueprint = unreal.EditorAssetLibrary.load_asset(NEW_PATH)
    else:
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, unreal.BTService_FindNearestHostile)

    if blueprint is None:
        raise RuntimeError("Migrated nearest-hostile Blueprint could not be loaded")

    generated_class = blueprint.generated_class()
    if generated_class:
        default_service = unreal.get_default_object(generated_class)
        default_service.set_editor_property("node_name", "Find Nearest Hostile")
        default_service.modify()

    for asset in [blueprint] + trees:
        asset.modify()
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("Failed to save {}".format(asset.get_path_name()))

    unreal.log("[EnemyAI][Migration] Saved nearest-hostile service and both Enemy Behavior Trees")


main()
