"""Bind a flat grey diagnostic material to every Guidemen slot, capture, then restore."""
import json
from pathlib import Path
import unreal

PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K"
BLUEPRINT = PACKAGE + "/BP_Guidemen_V5_4K"
MATERIALS = PACKAGE + "/Materials"
ROOT = Path("C:/Git/AuraProj/Saved/RawModelImport/V5/GuidemenRebuild")
REPORT = ROOT / "material-rebuild-20260918.json"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

name = "M_Diag_FlatGrey"
target = MATERIALS + "/" + name
if EAL.does_asset_exist(target):
    EAL.delete_asset(target)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
node = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 0)
node.set_editor_property("constant", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
assert MEL.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 300)
rough.set_editor_property("r", 0.6)
assert MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(mat)
EAL.save_loaded_asset(mat)

bp = EAL.load_asset(BLUEPRINT)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
seen, comps = set(), []
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    c = lib.get_object(sub.k2_find_subobject_data_from_handle(h))
    if not isinstance(c, unreal.HierarchicalInstancedStaticMeshComponent):
        continue
    if c.get_path_name() in seen: continue
    seen.add(c.get_path_name()); comps.append(c)

action = json.loads((ROOT / "flatgrey.json").read_text())
record = []
if action["action"] == "apply":
    for c in comps:
        for i in range(c.get_num_materials()):
            if c.get_material(i):
                record.append({"c": c.get_name(), "i": i, "m": c.get_material(i).get_path_name()})
                c.set_material(i, mat)
    (ROOT / "flatgrey-bindings.json").write_text(json.dumps(record, indent=1))
    print("FLATGREY_APPLIED", len(record))
else:
    record = json.loads((ROOT / "flatgrey-bindings.json").read_text())
    for item in record:
        for c in comps:
            if c.get_name() == item["c"]:
                c.set_material(item["i"], EAL.load_asset(item["m"]))
                break
    print("FLATGREY_RESTORED", len(record))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
EAL.save_loaded_asset(bp, only_if_is_dirty=False)
print("FLATGREY_DONE")
