import unreal, json
paths=['/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Textures/Plaster/T_Plaster_MR_4K']
for p in paths:
 t=unreal.EditorAssetLibrary.load_asset(p); print(p, t.blueprint_get_size_x(), t.blueprint_get_size_y())
