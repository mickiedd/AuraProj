"""Explicit rollback: restore exact V2 template materials, retaining new assets."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[1]/'Saved/RawModelImport/ZhengnanmenManual4K'
before=json.loads((root/'before.json').read_text());applied=json.loads((root/'apply.json').read_text())
bp=unreal.EditorAssetLibrary.load_asset(before['blueprint']);assert bp
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
expected={r['name']:r for r in before['components']};seen=set();targets=[]
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
    d=ss.k2_find_subobject_data_from_handle(h)
    if not lib.is_component(d):continue
    c=lib.get_object(d)
    if not isinstance(c,unreal.StaticMeshComponent) or not c.static_mesh or c.get_path_name() in seen:continue
    seen.add(c.get_path_name());r=expected[c.get_name()]
    assert all(c.get_material(i).get_path_name() in applied['materials'].values() for i in range(c.get_num_materials())), 'Actor changed since manual4K; inspect rather than clobber later work'
    old=[unreal.EditorAssetLibrary.load_asset(p) for p in r['materials']];assert all(old)
    targets.append((c,old))
assert len(targets)==115
for c,old in targets:
    c.modify()
    for i,m in enumerate(old):c.set_material(i,m)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
print('ZNM_MANUAL4K_ROLLED_BACK; new maps/materials retained, no maps saved.')
