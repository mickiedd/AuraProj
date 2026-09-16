import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
for name in (
    "M_GlazedTile_Green",
    "M_Gold_RidgeOrnament",
    "M_Signboard_Zhengnanmen",
    "M_Stone_Aged",
    "M_Wood_DarkAged",
    "M_Wood_RedLacquer",
    "M_DarkInterior",
):
    path = ROOT + "/" + name
    material = unreal.EditorAssetLibrary.load_asset(path)
    print("MATERIAL", path, "expressions", unreal.MaterialEditingLibrary.get_num_material_expressions(material) if material else -1)
    if not material:
        continue
    for texture in unreal.MaterialEditingLibrary.get_used_textures(material):
        print(" TEXTURE", texture.get_path_name(), texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
