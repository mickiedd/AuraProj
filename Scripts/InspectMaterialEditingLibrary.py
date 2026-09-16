import unreal

print("MEL_METHODS", [name for name in dir(unreal.MaterialEditingLibrary) if "expression" in name.lower() or "material" in name.lower()])
