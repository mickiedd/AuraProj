"""Read-only probe of BP_Xiaobeimen_AAA_V3: components, meshes, transforms, bounds.

Writes JSON rather than printing, because a commandlet's captured stdout does not carry
Python print reliably. Bounds are computed by transforming the mesh's own local bounds
through each component's relative transform — the Blueprint's components are authored in
the building's own space, so that is the actor-space box.
"""
import json
from pathlib import Path
import unreal

BP = ('/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
      '/BP_Xiaobeimen_AAA_V3')
MESH_ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/Meshes'

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
bp = unreal.EditorAssetLibrary.load_asset(BP)
assert bp, BP
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary


def mesh_bounds_corners(mesh):
    bounds = mesh.get_bounds()
    o, e = bounds.origin, bounds.box_extent
    return [[float(o.x) + sx * float(e.x), float(o.y) + sy * float(e.y),
             float(o.z) + sz * float(e.z)]
            for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]


def transform_corners(corners, loc, rot, scale):
    """Apply scale, then rotation, then translation — matching a component transform."""
    q = rot.quaternion()
    out = []
    for x, y, z in corners:
        v = unreal.Vector(x * scale.x, y * scale.y, z * scale.z)
        v = q.rotate_vector(v)
        out.append([v.x + loc.x, v.y + loc.y, v.z + loc.z])
    return out


rows = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    obj = library.get_object(data)
    if obj is None:
        continue
    row = {'name': obj.get_name(), 'class': obj.get_class().get_name()}
    mesh = getattr(obj, 'static_mesh', None)
    if mesh is None:
        rows.append(row)
        continue
    row['mesh'] = mesh.get_path_name()
    try:
        loc = obj.get_editor_property('relative_location')
        rot = obj.get_editor_property('relative_rotation')
        scale = obj.get_editor_property('relative_scale3d')
        row['location'] = [round(loc.x, 3), round(loc.y, 3), round(loc.z, 3)]
        row['rotation'] = [round(rot.roll, 3), round(rot.pitch, 3), round(rot.yaw, 3)]
        row['scale'] = [round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)]
        corners = transform_corners(mesh_bounds_corners(mesh), loc, rot, scale)
        row['box'] = [[round(min(c[i] for c in corners), 2) for i in range(3)],
                      [round(max(c[i] for c in corners), 2) for i in range(3)]]
        row['box_size'] = [round(row['box'][1][i] - row['box'][0][i], 2) for i in range(3)]
    except Exception as exc:  # noqa: BLE001
        row['transform_error'] = '{}: {}'.format(type(exc).__name__, exc)
    try:
        row['materials'] = [
            (obj.get_material(i).get_path_name() if obj.get_material(i) else 'None')
            for i in range(obj.get_num_materials())]
    except Exception as exc:  # noqa: BLE001
        row['materials_error'] = '{}: {}'.format(type(exc).__name__, exc)
    try:
        row['visible'] = bool(obj.get_editor_property('visible'))
    except Exception as exc:  # noqa: BLE001
        row['visible_error'] = '{}: {}'.format(type(exc).__name__, exc)
    rows.append(row)

# Local bounds of the modular meshes, so the geometry itself can be reasoned about.
modular = {}
for name in ('MOD_Plaque', 'MOD_WoodenDoorLeaf', 'MOD_WallStone_5m', 'MOD_ArchVoussoir',
             'MOD_Barrel', 'MOD_Battlement'):
    mesh = unreal.EditorAssetLibrary.load_asset('{}/{}'.format(MESH_ROOT, name))
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    o, e = mesh.get_bounds().origin, mesh.get_bounds().box_extent
    modular[name] = {'triangles': int(mesh.get_num_triangles(0)),
                     'box_min': [round(float(o.x) - float(e.x), 3),
                                 round(float(o.y) - float(e.y), 3),
                                 round(float(o.z) - float(e.z), 3)],
                     'box_max': [round(float(o.x) + float(e.x), 3),
                                 round(float(o.y) + float(e.y), 3),
                                 round(float(o.z) + float(e.z), 3)],
                     'size_cm': [round(float(e.x) * 2, 2), round(float(e.y) * 2, 2),
                                 round(float(e.z) * 2, 2)]}

report = {'blueprint': bp.get_path_name(), 'component_count': len(rows),
          'components': rows, 'modular_meshes': modular}
out = project / 'Saved/Reports/Xiaobeimen/blueprint-probe.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_PROBE ' + json.dumps({'components': len(rows)}))
