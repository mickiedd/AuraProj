import unreal

for path in [
    '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_DoorWood_ReferenceTuned',
    '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Iron_ReferenceTuned',
    '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Approach_ReferenceTuned',
]:
    material = unreal.EditorAssetLibrary.load_asset(path)
    print('MATERIAL', path, 'blend', material.get_editor_property('blend_mode'), 'shading', material.get_editor_property('shading_model'))
    for expr in material.get_editor_property('expressions'):
        print('EXPR', type(expr).__name__, expr.get_editor_property('material_expression_editor_x'), expr.get_editor_property('material_expression_editor_y'))
