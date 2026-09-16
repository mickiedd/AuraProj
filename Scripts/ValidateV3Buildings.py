"""Reload and spawn V3 building Blueprints without loading their preview maps.

Suitable for a fresh UnrealEditor-Cmd -run=pythonscript process with -NullRHI.
Checks source coverage, component transforms, collision, PBR references, and
package-local runtime dependency closure. Does not save or modify assets.
"""
from collections import Counter
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / 'Saved/RawModelImport/V3'
EAL = unreal.EditorAssetLibrary
AUTHORED_OVERRIDE_PREFIXES = (
    '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/RoofClean/',
    '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/GateDoorsAligned/',
)


def p(obj):
    return obj.get_path_name() if obj else ''


def xyz(v):
    return [v.x, v.y, v.z]


def main():
    cfgs = json.loads((ROOT / 'packages.json').read_text())
    results = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    # Validation world has no imported source actors, scene assets or preview maps.
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for cfg in cfgs:
        report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        assert report['complete'], cfg['name']
        assert {e['source']['relative'] for e in report['imports']} == {s['relative'] for s in cfg['meshes']}
        mesh_assets = []
        for entry in report['imports']:
            assert len(entry['meshes']) == entry['source']['mesh_count']
            for asset_path in entry['meshes']:
                mesh = EAL.load_asset(asset_path)
                assert isinstance(mesh, unreal.StaticMesh), asset_path
                assert mesh.get_num_sections(0) > 0, asset_path
                if not entry['source']['collision']:
                    for slot in mesh.get_editor_property('static_materials'):
                        assert p(slot.material_interface).startswith(cfg['destination'] + '/Materials/'), (asset_path, p(slot.material_interface))
                mesh_assets.append(asset_path)
        textures = []
        for source, asset_path in report['textures'].items():
            tex = EAL.load_asset(asset_path)
            assert isinstance(tex, unreal.Texture2D), asset_path
            assert tex.blueprint_get_size_x() > 0 and tex.blueprint_get_size_y() > 0, asset_path
            if '_4K' in source:
                assert tex.blueprint_get_size_x() == 4096 and tex.blueprint_get_size_y() == 4096, asset_path
            if any('_' + c + '_' in source for c in ('Normal', 'ORM', 'AO', 'Metallic', 'Roughness', 'Height')):
                assert not tex.get_editor_property('srgb'), asset_path
            textures.append(asset_path)
        bp = EAL.load_asset(report['blueprint'])
        assert isinstance(bp, unreal.Blueprint) and bp.generated_class()
        # Translate the actor to demonstrate that all parts travel together.
        offset = unreal.Vector(12345, -6789, 321)
        actor = actor_subsystem.spawn_actor_from_class(bp.generated_class(), offset)
        assert actor and not isinstance(actor, unreal.LevelInstance), report['blueprint']
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        visible = [c for c in components if c.get_editor_property('visible')]
        hidden = [c for c in components if not c.get_editor_property('visible')]
        primary = next(e for e in report['imports'] if e['source']['primary'])
        collision = next(e for e in report['imports'] if e['source']['collision'] and 'Simple' in e['source']['relative'])
        source_visible_count = primary['source']['instance_count']
        authored_visible_count = report['visible_components']
        assert authored_visible_count >= source_visible_count
        assert len(visible) == authored_visible_count
        assert len(hidden) == collision['source']['instance_count'] == report['collision_components']
        source_visible = [c for c in visible if not c.get_name().startswith('ReferenceInfill_')]
        authored_infill = [c for c in visible if c.get_name().startswith('ReferenceInfill_')]
        authored_overrides = [
            c for c in visible
            if any(p(c.static_mesh).startswith(prefix) for prefix in AUTHORED_OVERRIDE_PREFIXES)
        ]
        assert len(source_visible) == source_visible_count
        assert len(authored_infill) == authored_visible_count - source_visible_count
        if not authored_overrides:
            assert Counter(p(c.static_mesh) for c in source_visible) == Counter(r['mesh'] for r in primary['instances'])
        assert Counter(p(c.static_mesh) for c in hidden) == Counter(r['mesh'] for r in collision['instances'])
        for component in components:
            assert component.static_mesh
            if component in authored_infill or component in authored_overrides:
                assert str(component.get_collision_profile_name()) == 'NoCollision'
                for i in range(component.get_num_materials()):
                    assert p(component.get_material(i)).startswith(cfg['destination'] + '/Materials/')
                continue
            record = next(r for e in (primary, collision) for r in e['instances'] if r['mesh'] == p(component.static_mesh))
            assert max(abs(a-b) for a,b in zip(xyz(component.get_editor_property('relative_location')), record['location'])) < .01
            assert max(abs(a-b) for a,b in zip(xyz(component.get_editor_property('relative_scale3d')), record['scale'])) < .0001
            rotation = component.get_editor_property('relative_rotation')
            expected = unreal.Rotator(pitch=record['rotation'][0], yaw=record['rotation'][1], roll=record['rotation'][2])
            q, expected_q = rotation.quaternion(), expected.quaternion()
            assert abs(sum(getattr(q, axis) * getattr(expected_q, axis) for axis in ('x', 'y', 'z', 'w'))) > .99999
            if component in hidden:
                assert component.get_editor_property('hidden_in_game')
                assert str(component.get_collision_profile_name()) == 'BlockAll'
                body = component.static_mesh.get_editor_property('body_setup')
                assert body and len(body.get_editor_property('agg_geom').get_editor_property('box_elems')) > 0
            else:
                assert str(component.get_collision_profile_name()) == 'NoCollision'
                for i in range(component.get_num_materials()):
                    assert p(component.get_material(i)).startswith(cfg['destination'] + '/Materials/')
        origin, extent = actor.get_actor_bounds(False)
        bounds = {'center': xyz(origin), 'extent': xyz(extent), 'size_cm': [v*2 for v in xyz(extent)]}
        assert all(v > 500 for v in bounds['size_cm']), bounds
        assert extent.z > 800 and extent.z < 2000, 'Wrong up axis or unit scale: ' + str(bounds)
        deps, pending = set(), [report['blueprint']]
        options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
            include_hard_package_references=True, include_searchable_names=False,
            include_soft_management_references=False, include_hard_management_references=False)
        while pending:
            item = pending.pop()
            for dependency in registry.get_dependencies(item, options):
                dep = str(dependency)
                if dep in deps:
                    continue
                deps.add(dep)
                if dep.startswith('/Game/'):
                    assert dep.startswith(cfg['destination'] + '/'), ('External building dependency', dep)
                    assert EAL.does_asset_exist(dep), dep
                    assert not dep.endswith('_Preview'), ('Preview map dependency', dep)
                    pending.append(dep)
        results.append({'name': cfg['name'], 'passed': True, 'blueprint': report['blueprint'],
                        'mesh_sources': len(report['imports']), 'mesh_assets': len(mesh_assets),
                        'textures': len(textures), 'materials': len(report['materials']),
                        'visible_components': len(visible), 'source_visible_components': source_visible_count,
                        'authored_infill_components': len(authored_infill), 'authored_override_components': len(authored_overrides),
                        'collision_components': len(hidden),
                        'source_primary_triangles': primary['source']['triangles'],
                        'bounds': bounds, 'dependencies': sorted(deps)})
        actor_subsystem.destroy_actor(actor)
        print('V3_VALIDATION_PASSED', cfg['name'])
    output = ROOT / 'validation.json'
    output.write_text(json.dumps({'passed': True, 'fresh_world': True, 'packages': results}, indent=2))
    print('V3_ALL_VALIDATION_PASSED', str(output))


if __name__ == '__main__':
    main()
