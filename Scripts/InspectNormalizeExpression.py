import unreal

mat = unreal.Material()
node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionNormalize, 0, 0)
print("NORMALIZE_NODE", node)
for name in ("get_editor_property_names", "get_editor_property_name", "get_editor_property"):
    print("HAS", name, hasattr(node, name))
try:
    print("PROPS", node.get_editor_property_names())
except Exception as exc:
    print("PROPS_ERROR", exc)
print("DIR", [name for name in dir(node) if "input" in name.lower() or "vector" in name.lower() or "normal" in name.lower()])
