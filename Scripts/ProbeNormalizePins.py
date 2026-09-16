import unreal

mat = unreal.Material()
source = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -200, 0)
source.set_editor_property("constant", unreal.LinearColor(0.5, 0.5, 1.0, 1.0))
for pin in ("Input", "InputVector", "Vector", "V", "A", "Value", "X", "Normalized", "Normal"):
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionNormalize, 0, 0)
    try:
        ok = unreal.MaterialEditingLibrary.connect_material_expressions(source, "", node, pin)
        print("NORMALIZE_PIN", pin, ok)
    except Exception as exc:
        print("NORMALIZE_PIN_ERROR", pin, exc)
