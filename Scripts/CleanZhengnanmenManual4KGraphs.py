"""Replace two task-owned interrupted candidate graphs with clean siblings.

No delete/rewrite of existing candidates. Preserve original baseline, bind only
the two corrected material roles on the same target Blueprint, save no map.
"""
import json
from pathlib import Path
import unreal

script=Path(__file__).with_name('ApplyZhengnanmenManual4K.py')
ns={'__file__':str(script),'__name__':'authoring_helpers','MATERIAL_SUFFIX':'_Clean'}
exec(compile(script.read_text(),str(script),'exec'),ns)
out=ns['REPORT'];report=json.loads((out/'apply.json').read_text())
manifest=json.loads(Path(report['source_manifest']).read_text())
bp=unreal.EditorAssetLibrary.load_asset(report['blueprint'])
cs=ns['templates'](bp)
new={};old={}
for kind in ('Stone','Glaze'):
    old[kind]=report['materials'][kind]
    maps={k:unreal.EditorAssetLibrary.load_asset(p) for k,p in report['textures'][kind].items()}
    m=ns['build'](kind,manifest['materials'][kind],maps)
    assert unreal.MaterialEditingLibrary.get_num_material_expressions(m)==12
    new[kind]=m
for c in cs:
    for i in range(c.get_num_materials()):
        p=c.get_material(i).get_path_name()
        for kind,path in old.items():
            if p==path:c.modify();c.set_material(i,new[kind])
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
report['superseded_task_candidates']=old
for kind,m in new.items():report['materials'][kind]=m.get_path_name()
(out/'apply.json').write_text(json.dumps(report,indent=2))
print('ZNM_MANUAL4K_CLEAN_SIBLINGS_BOUND',report['materials'])
