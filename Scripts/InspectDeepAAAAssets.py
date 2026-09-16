import unreal

for path in (
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/DeepAAA/Wuxianmen_V4_DeepAAA.Wuxianmen_V4_DeepAAA",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/DeepAAA/Wuxianmen_V4_DeepAAA/Wuxianmen_V4_DeepAAA.Wuxianmen_V4_DeepAAA",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Meshes/DeepAAA/Zhengximen_V4_DeepAAA.Zhengximen_V4_DeepAAA",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Meshes/DeepAAA/Zhengximen_V4_DeepAAA/Zhengximen_V4_DeepAAA.Zhengximen_V4_DeepAAA",
):
    print("DEEP_ASSET", path, unreal.EditorAssetLibrary.does_asset_exist(path), str(unreal.EditorAssetLibrary.load_asset(path)))
