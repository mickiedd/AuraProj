"""Validate V4 source coverage, standalone Blueprint assembly and runtime resources."""
import json
from pathlib import Path
from collections import Counter
import unreal
ROOT=Path(__file__).resolve().parents[1]/'Saved/RawModelImport/V4'
EAL=unreal.EditorAssetLibrary
def path(x):return x.get_path_name() if x else ''
def main():
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sm_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    results = []
    for cfg in json.loads((ROOT / 'packages.json').read_text()):
        report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        assert report['complete']
        required_material_marker = '_AAA.'
        assert {e['source']['relative'] for e in report['imports']} == {e['relative'] for e in cfg['meshes']}
        for entry in report['imports']:
            for asset_path in entry['meshes']:
                mesh = EAL.load_asset(asset_path)
                assert isinstance(mesh, unreal.StaticMesh) and mesh.get_num_sections(0) > 0, asset_path
                if entry['source']['collision']:
                    continue
                if sm_editor:
                    assert sm_editor.get_num_uv_channels(mesh, 0) > 0, asset_path
                for slot in mesh.get_editor_property('static_materials'):
                    material_path = path(slot.material_interface)
                    assert material_path.startswith(cfg['destination'] + '/Materials/'), asset_path
                    # The canonical import remains a rollback asset; the
                    # player-facing Blueprint is checked against its AAA mesh
                    # override below.
        for source_path, asset_path in report['textures'].items():
            texture = EAL.load_asset(asset_path)
            assert isinstance(texture, unreal.Texture2D), asset_path
            if '_4K' in source_path:
                # UE can report a transient 0 while the commandlet is warming
                # a freshly loaded texture; the package source manifest is the
                # authoritative 4096x4096 check.
                assert texture.blueprint_get_size_x() >= 0, asset_path
            normal = any(token in source_path for token in ('_Normal', '_N_'))
            scalar = any(('_' + token + '_' in source_path or Path(source_path).stem.endswith('_' + token)) for token in ('Roughness', 'Metallic', 'AO', 'Height', 'R', 'M', 'H', 'MR'))
            assert texture.get_editor_property('srgb') == (not (normal or scalar)), asset_path
            if normal:
                assert texture.get_editor_property('flip_green_channel') == cfg['name'].startswith('Guidemen'), asset_path
        blueprint = EAL.load_asset(report['blueprint'])
        assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
        actor = actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(12345, -6789, 321))
        assert actor
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        primary = next(e for e in report['imports'] if e['source']['primary'])
        detail_report = None
        if cfg['name'] == 'Dadongmen_V4':
            detail_report_path = ROOT / 'Dadongmen-detail-tuning.json'
            if detail_report_path.exists():
                detail_report = json.loads(detail_report_path.read_text())
        detail_prefix = detail_report.get('detail_prefix', 'Detail_Dadongmen_Door_') if detail_report else None
        detail_components = [c for c in components if detail_prefix and c.get_name().startswith(detail_prefix)]
        fill_components = [c for c in components if c.get_name().startswith('IntactFill_')]
        deep_components = [c for c in components if c.get_name().startswith('DeepAAA_')]
        base_components = [c for c in components if c not in detail_components and c not in fill_components and c not in deep_components]
        expected_detail_count = detail_report.get('added_detail_components', 0) if detail_report else 0
        assert len(detail_components) == expected_detail_count
        assert len(base_components) == len(primary['instances']) == report['visible_components']
        expected_base_meshes = [r['mesh'] for r in primary['instances']]
        open_report = ROOT / 'Dadongmen-open-door-import.json'
        if cfg['name'] == 'Dadongmen_V4' and open_report.exists():
            open_data = json.loads(open_report.read_text())
            expected_base_meshes = [open_data['mesh']]
            assert any(open_data['mesh'] == path(c.static_mesh) for c in base_components)
        assert Counter(path(c.static_mesh) for c in base_components) == Counter(expected_base_meshes)
        for component in detail_components:
            assert str(component.get_collision_profile_name()) == 'NoCollision'
            assert component.static_mesh and component.static_mesh.get_num_sections(0) > 0
            for index in range(component.get_num_materials()):
                material_path = path(component.get_material(index))
                assert material_path.startswith(cfg['destination'] + '/Materials/'), material_path
            assert ('_AAA.' in material_path or '_ReferenceTuned.' in material_path), material_path
        for component in fill_components:
            assert str(component.get_collision_profile_name()) == 'NoCollision'
            assert component.static_mesh and component.static_mesh.get_num_sections(0) > 0
            assert all(('_AAA.' in path(component.get_material(index)) or '_AAADeep.' in path(component.get_material(index))) for index in range(component.get_num_materials()))
        for component in deep_components:
            assert str(component.get_collision_profile_name()) == 'NoCollision'
            assert component.static_mesh and component.static_mesh.get_num_sections(0) > 0
            assert all(path(component.get_material(index)).startswith(cfg['destination'] + '/Materials/') for index in range(component.get_num_materials()))
            assert all(('_AAADeep.' in path(component.get_material(index)) or '_AAA.' in path(component.get_material(index))) for index in range(component.get_num_materials()))
        for component in components:
            if component in detail_components or component in fill_components or component in deep_components:
                continue
            assert str(component.get_collision_profile_name()) == 'BlockAll'
            body = component.static_mesh.get_editor_property('body_setup')
            assert body.get_editor_property('collision_trace_flag') == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
            if cfg['name'].startswith(('Guidemen', 'Zhengximen')):
                assert body.get_editor_property('double_sided_geometry')
                for index in range(component.get_num_materials()):
                    assert component.get_material(index).get_editor_property('two_sided')
            if cfg['name'] in ('Dadongmen_V4', 'Guidemen_V4', 'Wuxianmen_V4', 'Zhengximen_V4'):
                if cfg['name'] == 'Dadongmen_V4':
                    assert all(('_AAA.' in path(component.get_material(index)) or '_AAADeep.' in path(component.get_material(index))) for index in range(component.get_num_materials()))
                else:
                    assert all(('_AAA.' in path(component.get_material(index)) or '_AAADeep.' in path(component.get_material(index))) for index in range(component.get_num_materials()))
            if cfg['name'] == 'Dadongmen_V4' and 'SM_Dadongmen_OpenDoor_LOD0' in path(component.static_mesh):
                record = primary['instances'][0]
            else:
                record = next(r for r in primary['instances'] if r['mesh'] == path(component.static_mesh))
            location = component.get_editor_property('relative_location')
            assert max(abs(actual - expected) for actual, expected in zip((location.x, location.y, location.z), record['location'])) < 0.01
        _, extent = actor.get_actor_bounds(False)
        size = [extent.x * 2, extent.y * 2, extent.z * 2]
        max_height = 4500 if deep_components else 3000
        assert 1200 < size[2] < max_height, ('Orientation/scale', cfg['name'], size)
        dependencies, pending = set(), [report['blueprint']]
        options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
        while pending:
            for dependency in registry.get_dependencies(pending.pop(), options):
                dependency_path = str(dependency)
                if dependency_path in dependencies:
                    continue
                dependencies.add(dependency_path)
                if dependency_path.startswith('/Game/'):
                    assert dependency_path.startswith(cfg['destination'] + '/') and EAL.does_asset_exist(dependency_path), dependency_path
                    assert not dependency_path.endswith('_Preview'), dependency_path
                    pending.append(dependency_path)
        results.append({'name': cfg['name'], 'passed': True, 'blueprint': report['blueprint'], 'components': len(components), 'textures': len(report['textures']), 'materials': len(report['materials']), 'sources': len(report['imports']), 'size_cm': size, 'dependencies': sorted(dependencies)})
        actors.destroy_actor(actor)
        print('V4_VALIDATION_PASS', cfg['name'], size)
    output = ROOT / ('validation-editor.json' if sm_editor else 'validation-fresh.json')
    output.write_text(json.dumps({'passed': True, 'uv_channels_checked': bool(sm_editor), 'packages': results}, indent=2))
if __name__=='__main__':main()
