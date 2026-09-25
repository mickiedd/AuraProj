"""Reimport Zhengximen timber PBR maps and UV-tuned mesh groups in Unreal."""
import json
import runpy
from pathlib import Path

import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
package = (project / 'Raw3DPacket/Zhengximen_GreatWestGate_UE5_Package'
           / 'Zhengximen_GreatWestGate_UE5')
base = '/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen'
pipeline = runpy.run_path(str(project / 'Scripts/ImportZhengximenLandmark.py'))
eal = unreal.EditorAssetLibrary
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), \
    'Save the current map before timber reimport'

groups = ('AgedWood', 'DarkTimber')
tasks = []
for group in groups:
    for kind in ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO', 'Height'):
        stem = 'T_{}_{}'.format(group, kind)
        source = package / 'Textures' / (stem + '.png')
        assert source.is_file(), source
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(source))
        task.set_editor_property('destination_path', base + '/Textures')
        task.set_editor_property('destination_name', stem)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        task.set_editor_property('replace_existing', True)
        tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for task in tasks:
    stem = task.get_editor_property('destination_name')
    texture = eal.load_asset(base + '/Textures/' + stem)
    assert isinstance(texture, unreal.Texture2D), stem
    kind = stem.rsplit('_', 1)[1]
    compression, srgb = pipeline['MAP_KINDS'][kind]
    texture.set_editor_property('compression_settings',
                                getattr(unreal.TextureCompressionSettings, compression))
    texture.set_editor_property('srgb', srgb)
    if kind == 'Normal':
        texture.set_editor_property('flip_green_channel', False)
    assert eal.save_loaded_asset(texture, only_if_is_dirty=False), stem

bp = eal.load_asset(base + '/BP_Zhengximen')
assert bp
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
rows = []
for group in groups:
    row = pipeline['import_group'](group)
    assert row and row['roll'] == -90.0 and row['scale'] == 1.0, row
    mesh = eal.load_asset(row['mesh'])
    material = eal.load_asset(base + '/Materials/MI_' + group)
    assert mesh and material
    mesh.set_material(0, material)
    assert eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    bound = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if isinstance(component, unreal.InstancedStaticMeshComponent) and \
                group in str(library.get_display_name(data)):
            component.set_static_mesh(mesh)
            component.set_material(0, material)
            bound += 1
    assert bound == 1, (group, bound)
    rows.append(row)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
print('ZHENGXIMEN_TIMBER_APPLIED ' + json.dumps({
    'textures': len(tasks), 'meshes': rows, 'blueprint': bp.get_path_name()},
    default=str))
