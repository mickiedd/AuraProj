"""Import all canonical meshes/textures and build four independent Actor Blueprints.

Run PrepareV4Buildings.py first (also normalizes Zhengximen ASCII FBX). In Unreal Python: import ImportV4Buildings as v;
v.import_package(0), then 1, 2 and 3. Imports only into the reserved V4 namespace.
Each successful source import is checkpointed, so interrupted jobs can resume.
"""
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / 'Saved/RawModelImport/V4'
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def write(path, value):
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def path(obj):
    return obj.get_path_name() if obj else ''


def texture_materials(cfg, report):
    package = Path(cfg['root'])
    dest = cfg['destination']
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    textures = {}
    for src in sorted((package / 'Textures').rglob('*.png')):
        if '.tmp.' in src.name: continue
        sub = src.parent.relative_to(package).as_posix()
        repair = cfg.get('texture_repairs', {}).get(src.relative_to(package).as_posix())
        asset_name = src.stem + ('_Rebuilt' if repair else '')
        asset_path = dest + '/' + sub + '/' + asset_name
        tex = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if not tex:
            task = unreal.AssetImportTask()
            import_file = cfg.get('texture_repairs', {}).get(src.relative_to(package).as_posix(), {}).get('source', str(src))
            task.filename, task.destination_path = import_file, dest + '/' + sub
            task.destination_name = asset_name
            task.automated, task.save, task.replace_existing = True, True, False
            tools.import_asset_tasks([task])
            assert task.imported_object_paths, str(src)
            tex = EAL.load_asset(task.imported_object_paths[0])
        assert isinstance(tex, unreal.Texture2D), asset_path
        normal = any(x in src.name for x in ('_Normal', '_N_'))
        scalar = any(('_' + channel + '_' in src.name or src.stem.endswith('_' + channel)) for channel in ('Roughness', 'Metallic', 'AO', 'Height', 'ORM', 'R', 'M', 'H', 'MR'))
        tex.set_editor_property('srgb', not (normal or scalar))
        if normal:
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('flip_green_channel', cfg['name'].startswith('Guidemen'))
        elif scalar:
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        EAL.save_loaded_asset(tex)
        textures[src.relative_to(package).as_posix()] = tex

    definitions = cfg['materials']

    materials = {}
    for name, maps in definitions.items():
        asset_path = dest + '/Materials/' + name
        mat = EAL.load_asset(asset_path) if EAL.does_asset_exist(asset_path) else None
        if not mat:
            mat = tools.create_asset(name, dest + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
            mat.set_editor_property('two_sided', cfg['name'].startswith(('Guidemen', 'Zhengximen')) or any(s in name for s in ('Fabric', 'Flag', 'Vegetation', 'Moss', 'Plaque')))
            for row, (channel, prop) in enumerate((('BaseColor', unreal.MaterialProperty.MP_BASE_COLOR),
                                                 ('Normal', unreal.MaterialProperty.MP_NORMAL),
                                                 ('Roughness', unreal.MaterialProperty.MP_ROUGHNESS),
                                                 ('Metallic', unreal.MaterialProperty.MP_METALLIC),
                                                 ('AO', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION))):
                if channel not in maps: continue
                tex = textures[maps[channel]]
                sample = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, row * 220)
                sample.set_editor_property('texture', tex)
                sampler = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
                if channel == 'Normal':
                    sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
                elif channel not in ('BaseColor',):
                    sampler = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
                sample.set_editor_property('sampler_type', sampler)
                assert MEL.connect_material_property(sample, 'RGB' if channel in ('Normal', 'BaseColor') else 'R', prop)
            if not maps:
                expr = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
                expr.set_editor_property('constant', unreal.LinearColor(.18,.29,.10,1))
                MEL.connect_material_property(expr, '', unreal.MaterialProperty.MP_BASE_COLOR)
            MEL.layout_material_expressions(mat)
            MEL.recompile_material(mat)
            assert EAL.save_loaded_asset(mat)
        materials[name] = mat
    report['textures'] = {k: path(v) for k, v in textures.items()}
    report['materials'] = {k: path(v) for k, v in materials.items()}
    return materials


def get_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def import_source(cfg, source, materials):
    if source['source'].endswith('.fbx'):
        return import_fbx(cfg, source, materials)
    dest = cfg['destination'] + '/Meshes/' + source['stem']
    assert not EAL.does_directory_exist(dest), 'Uncheckpointed partial destination: ' + dest
    asset_pipe = unreal.InterchangeGenericAssetsPipeline()
    asset_pipe.import_offset_rotation = unreal.Rotator(roll=-90)
    asset_pipe.common_meshes_properties.bake_meshes = False
    asset_pipe.common_meshes_properties.bake_pivot_meshes = False
    asset_pipe.mesh_pipeline.combine_static_meshes = False
    asset_pipe.mesh_pipeline.build_nanite = source['primary']
    asset_pipe.mesh_pipeline.set_editor_property('collision', False)
    asset_pipe.mesh_pipeline.import_collision_according_to_mesh_name = False
    asset_pipe.mesh_pipeline.generate_lightmap_u_vs = False
    # Native package-local materials replace preview/proxy materials by slot name.
    asset_pipe.material_pipeline.import_materials = False
    asset_pipe.material_pipeline.texture_pipeline.import_textures = False
    scene_pipe = unreal.InterchangeGenericLevelPipeline()
    scene_pipe.scene_hierarchy_type = unreal.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS
    params = unreal.ImportAssetParameters()
    params.is_automated, params.replace_existing = True, False
    params.import_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    params.override_pipelines = [unreal.SoftObjectPath(asset_pipe.get_path_name()), unreal.SoftObjectPath(scene_pipe.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    before = {path(a) for a in get_actors()}
    assert manager.import_scene(dest, manager.create_source_data(source['source']), params), source['relative']
    actors = [a for a in get_actors() if path(a) not in before]
    records, mesh_paths = [], set()
    sm_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for actor in actors:
        c = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not c or not c.static_mesh:
            continue
        mesh = c.static_mesh
        if path(mesh) not in mesh_paths:
            for i, slot in enumerate(mesh.get_editor_property('static_materials')):
                slotname = str(slot.material_slot_name)
                if not source['collision']:
                    assert slotname in materials, (source['relative'], slotname, list(materials))
                    mesh.set_material(i, materials[slotname])
            if source['collision']:
                sm_editor.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
            if not source['collision']:
                mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            if cfg['name'].startswith(('Guidemen', 'Zhengximen')):
                mesh.get_editor_property('body_setup').set_editor_property('double_sided_geometry', True)
            mesh_paths.add(path(mesh))
        # Override arrays may retain preview materials; explicitly bind native maps.
        for i, slot in enumerate(mesh.get_editor_property('static_materials')):
            if str(slot.material_slot_name) in materials:
                c.set_material(i, materials[str(slot.material_slot_name)])
        t = actor.get_actor_transform()
        r = t.rotation.rotator()
        records.append({'label': actor.get_actor_label(), 'mesh': path(mesh),
                        'location': [t.translation.x, t.translation.y, t.translation.z],
                        'rotation': [r.pitch, r.yaw, r.roll],
                        'scale': [t.scale3d.x, t.scale3d.y, t.scale3d.z]})
    assert len(mesh_paths) == source['mesh_count'], (source['relative'], len(mesh_paths), source['mesh_count'])
    assert len(records) == source['instance_count'], (source['relative'], len(records), source['instance_count'])
    assert EAL.save_directory(dest, only_if_is_dirty=False, recursive=True)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors:
        actor_subsystem.destroy_actor(actor)
    return {'source': source, 'meshes': sorted(mesh_paths), 'instances': records}


def create_blueprint(cfg, report):
    bp_path = cfg['destination'] + '/BP_' + cfg['name']
    assert not EAL.does_asset_exist(bp_path), bp_path
    bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(bp_path, unreal.Actor)
    assert bp
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = ss.k2_gather_subobject_data_for_blueprint(bp)
    root = next((h for h in handles if lib.is_root_component(ss.k2_find_subobject_data_from_handle(h))), None)
    if root is None:
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.SceneComponent, blueprint_context=bp)
        root, reason = ss.add_new_subobject(params)
        assert lib.is_handle_valid(root), str(reason)
    lib.get_object(ss.k2_find_subobject_data_from_handle(root)).set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    count, collision_count = 0, 0
    for entry in report['imports']:
        source = entry['source']
        collision = False
        if not source['primary']:
            continue
        for record in entry['instances']:
            params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent,
                blueprint_context=bp, conform_transform_to_parent=False)
            handle, reason = ss.add_new_subobject(params)
            assert lib.is_handle_valid(handle), str(reason)
            ss.rename_subobject(handle, unreal.Text(('Collision_' if collision else '') + record['label']))
            component = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
            mesh = EAL.load_asset(record['mesh'])
            component.set_static_mesh(mesh)
            component.set_editor_property('relative_location', unreal.Vector(*record['location']))
            component.set_editor_property('relative_rotation', unreal.Rotator(pitch=record['rotation'][0], yaw=record['rotation'][1], roll=record['rotation'][2]))
            component.set_editor_property('relative_scale3d', unreal.Vector(*record['scale']))
            component.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
            component.set_collision_profile_name('BlockAll')
            if collision:
                component.set_editor_property('visible', False)
                component.set_editor_property('hidden_in_game', True)
                component.set_editor_property('cast_shadow', False)
                collision_count += 1
            else:
                count += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EAL.save_loaded_asset(bp, only_if_is_dirty=False)
    report['blueprint'] = bp_path
    report['visible_components'] = count
    report['collision_components'] = collision_count
    report['parent'] = '/Script/Engine.Actor'
    return bp


def import_package(index):
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
    cfg = json.loads((ROOT / 'packages.json').read_text(encoding='utf-8'))[index]
    assert cfg['destination'].startswith('/Game/Assets/Environment/GuangzhouLandmarks/V4/')
    report_path = ROOT / (cfg['name'] + '-import.json')
    report = json.loads(report_path.read_text()) if report_path.exists() else {'name': cfg['name'], 'imports': []}
    if report.get('complete'):
        print('V4_ALREADY_COMPLETE', cfg['name'])
        return
    # Refuse to discard an unsaved map; never save unrelated dirty materials.
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, 'Save your current map before running the V4 import: ' + str([path(x) for x in dirty])
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    materials = texture_materials(cfg, report)
    write(report_path, report)
    completed = {e['source']['relative'] for e in report['imports']}
    for source in sorted(cfg['meshes'], key=lambda s: (not s['primary'], s['relative'])):
        if source['relative'] in completed:
            entry = next(e for e in report['imports'] if e['source']['relative'] == source['relative'])
            assert all(EAL.does_asset_exist(p) for p in entry['meshes'])
            continue
        report['imports'].append(import_source(cfg, source, materials))
        write(report_path, report)
        print('V4_IMPORTED', cfg['name'], source['relative'])
    bp = create_blueprint(cfg, report)
    write(report_path, report)
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(bp.generated_class(), unreal.Vector())
    assert actor
    actor.set_actor_label('Preview_' + cfg['name'])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    report['preview_level'] = cfg['destination'] + '/L_' + cfg['name'] + '_Preview'
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, report['preview_level'])
    report['complete'] = True
    write(report_path, report)
    print('V4_PACKAGE_COMPLETE', cfg['name'], report['visible_components'], report['collision_components'])


def import_fbx(cfg, source, materials):
    dest = cfg['destination'] + '/Meshes/' + source['stem']
    task = unreal.AssetImportTask()
    assert not EAL.does_directory_exist(dest), 'Uncheckpointed FBX destination: ' + dest
    task.filename, task.destination_path = source['source'], dest
    task.automated, task.save, task.replace_existing = True, True, False
    ui = unreal.FbxImportUI()
    ui.import_mesh, ui.import_materials, ui.import_textures = True, False, False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.automated_import_should_detect_type = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.generate_lightmap_u_vs = False
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.build_nanite = source['primary']
    task.options = ui
    # UE 5.5 Interchange rejects the package ASCII FBX; use the native FBX importer.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_flag = unreal.SystemLibrary.get_console_variable_bool_value('Interchange.FeatureFlags.Import.FBX')
    unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        task.factory = unreal.FbxFactory()
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX ' + str(int(old_flag)))
    meshes = [x for x in task.get_objects() if isinstance(x, unreal.StaticMesh)]
    assert len(meshes) == 1, task.imported_object_paths
    mesh = meshes[0]
    for i, slot in enumerate(mesh.get_editor_property('static_materials')):
        name = str(slot.material_slot_name)
        assert name in materials, (name, list(materials))
        mesh.set_material(i, materials[name])
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.get_editor_property('body_setup').set_editor_property('double_sided_geometry', True)
    assert EAL.save_loaded_asset(mesh)
    return {'source': source, 'meshes': [path(mesh)], 'instances': [{'label':source['stem'], 'mesh':path(mesh), 'location':[0,0,0], 'rotation':[0,0,0], 'scale':[1,1,1]}]}


if __name__ == '__main__':
    for package_index in range(4):
        import_package(package_index)
