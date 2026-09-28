"""Reimport the reference repair in-place and refresh BP_Zhengdongmen.

Reimports only the parts whose source actually changed, plus the texture maps that were
re-authored or are new. `import_textures` skips a texture that already exists, so the new
plaque board and the whole Ridge set would otherwise never reach the engine.

A group with no component in the Blueprint yet (Ridge is new) gets its HISM added **in
place**. Rebuilding the Blueprint through `CreateZhengdongmenLandmarkBlueprint.py` would
force-delete and recreate the asset, and `L_GuangzhouLandmarkShowcase` holds a reference
to its generated class.

Nanite is re-asserted on every reimported mesh: a reimport resets `fallback_target` to
AUTO, which decimates the fallback on Metal and renders the walls and roof as black
triangular holes.
"""
import json
import runpy
import subprocess
from pathlib import Path
import unreal

# Refuse to run while a GUI editor holds the project. This script is launched as
# UnrealEditor-Cmd, so it cannot detect itself; matching on the .app bundle path finds
# only a real editor. Learned the hard way: one run of this script went ahead with an
# editor open, and while the disk assets came out correct the editor kept stale copies
# of the meshes and the Blueprint until it was closed.
_running = subprocess.run(['pgrep', '-f', r'UnrealEditor\.app'],
                          capture_output=True, text=True).stdout.split()
assert not _running, (
    'a GUI UnrealEditor is running (pid {}); close it before reimporting, or the '
    'editor will hold stale assets and can save them back over the fixed ones'.format(
        ', '.join(_running)))

# RoofTile carries the rebuilt continuous tile courses; Ridge is the new group the roof
# ridges moved onto; Sign carries the re-authored plaque board.
GROUPS = ('RoofTile', 'Ridge', 'Sign')
# Sign's maps were regenerated at the board's own aspect ratio, and Ridge's are new
# assets with no counterpart in the supplied package.
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


def components():
    """Every subobject of the Blueprint, as (handle, data, object) triples."""
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        yield handle, data, library.get_object(data)


def root_handle():
    for handle, data, _obj in components():
        if library.is_root_component(data):
            return handle
    raise AssertionError('no root component on ' + bp.get_path_name())


def add_hism(group, mesh, material):
    """Add a HISM_<group> component to the existing Blueprint, in place."""
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property('parent_handle', root_handle())
    params.set_editor_property('new_class', unreal.HierarchicalInstancedStaticMeshComponent)
    params.set_editor_property('blueprint_context', bp)
    params.set_editor_property('conform_transform_to_parent', False)
    handle, fail_reason = subsystem.add_new_subobject(params)
    assert library.is_handle_valid(handle), fail_reason
    subsystem.rename_subobject(handle, unreal.Text('HISM_' + group))
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    template = library.get_object(data)
    assert template, group
    template.set_static_mesh(mesh)
    template.set_material(0, material)
    # Each part is authored in the building's own space: one instance at identity.
    template.add_instances([unreal.Transform()], False, True, False)
    return template


textures = pipeline['refresh_textures'](
    ['T_ZDM_{}_{}'.format(group, kind)
     for group in REFRESH_TEXTURE_GROUPS for kind in TEXTURE_KINDS])

# Material instances are built by the import's own main(), which this script does not
# run, so Ridge had no MI_Ridge and the mesh could not be bound. build_instances is
# idempotent: it creates a missing instance and re-binds every group's textures, which
# also picks up the re-authored Sign maps.
master = eal.load_asset(base + '/Materials/M_ZDM_Master')
assert master, 'master material missing at ' + base + '/Materials/M_ZDM_Master'
pipeline['build_instances'](master)

rows = []
added = []
for group in GROUPS:
    row = pipeline['import_group'](group)
    assert row and row['roll'] == -90.0 and row['scale'] == 1.0, row
    mesh = eal.load_asset(row['mesh'])
    # import_group sets Nanite, but a reimport is what resets it, so re-assert here too
    # and record it rather than trusting the import path.
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
    for _handle, _data, component in components():
        if (isinstance(component, unreal.InstancedStaticMeshComponent)
                and component.static_mesh
                and component.static_mesh.get_path_name() == mesh.get_path_name()):
            component.set_static_mesh(mesh)
            component.set_material(0, material)
            bound += 1
    if bound == 0:
        add_hism(group, mesh, material)
        added.append(group)
        bound = 1
    assert bound == 1, (group, bound)
    rows.append(row)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
result = {'passed': True, 'meshes': rows, 'textures': textures, 'components_added': added,
          'blueprint': bp.get_path_name(),
          'total_triangles': sum(row['triangles'] for row in rows)}
(project / 'Saved/Reports/Zhengdongmen/unreal-apply.json').write_text(
    json.dumps(result, indent=2))
print('ZHENGDONGMEN_REFERENCE_APPLIED ' + json.dumps(result))
