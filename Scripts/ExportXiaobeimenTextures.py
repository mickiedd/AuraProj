"""Export the Xiaobeimen V3 textures and material bindings for inspection.

The V3 asset has no source art outside the engine — the wall, plaque, door and metal maps
exist only as .uasset — so this is the only way to measure what the materials actually
sample. A commandlet can do asset export; it needs no RHI.
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
FOLDERS = ('StoneWall', 'Plaque', 'AgedWood', 'MetalFittings')
KINDS = ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO', 'Height', 'ORM')

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out_dir = project / 'Saved/Reports/Xiaobeimen/textures'
out_dir.mkdir(parents=True, exist_ok=True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

exported = []
for folder in FOLDERS:
    for kind in KINDS:
        name = '{}_{}_4K'.format(folder, kind)
        texture = eal.load_asset('{}/Textures/{}/{}'.format(ROOT, folder, name))
        if not isinstance(texture, unreal.Texture2D):
            continue
        target = out_dir / (name + '.png')
        task = unreal.AssetExportTask()
        task.set_editor_property('object', texture)
        task.set_editor_property('filename', str(target))
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_identical', True)
        task.set_editor_property('exporter', unreal.TextureExporterPNG())
        ok = tools.export_asset_tasks([task])
        exported.append({'name': name, 'ok': bool(ok),
                         'size': [texture.blueprint_get_size_x(),
                                  texture.blueprint_get_size_y()],
                         'srgb': bool(texture.get_editor_property('srgb')),
                         'bytes': target.stat().st_size if target.exists() else 0})

# Material bindings, so the texture-to-parameter wiring is known rather than assumed.
materials = {}
for material_name in ('M_StoneWall', 'M_Plaque', 'M_AgedWood', 'M_Metal'):
    material = eal.load_asset('{}/Materials/{}'.format(ROOT, material_name))
    if not isinstance(material, unreal.MaterialInterface):
        continue
    params = {}
    try:
        names = unreal.MaterialEditingLibrary.get_texture_parameter_names(material)
        for param in names:
            tex = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
                material, param)
            params[str(param)] = tex.get_path_name() if tex else None
    except Exception as exc:  # noqa: BLE001
        params['error'] = '{}: {}'.format(type(exc).__name__, exc)
    materials[material_name] = params

report = {'exported': exported, 'materials': materials, 'out': str(out_dir)}
(project / 'Saved/Reports/Xiaobeimen/texture-export.json').write_text(
    json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_TEXTURES_EXPORTED ' + json.dumps({'count': len(exported)}))
