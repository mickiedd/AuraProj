"""Reimport the reference repair in-place and refresh BP_Zhengdongmen.

Reimports only the parts whose source actually changed, plus the texture maps that
were re-authored. `import_textures` skips a texture that already exists, so the new
plaque board would otherwise never reach the engine.

Nanite is re-asserted on every reimported mesh: a reimport resets
`fallback_target` to AUTO, which decimates the fallback on Metal and renders the
walls and roof as black triangular holes.
"""
import json
import runpy
from pathlib import Path
import unreal

# RoofTile carries the rebuilt continuous tile courses; Ridge carries the roof
# ridges, which now have their own material; Sign carries the re-authored plaque.
GROUPS = ('RoofTile', 'Ridge', 'Sign')
# Sign's maps were regenerated at the board's own aspect ratio, and Ridge's are new
# assets with no counterpart in the supplied package. `import_textures` only imports a
# texture that does not exist yet, so both need the explicit refresh path.
REFRESH_TEXTURE_GROUPS = ('Sign', 'Ridge')
TEXTURE_KINDS = ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO', 'Height')

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
pipeline = runpy.run_path(str(project / 'Scripts/ImportZhengdongmenLandmark.py'))
base = pipeline['DEST']
eal = unreal.EditorAssetLibrary
bp = eal.load_asset(base + '/BP_Zhengdongmen')
assert bp
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary

textures = pipeline['refresh_textures'](
    ['T_ZDM_{}_{}'.format(group, kind)
     for group in REFRESH_TEXTURE_GROUPS for kind in TEXTURE_KINDS])

rows = []
for group in GROUPS:
    row = pipeline['import_group'](group)
    assert row and row['roll'] == -90.0 and row['scale'] == 1.0, row
    mesh = eal.load_asset(row['mesh'])
    # import_group sets Nanite, but a reimport is what resets it, so re-assert here
    # too and record it rather than trusting the import path.
    settings = mesh.get_editor_property('nanite_settings')
    assert settings.get_editor_property('enabled')
    assert 'PERCENT_TRIANGLES' in str(settings.get_editor_property('fallback_target'))
    assert float(settings.get_editor_property('fallback_percent_triangles')) == 1.0
    assert float(settings.get_editor_property('fallback_relative_error')) == 0.0
    row['nanite_fallback'] = 'PERCENT_TRIANGLES 1.0'
    material = eal.load_asset(base + '/Materials/MI_' + group)
    mesh.set_material(0, material)
    assert eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    bound = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if (isinstance(component, unreal.InstancedStaticMeshComponent)
                and component.static_mesh
                and component.static_mesh.get_path_name() == mesh.get_path_name()):
            component.set_static_mesh(mesh)
            component.set_material(0, material)
            bound += 1
    assert bound == 1, (group, bound)
    rows.append(row)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
result = {'passed': True, 'meshes': rows, 'textures': textures,
          'blueprint': bp.get_path_name(),
          'total_triangles': sum(row['triangles'] for row in rows)}
(project / 'Saved/Reports/Zhengdongmen/unreal-apply.json').write_text(
    json.dumps(result, indent=2))
print('ZHENGDONGMEN_REFERENCE_APPLIED ' + json.dumps(result))
