import unreal

for name in ('M_Stone', 'M_DoorWood', 'M_RoofGlaze'):
    path = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/Materials/' + name
    m = unreal.EditorAssetLibrary.load_asset(path)
    print('MATERIAL', name, 'blend', str(m.get_editor_property('blend_mode')), 'two_sided', m.get_editor_property('two_sided'))
    try:
        for e in unreal.MaterialEditingLibrary.get_material_expressions(m):
            print('EXPR', e.get_class().get_name(), 'texture=', getattr(e, 'texture', None), 'param=', getattr(e, 'parameter_name', None))
    except Exception as exc:
        print('EXPR_ERROR', exc)
