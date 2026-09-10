"""Read-only post-import validation, executed in the Unreal Editor."""
import json
from pathlib import Path
import unreal
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"
DEST='/Game/Assets/Environment/GuangzhouLandmarks'
EXPECTED={'GreatSouthGate':(3260,1545,2486,8),'GreatNorthGate':(3600,1750,2476.025,8),'ZhenhaiTower':(3466.596,2834.554,2085,8),'Xiaobeimen':(3482,3731,2532,12)}
report={'meshes':[], 'errors':[], 'assets':[]}
for name,(x,y,z,slots) in EXPECTED.items():
    path=DEST+'/'+name+'/SM_'+name
    mesh=unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(mesh,unreal.StaticMesh):report['errors'].append('Missing '+path);continue
    b=mesh.get_bounds();size=[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2]
    nanite=mesh.get_editor_property('nanite_settings').enabled
    mats=[s.material_interface for s in mesh.static_materials]
    entry={'path':mesh.get_path_name(),'size_cm':size,'bottom_z_cm':b.origin.z-b.box_extent.z,'nanite':nanite,
           'material_slots':len(mats),'materials':[m.get_path_name() if m else None for m in mats]}
    report['meshes'].append(entry)
    if not nanite:report['errors'].append(name+' Nanite disabled')
    if len(mats)!=slots or not all(mats):report['errors'].append(name+' material slot mismatch')
    if any(abs(actual-expected)>2 for actual,expected in zip(size,(x,y,z))):report['errors'].append(name+' width/height mismatch')
    if abs(entry['bottom_z_cm'])>1:report['errors'].append(name+' not grounded at local Z=0')
for path in unreal.EditorAssetLibrary.list_assets(DEST,recursive=True,include_folder=False):
    asset=unreal.EditorAssetLibrary.load_asset(path)
    item={'path':path,'class':asset.get_class().get_name()}
    if isinstance(asset,unreal.Texture2D):
        item.update(srgb=asset.get_editor_property('srgb'),compression=str(asset.get_editor_property('compression_settings')),size=[asset.blueprint_get_size_x(),asset.blueprint_get_size_y()])
        if min(item['size']) <= 0:report['errors'].append('Empty texture '+path)
    if isinstance(asset,unreal.Material):
        textures=unreal.MaterialEditingLibrary.get_used_textures(asset)
        item['used_textures']=[t.get_path_name() for t in textures]
        for prop in ([unreal.MaterialProperty.MP_BASE_COLOR] if asset.get_name()=='Black_Sign' else [unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_NORMAL,unreal.MaterialProperty.MP_ROUGHNESS]):
            node=unreal.MaterialEditingLibrary.get_material_property_input_node(asset,prop)
            if node is None:report['errors'].append('Disconnected material property '+path+' '+str(prop))
            elif isinstance(node,unreal.MaterialExpressionTextureSample) and node.texture is None:report['errors'].append('Invalid texture node '+path)
        if not textures and asset.get_name()!='Black_Sign':report['errors'].append('Material has no textures '+path)
    report['assets'].append(item)
report['passed']=not report['errors'] and len(report['meshes'])==4
Path('C:/Git/AuraProj/Saved/RawModelImport/validation.json').write_text(json.dumps(report,indent=2))
print('VALIDATION',json.dumps({'passed':report['passed'],'errors':report['errors'],'meshes':report['meshes'],'asset_count':len(report['assets'])}))
assert report['passed'],report['errors']
