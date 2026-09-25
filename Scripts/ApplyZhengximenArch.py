"""Reimport the curved stone vault without changing the rest of BP_Zhengximen."""
import json
import runpy
from pathlib import Path

import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
pipeline = runpy.run_path(str(project / 'Scripts/ImportZhengximenLandmark.py'))
base = '/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen'
eal = unreal.EditorAssetLibrary
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), \
    'Save the current map before reimporting the arch'

row = pipeline['import_group']('StoneFoundation')
assert row and row['roll'] == -90.0 and row['scale'] == 1.0, row
mesh = eal.load_asset(row['mesh'])
material = eal.load_asset(base + '/Materials/MI_StoneFoundation')
bp = eal.load_asset(base + '/BP_Zhengximen')
assert mesh and material and bp
mesh.set_material(0, material)
assert eal.save_loaded_asset(mesh, only_if_is_dirty=False)

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
bound = 0
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    component = library.get_object(data)
    if isinstance(component, unreal.InstancedStaticMeshComponent) and \
            'StoneFoundation' in str(library.get_display_name(data)):
        component.set_static_mesh(mesh)
        component.set_material(0, material)
        bound += 1
assert bound == 1, bound
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
print('ZHENGXIMEN_ARCH_APPLIED ' + json.dumps({
    'mesh': row, 'bound_components': bound, 'blueprint': bp.get_path_name()},
    default=str))
