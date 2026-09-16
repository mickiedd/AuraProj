import unreal

print("NORMAL_CLASSES", [name for name in dir(unreal) if "Normal" in name])
print("VECTOR_CLASSES", [name for name in dir(unreal) if name in ("MaterialExpressionAdd", "MaterialExpressionNormalize", "MaterialExpressionAppendVector", "MaterialExpressionComponentMask", "MaterialExpressionMultiply")])
