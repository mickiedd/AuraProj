"""Import supplied Guangzhou landmarks into UE 5.5; run inside the editor.
Extract the four source ZIPs beneath C:/Works/Raw3DModels/Extracted first.
Existing destination folders are rejected to prevent accidental overwrites.
"""
import json
from pathlib import Path
import unreal
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"

ROOT = Path('C:/Works/Raw3DModels/Extracted')
DEST = '/Game/Assets/Environment/GuangzhouLandmarks'
REPORT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'RawModelImport'
GLBS = {
    'GreatSouthGate': ('Great_South_Gate_Textured.glb', 1.0),
    'GreatNorthGate': ('GreatNorthGate_Guangzhou_MegaDetail_Textured.glb', 1.0),
    'ZhenhaiTower': ('Zhenhai_Tower_UE5_HighDetail_Textured_PBR.glb', 0.01),
}

def import_glb(name):
    filename, scale = GLBS[name]
    matches = list(ROOT.rglob(filename))
    assert len(matches) == 1, matches
    destination = DEST + '/' + name
    assert not unreal.EditorAssetLibrary.does_directory_exist(destination), destination
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = 'SM_' + name
    pipeline.import_offset_rotation = unreal.Rotator(roll=-90)
    pipeline.import_offset_uniform_scale = scale
    pipeline.common_meshes_properties.bake_meshes = True
    pipeline.mesh_pipeline.combine_static_meshes = True
    pipeline.mesh_pipeline.build_nanite = True
    pipeline.mesh_pipeline.set_editor_property('collision', False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = False
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(destination, manager.create_source_data(str(matches[0])), params)
    assert objects, 'Import returned no objects'
    assert unreal.EditorAssetLibrary.save_directory(destination), 'Save failed'
    meshes = [o for o in objects if isinstance(o, unreal.StaticMesh)]
    assert len(meshes) == 1, [o.get_path_name() for o in meshes]
    mesh = meshes[0]
    assert all(s.material_interface for s in mesh.static_materials), 'Unassigned material'
    bounds = mesh.get_bounds()
    result = {'name': name, 'source': str(matches[0]), 'scale_offset': scale, 'roll_offset': -90,
              'mesh': mesh.get_path_name(), 'nanite': mesh.get_editor_property('nanite_settings').enabled,
              'size_cm': [bounds.box_extent.x*2,bounds.box_extent.y*2,bounds.box_extent.z*2],
              'origin_cm': [bounds.origin.x,bounds.origin.y,bounds.origin.z],
              'materials': [s.material_interface.get_path_name() for s in mesh.static_materials],
              'assets': [o.get_path_name() for o in objects]}
    REPORT.mkdir(parents=True, exist_ok=True)
    (REPORT/(name+'.json')).write_text(json.dumps(result,indent=2))
    print('IMPORT_VALIDATED',json.dumps(result))

if __name__ == '__main__':
    for name in globals().get('MODEL_NAMES', list(GLBS)):
        import_glb(name)

