"""Open the updated Wenmingmen Blueprint for a live visual review."""
import unreal

asset = unreal.EditorAssetLibrary.load_asset(
    "/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen/BP_Wenmingmen"
)
assert isinstance(asset, unreal.Blueprint)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
print("WENMINGMEN_REVIEW_OPEN", asset.get_path_name())
