"""Probe BP_Xiaobeimen_AAA_V3's parts for triangle counts and M_StoneWall's UV setup.

"Optimize the bricks" could be a scale problem (the wall reads as fine modern brick) or a
cost problem. This measures both: every visible part's triangle count, and whether
M_StoneWall exposes a UV scale that can be changed without editing the mesh.
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
BP = ROOT + '/BP_Xiaobeimen_AAA_V3'

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
bp = unreal.EditorAssetLibrary.load_asset(BP)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary

parts = {}
seen = set()
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    obj = library.get_object(data)
    mesh = getattr(obj, 'static_mesh', None) if obj else None
    if mesh is None:
        continue
    name = mesh.get_name()
    if name in seen:
        continue
    seen.add(name)
    parts[name] = {'triangles': int(mesh.get_num_triangles(0)),
                   'lod_count': int(mesh.get_lod_count())
                   if hasattr(mesh, 'get_lod_count') else None}

# Material graph: what drives UVs, and which texture parameters exist.
material = unreal.EditorAssetLibrary.load_asset(ROOT + '/Materials/M_StoneWall')
material_info = {'found': bool(material)}
if material:
    try:
        material_info['texture_params'] = sorted(
            str(n) for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(material))
    except Exception as exc:  # noqa: BLE001
        material_info['texture_params_error'] = str(exc)
    try:
        material_info['scalar_params'] = sorted(
            str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material))
    except Exception as exc:  # noqa: BLE001
        material_info['scalar_params_error'] = str(exc)
    try:
        nodes = []
        for expr in unreal.MaterialEditingLibrary.get_material_expressions(material):
            entry = {'class': expr.get_class().get_name()}
            if 'TextureCoordinate' in entry['class'] or 'TexCoord' in entry['class']:
                try:
                    entry['utiling'] = float(expr.get_editor_property('u_tiling'))
                    entry['vtiling'] = float(expr.get_editor_property('v_tiling'))
                except Exception:  # noqa: BLE001
                    pass
            if 'Multiply' in entry['class']:
                try:
                    entry['const_b'] = float(expr.get_editor_property('const_b'))
                except Exception:  # noqa: BLE001
                    pass
            nodes.append(entry)
        material_info['nodes'] = nodes
    except Exception as exc:  # noqa: BLE001
        material_info['nodes_error'] = str(exc)

report = {'parts': parts,
          'total_triangles': sum(p['triangles'] for p in parts.values()),
          'material': material_info}
out = project / 'Saved/Reports/Xiaobeimen/parts-and-material.json'
out.write_text(json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_PARTS ' + json.dumps({'total': report['total_triangles']}))
