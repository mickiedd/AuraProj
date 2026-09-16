import unreal
paths = [
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Stone_AAA',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_DoorWood_AAA',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/Materials/M_Stone_BlueGrey_AAA',
]
paths += [
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Stone',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Wood',
]
for path in paths:
    m = unreal.EditorAssetLibrary.load_asset(path)
    print('MAT', path, 'blend', m.get_editor_property('blend_mode'), 'used', [t.get_path_name() for t in unreal.MaterialEditingLibrary.get_used_textures(m)])
