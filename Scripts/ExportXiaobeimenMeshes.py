"""Export the Xiaobeimen V3 meshes this job needs to OBJ, for measurement.

The V3 asset has no procedural source: the wall, arch, plaque and door exist only as
binary .uasset. To fit a door to the arch, move the plaque to a measured height, or judge
the wall's block scale, the geometry has to be read — and OBJ carries positions and UVs
that plain Python can parse.
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
PARTS = ('Arch_Voussoirs__M_StoneWall', 'Wooden_Doors__M_AgedWood',
         'Plaque_Body__M_StoneWall', 'Plaque_Face__M_Plaque',
         'Wall_Main__M_StoneWall', 'Wall_ArchSpandrel__M_StoneWall',
         'Wall_Coping__M_StoneWall', 'Battlements__M_StoneWall',
         'Door_Metal__M_Metal')

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out_dir = project / 'Saved/Reports/Xiaobeimen/meshes'
out_dir.mkdir(parents=True, exist_ok=True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

exported = []
for part in PARTS:
    mesh = eal.load_asset('{}/Meshes/Xiaobeimen_HP/{}'.format(ROOT, part))
    if not isinstance(mesh, unreal.StaticMesh):
        exported.append({'part': part, 'error': 'not a StaticMesh'})
        continue
    target = out_dir / (part + '.obj')
    options = unreal.AssetExportTask()
    options.set_editor_property('exporter', unreal.StaticMeshExporterOBJ())
    options.set_editor_property('automated', True)
    options.set_editor_property('replace_identical', True)
    options.set_editor_property('prompt', False)
    ok = tools.export_assets([mesh], str(out_dir), options)
    produced = sorted(out_dir.glob('*.obj'))
    exported.append({'part': part, 'ok': bool(ok),
                     'triangles': int(mesh.get_num_triangles(0)),
                     'files': [p.name for p in produced]})

report = {'exported': exported, 'out': str(out_dir),
          'files': sorted(p.name for p in out_dir.glob('*.obj'))}
(project / 'Saved/Reports/Xiaobeimen/mesh-export.json').write_text(
    json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_MESHES_EXPORTED ' + json.dumps(
    {'files': report['files']}))
