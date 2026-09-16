import unreal
paths = unreal.EditorAssetLibrary.list_assets('/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/RoofClean', recursive=True, include_folder=False)
print('ROOF_ASSETS', paths)

