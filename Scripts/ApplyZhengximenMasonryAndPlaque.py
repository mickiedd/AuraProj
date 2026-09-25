"""Apply rebuilt Zhengximen masonry and plaque to the existing Blueprint.

Run inside an open editor after rebuild_surface_textures.py and
build_zhengximen.py. Reimports only the changed 12 texture maps and two
masonry meshes; keeps the other five mesh groups and placement intact.
"""
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
dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not dirty_maps, 'Save open maps before reimport: ' + str(dirty_maps)

tasks = []
for group in ('GrayBrick', 'GatePlaque'):
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

textures = []
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
    textures.append(texture.get_path_name())

bp = eal.load_asset(base + '/BP_Zhengximen')
assert bp
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
rows = []
for group in ('GrayBrick', 'StoneFoundation'):
    row = pipeline['import_group'](group)
    assert row and row['roll'] == -90.0 and row['scale'] == 1.0, row
    mesh = eal.load_asset(row['mesh'])
    material = eal.load_asset(base + '/Materials/MI_' + group)
    assert mesh and material
    mesh.set_material(0, material)
    assert eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    rebound = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if isinstance(component, unreal.InstancedStaticMeshComponent) and \
                group in str(library.get_display_name(data)):
            component.set_static_mesh(mesh)
            component.set_material(0, material)
            rebound += 1
    assert rebound == 1, (group, rebound)
    rows.append(row)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)

print('ZHENGXIMEN_MASONRY_PLAQUE_APPLIED ' + json.dumps({
    'textures': len(textures), 'meshes': rows, 'blueprint': bp.get_path_name(),
    'rebound_components': len(rows)}, default=str))
