"""Fix BP_Xiaobeimen_AAA_V3: plaque height, arch-fitted door, ashlar wall scale.

Three defects, all measured rather than guessed (see `Saved/Reports/Xiaobeimen/`):

1. **Plaque.** `Plaque_Body` spans z 497.5..612.5 and `Plaque_Face` z 513..597 — both inside
   the arch's opening, which is a semicircle of intrados radius 265 centred at z = 370, so
   its crown is at 635. The tablet floats in the arch's mouth. It belongs on the wall above
   the crown, centred in the 152.5 cm between the crown and the wall coping at 787.5:
   a 115 cm tablet centred there sits at z 653.5..768.5. Offset **+156 cm**.
2. **Door.** The leaves were flat-topped blocks ending at z 344.5, below the springing.
   Reimported from the authored source, closed, with the intrados as their top edge.
3. **Wall.** The stone units measured 18 x 15 cm; the reference is ~50 x 28 cm. The maps
   are re-authored at ashlar scale and the material's texture coordinates are scaled to
   0.25 so one map covers 320 cm instead of 80.

Refuses to run while a GUI editor holds the project.
"""
import json
import subprocess
from pathlib import Path
import unreal

ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
BP = ROOT + '/BP_Xiaobeimen_AAA_V3'
MESH_DEST = ROOT + '/Meshes/Xiaobeimen_HP'
TEX_DEST = ROOT + '/Textures/StoneWall'
PLAQUE_RISE_CM = 156.0
# The authored component offset, measured from the probe. The plaque is set to an
# absolute target rather than nudged, so re-running this script is a no-op.
PLAQUE_BASE_Z = 156.0
PLAQUE_TARGET_Z = PLAQUE_BASE_Z + PLAQUE_RISE_CM
UV_SCALE = 0.25
REPORT_PATH = None  # set below

_running = subprocess.run(['pgrep', '-f', r'UnrealEditor\.app'],
                          capture_output=True, text=True).stdout.split()
assert not _running, 'close the GUI editor first (pid {})'.format(', '.join(_running))

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
prepared = project / 'Raw3DPacket/Xiaobeimen/prepared/Textures'
door_glb = project / 'Saved/Reports/Xiaobeimen/door-glb'
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary

report = {'steps': [], 'problems': []}


def components():
    bp = eal.load_asset(BP)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        yield handle, data, library.get_object(data)


# ------------------------------------------------------------------ 1. plaque
bp = eal.load_asset(BP)
assert bp, BP
# The subobject walk yields each component more than once (template and class default),
# so dedupe by name or the plaque would be raised twice.
seen, moved = set(), []
for _handle, _data, obj in components():
    if obj is None or not isinstance(obj, unreal.StaticMeshComponent):
        continue
    name = obj.get_name()
    if not name.startswith('Plaque_') or name in seen:
        continue
    seen.add(name)
    current = obj.get_editor_property('relative_location')
    # Only Z changes: the tablet is already centred on the arch in x, and its standoff
    # from the wall face is correct. Set absolutely so a re-run changes nothing.
    obj.set_editor_property('relative_location',
                            unreal.Vector(current.x, current.y, PLAQUE_TARGET_Z))
    moved.append({'component': name, 'from_z': round(current.z, 2),
                  'to_z': PLAQUE_TARGET_Z,
                  'changed': abs(current.z - PLAQUE_TARGET_Z) > 0.01})
report['plaque'] = moved
report['steps'].append('plaque_raised_{}'.format(len(moved)))


# -------------------------------------------------------------------- 2. door
def interchange_import(source, asset_name, roll, scale):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = asset_name
    pipeline.import_offset_rotation = unreal.Rotator(roll=roll)
    pipeline.import_offset_uniform_scale = scale
    pipeline.common_meshes_properties.bake_meshes = True
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = False
    pipeline.mesh_pipeline.set_editor_property('collision', False)
    pipeline.mesh_pipeline.import_collision_according_to_mesh_name = False
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    pipeline.material_pipeline.import_materials = False
    pipeline.material_pipeline.texture_pipeline.import_textures = False
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    return manager.import_asset(MESH_DEST, manager.create_source_data(str(source)), params)


def mesh_actor_box(mesh, roll):
    """The mesh's box after the component's roll, in actor axes."""
    bounds = mesh.get_bounds()
    o, e = bounds.origin, bounds.box_extent
    corners = [unreal.Vector(o.x + sx * e.x, o.y + sy * e.y, o.z + sz * e.z)
               for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    q = unreal.Rotator(roll=roll).quaternion()
    rotated = [q.rotate_vector(c) for c in corners]
    low = [round(min(float(getattr(v, a)) for v in rotated), 1) for a in ('x', 'y', 'z')]
    high = [round(max(float(getattr(v, a)) for v in rotated), 1) for a in ('x', 'y', 'z')]
    return low, high


door_rows = []
# Expected boxes are in MESH-LOCAL space, not actor space: the components carry a -90 roll,
# so the mesh's local axes are (actor_x, actor_z, -actor_y) and that is what is measured.
for name, expected in (('Wooden_Doors__M_AgedWood',
                        {'x': (-264.0, 264.0), 'y': (0.0, 635.0), 'z': (-445.0, -430.0)}),
                       ('Door_Metal__M_Metal',
                        {'x': (-249.3, 249.3), 'y': (46.6, 585.5),
                         'z': (-448.4, -445.0)}),
                       # The wall's arch curve: rebuilt at 0.5 degrees (a 2.31 cm chord)
                       # from the original ~4 degrees (18 cm), which is what the 锯齿 was.
                       ('Wall_ArchSpandrel__M_StoneWall',
                        {'x': (-260.0, 260.0), 'y': (421.0, 630.2),
                         'z': (-450.0, 450.0)})):
    source = door_glb / (name + '.glb')
    assert source.exists(), source
    mesh = None
    # Two conventions to satisfy, both measured rather than assumed:
    #  - the file carries centimetres but glTF positions are metres, so the import needs
    #    an offset scale of 0.01 or every part lands 100x too large;
    #  - at roll 0 Interchange maps the file's (X, Y, Z) to mesh-local (X, Z, Y), which is
    #    why the source is authored as (actor_x, -actor_y, actor_z).
    for roll in (0.0, -90.0, 90.0, 180.0):
        interchange_import(source, name, roll, 0.01)
        candidate = eal.load_asset('{}/{}'.format(MESH_DEST, name))
        if not isinstance(candidate, unreal.StaticMesh):
            continue
        low, high = mesh_actor_box(candidate, roll)
        ok = (abs(low[0] - expected['x'][0]) < 3 and abs(high[0] - expected['x'][1]) < 3
              and abs(low[1] - expected['y'][0]) < 3 and abs(high[1] - expected['y'][1]) < 3
              and abs(low[2] - expected['z'][0]) < 3 and abs(high[2] - expected['z'][1]) < 3)
        door_rows.append({'mesh': name, 'roll': roll, 'actor_box': [low, high],
                          'expected': expected, 'matches': ok})
        if ok:
            mesh = candidate
            break
    if mesh is None:
        report['problems'].append('{} did not land in the expected box'.format(name))
    else:
        eal.save_loaded_asset(mesh, only_if_is_dirty=False)
report['door'] = door_rows
report['steps'].append('door_reimported')


# -------------------------------------------------------------------- 3. wall
material = eal.load_asset(ROOT + '/Materials/M_StoneWall')
assert material, 'M_StoneWall missing'
# Adding a TexCoord -> Multiply pair is NOT idempotent, and the graph cannot be walked
# from Python (no expression enumeration), so a previous run's report is the record that
# decides whether to do it again. Without this a re-run would square the scale.
out_path = project / 'Saved/Reports/Xiaobeimen/fix-report.json'
previous = json.loads(out_path.read_text()) if out_path.exists() else {}
already_scaled = bool(previous.get('wall_uv', {}).get('linked'))
if already_scaled:
    report['wall_uv'] = dict(previous['wall_uv'], skipped='already applied')
else:
    # The graph is five TextureSample nodes wired straight to their properties with no
    # TexCoord node, so one added TexCoord -> Multiply feeds all five.
    texcoord = MEL.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    texcoord.set_editor_property('u_tiling', 1.0)
    texcoord.set_editor_property('v_tiling', 1.0)
    multiply = MEL.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -700, 0)
    multiply.set_editor_property('const_b', UV_SCALE)
    MEL.connect_material_expressions(texcoord, '', multiply, 'A')
    linked = []
    for prop_name in ('MP_BASE_COLOR', 'MP_NORMAL', 'MP_ROUGHNESS', 'MP_METALLIC',
                      'MP_AMBIENT_OCCLUSION'):
        prop = getattr(unreal.MaterialProperty, prop_name)
        node = MEL.get_material_property_input_node(material, prop)
        if node is None:
            continue
        try:
            MEL.connect_material_expressions(multiply, '', node, 'Coordinates')
            linked.append(prop_name)
        except Exception as exc:  # noqa: BLE001
            report['problems'].append('{}: {}'.format(prop_name, exc))
    MEL.recompile_material(material)
    assert eal.save_loaded_asset(material, only_if_is_dirty=False)
    report['wall_uv'] = {'scale': UV_SCALE, 'linked': linked,
                         'texture_world_cm': 80.0 / UV_SCALE}
    report['steps'].append('wall_uv_scaled')

# Re-authored maps, same asset names and size so the material needs no other change.
tools = unreal.AssetToolsHelpers.get_asset_tools()
refreshed = []
for kind in ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO'):
    source = prepared / 'StoneWall_{}_4K.png'.format(kind)
    if not source.exists():
        report['problems'].append('missing authored map ' + source.name)
        continue
    asset_name = 'StoneWall_{}_4K'.format(kind)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', TEX_DEST)
    task.set_editor_property('destination_name', asset_name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    tools.import_asset_tasks([task])
    texture = eal.load_asset('{}/{}'.format(TEX_DEST, asset_name))
    if isinstance(texture, unreal.Texture2D):
        texture.set_editor_property('srgb', kind in ('BaseColor', 'AO'))
        if kind == 'Normal':
            texture.set_editor_property('flip_green_channel', False)
        eal.save_loaded_asset(texture)
        refreshed.append({'kind': kind, 'size': [texture.blueprint_get_size_x(),
                                                 texture.blueprint_get_size_y()]})
    else:
        report['problems'].append('{} did not import'.format(asset_name))
report['wall_textures'] = refreshed
report['steps'].append('wall_textures_refreshed')

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert eal.save_loaded_asset(bp, only_if_is_dirty=False)
report['passed'] = not report['problems']
out = project / 'Saved/Reports/Xiaobeimen/fix-report.json'
out.write_text(json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_FIX ' + json.dumps({'passed': report['passed'],
                                           'steps': report['steps'],
                                           'problems': report['problems']}))
