"""Polish the three V3 building packages after import.

The pass is intentionally asset-local and reversible. It keeps source geometry
and actor transforms intact while improving Nanite coverage, lightmap UVs,
lighting/shadow defaults, collision query behavior, and per-package material
configuration.
"""
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / 'Saved/RawModelImport/V3'
EAL = unreal.EditorAssetLibrary


def path(obj):
    return obj.get_path_name() if obj else ''


def tune_mesh(mesh, primary, collision, report):
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    # Preserve the supplied geometry while enabling the intended render path.
    desired_nanite = not collision
    nanite = editor.get_nanite_settings(mesh) if editor else None
    if nanite is None:
        try:
            nanite = mesh.get_editor_property('nanite_settings')
        except Exception:
            nanite = None
    if nanite is not None and nanite.enabled != desired_nanite:
        nanite.enabled = desired_nanite
        if editor:
            editor.set_nanite_settings(mesh, nanite, True)
        else:
            mesh.set_editor_property('nanite_settings', nanite)
        report['nanite_changed'] += 1
    if not collision:
        # The source UV0 remains the authored texture UV. UE builds UV1 for
        # baked-lighting users without changing UV0 or material coordinates.
        already_generated = False
        try:
            already_generated = mesh.get_editor_property('light_map_coordinate_index') == 1
        except Exception:
            pass
        generated = False
        if not already_generated:
            if editor:
                generated = editor.set_generate_lightmap_uv(mesh, True)
            else:
                try:
                    build = mesh.get_editor_property('build_settings')
                    build.generate_lightmap_u_vs = True
                    build.dst_lightmap_index = 1
                    mesh.set_editor_property('build_settings', build)
                    generated = True
                except Exception:
                    generated = False
        if generated:
            report['lightmap_uv_enabled'] += 1
        triangles = mesh.get_num_triangles(0)
        resolution = 1024 if primary or triangles > 100000 else 512 if triangles > 10000 else 256 if triangles > 1000 else 128
        if mesh.get_editor_property('light_map_resolution') != resolution:
            mesh.set_editor_property('light_map_resolution', resolution)
            report['lightmap_resolution_changed'] += 1
        try:
            mesh.set_editor_property('light_map_coordinate_index', 1)
        except Exception:
            # Some UE point releases expose this as a build-only setting.
            report['lightmap_coordinate_index_unavailable'] += 1
    body = mesh.get_editor_property('body_setup')
    if collision and body:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    assert EAL.save_loaded_asset(mesh, only_if_is_dirty=False), path(mesh)


def tune_material(material, report):
    # Explicitly set stable opaque/PBR defaults. Texture channel semantics were
    # assigned during import and are not replaced by this pass.
    try:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    except Exception:
        pass
    desired_two_sided = any(token in material.get_name() for token in ('M_Fabric', 'M_Flag', 'M_Vegetation', 'M_Moss', 'M_Plaque'))
    if material.get_editor_property('two_sided') != desired_two_sided:
        material.set_editor_property('two_sided', desired_two_sided)
        report['material_two_sided_changed'] += 1
    material.set_editor_property('use_material_attributes', False)
    material.set_editor_property('fully_rough', False)
    material.set_editor_property('dithered_lod_transition', False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert EAL.save_loaded_asset(material, only_if_is_dirty=False), path(material)


def tune_blueprint(cfg, data, report):
    bp = EAL.load_asset(data['blueprint'])
    assert bp and bp.generated_class(), data['blueprint']
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    records = {i['mesh']: i for e in data['imports'] for i in e['instances']}
    visible, collision = 0, 0
    seen = set()
    for handle in ss.k2_gather_subobject_data_for_blueprint(bp):
        component = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        component_path = component.get_path_name()
        if component_path in seen:
            continue
        seen.add(component_path)
        mesh_path = path(component.static_mesh)
        record = records.get(mesh_path)
        if not record:
            continue
        is_collision = component.get_name().startswith('Collision_') or component.get_editor_property('hidden_in_game')
        component.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
        component.set_editor_property('generate_overlap_events', False)
        if is_collision:
            component.set_editor_property('visible', False)
            component.set_editor_property('hidden_in_game', True)
            component.set_editor_property('cast_shadow', False)
            component.set_collision_profile_name('BlockAll')
            component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            collision += 1
        else:
            component.set_editor_property('visible', True)
            component.set_editor_property('hidden_in_game', False)
            component.set_editor_property('cast_shadow', True)
            component.set_collision_profile_name('NoCollision')
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            visible += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EAL.save_loaded_asset(bp, only_if_is_dirty=False), data['blueprint']
    assert visible == data['visible_components'] and collision == data['collision_components'], (visible, collision)
    return visible, collision


def main():
    configs = json.loads((ROOT / 'packages.json').read_text())
    result = {'passed': False, 'packages': [], 'scope': 'V3 only'}
    for cfg in configs:
        data = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        assert data['complete'], cfg['name']
        report = {'name': cfg['name'], 'blueprint': data['blueprint'], 'mesh_assets': 0,
                  'nanite_changed': 0, 'lightmap_uv_enabled': 0, 'lightmap_resolution_changed': 0,
                  'lightmap_coordinate_index_unavailable': 0, 'material_two_sided_changed': 0}
        primary = next(e for e in data['imports'] if e['source']['primary'])
        for entry in data['imports']:
            for asset_path in entry['meshes']:
                mesh = EAL.load_asset(asset_path)
                assert isinstance(mesh, unreal.StaticMesh), asset_path
                tune_mesh(mesh, entry is primary, entry['source']['collision'], report)
                report['mesh_assets'] += 1
        for asset_path in data['materials'].values():
            mat = EAL.load_asset(asset_path)
            assert isinstance(mat, unreal.Material), asset_path
            tune_material(mat, report)
        report['visible_components'], report['collision_components'] = tune_blueprint(cfg, data, report)
        report['passed'] = True
        result['packages'].append(report)
        print('V3_FINE_TUNED', cfg['name'], json.dumps(report))
    result['passed'] = True
    (ROOT / 'fine-tune.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('V3_FINE_TUNE_COMPLETE', ROOT / 'fine-tune.json')


if __name__ == '__main__':
    main()
