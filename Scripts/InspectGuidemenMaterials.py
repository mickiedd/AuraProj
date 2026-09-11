"""Read-only inspection of Guidemen material texture references."""
import json

import unreal


DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen"
paths = unreal.EditorAssetLibrary.list_assets(DESTINATION, recursive=False, include_folder=False)
result = {}
for path in paths:
    obj = unreal.load_asset(path)
    if not isinstance(obj, unreal.Material):
        continue
    refs = []
    try:
        expressions = obj.get_editor_property("expressions")
    except Exception as exc:
        result[obj.get_name()] = {"error": str(exc)}
        continue
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionTextureSample):
            texture = expression.get_editor_property("texture")
            refs.append(texture.get_path_name() if texture else None)
    result[obj.get_name()] = {"texture_references": refs}
print("MATERIAL_REFERENCES", json.dumps(result))
