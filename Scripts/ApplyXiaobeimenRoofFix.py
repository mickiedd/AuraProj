"""Import the cleaned roof mesh and replace the Production V3 roof component."""
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/GateDetailFix'
OUT.mkdir(exist_ok=True)
import sys
sys.path.insert(0, str(ROOT / 'Scripts'))
import ImportV3Buildings as importer

cfg = json.loads((ROOT / 'Saved/RawModelImport/V3/packages.json').read_text())[1]
report = json.loads((ROOT / 'Saved/RawModelImport/V3/Xiaobeimen_Production_V3-import.json').read_text())
materials = {name: unreal.EditorAssetLibrary.load_asset(path) for name, path in report['materials'].items()}
source = {
    'stem': 'RoofClean',
    'source': str(OUT / 'RoofClean.glb'),
    'primary': True,
    'collision': False,
    'mesh_count': 1,
    'instance_count': 1,
    'relative': 'RoofClean.glb',
    'triangles': 106768 - 856,
}
destination = cfg['destination'] + '/Meshes/RoofClean'
roof_path = destination + '/RoofClean'
if unreal.EditorAssetLibrary.does_asset_exist(roof_path):
    result = {'instances': [{'mesh': roof_path}]}
else:
    assert not unreal.EditorAssetLibrary.does_directory_exist(destination), destination
    result = importer.import_source(cfg, source, materials)
bp_path = cfg['destination'] + '/BP_' + cfg['name']
bp_file = ROOT / ('Content' + bp_path.removeprefix('/Game') + '.uasset')
backup = OUT / 'BP_Xiaobeimen_Production_V3.before-roof-clean.uasset'
if not backup.exists():
    shutil.copy2(bp_file, backup)
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
changes = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
    if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
        continue
    if component.static_mesh.get_name() == 'SM_XiaobeiMen_RoofTile_Green_LOD0':
        changes.append({'component': component.get_name(), 'before': component.static_mesh.get_path_name(), 'after': result['instances'][0]['mesh']})
        component.set_static_mesh(unreal.EditorAssetLibrary.load_asset(result['instances'][0]['mesh']))
assert len(changes) == 1, changes
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
(OUT / 'roof-asset-changes.json').write_text(json.dumps(changes, indent=2), encoding='utf-8')
print('ROOF_FIX_APPLIED', changes)
