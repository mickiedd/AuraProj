import unreal

for path in (
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/Wuxianmen_NaniteHigh/WXM_WeatheredStone_NaniteHigh.WXM_WeatheredStone_NaniteHigh",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/Wuxianmen_NaniteHigh/WXM_AgedWood_NaniteHigh.WXM_AgedWood_NaniteHigh",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/Wuxianmen_NaniteHigh/WXM_ClayRoof_NaniteHigh.WXM_ClayRoof_NaniteHigh",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/Wuxianmen_NaniteHigh/WXM_GateWood_NaniteHigh.WXM_GateWood_NaniteHigh",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Meshes/SM_Zhengximen_LOD0/SM_Zhengximen_LOD0.SM_Zhengximen_LOD0",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Meshes/DeepAAA/Wuxianmen_V4_DeepAAA/Wuxianmen_V4_DeepAAA.Wuxianmen_V4_DeepAAA",
    "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Meshes/DeepAAA/Zhengximen_V4_DeepAAA/Zhengximen_V4_DeepAAA.Zhengximen_V4_DeepAAA",
):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    try:
        print("MESH_BOUNDS", path, mesh.get_bounds())
    except Exception as exc:
        print("MESH_BOUNDS_ERROR", path, exc)
