"""Inspect static-mesh editor property exposure in commandlet mode."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
EAL = unreal.EditorAssetLibrary

def main():
    cfg = json.loads((ROOT / 'packages.json').read_text())[0]
    report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
    mesh = EAL.load_asset(report['imports'][0]['meshes'][0])
    unreal.log('MESH %s %s' % (mesh, mesh.get_class().get_path_name()))
    for prop in ('nanite_settings','light_map_resolution','light_map_coordinate_index','body_setup'):
        try:
            value = mesh.get_editor_property(prop)
            unreal.log('PROP %s %s' % (prop, str(value)))
            if prop == 'nanite_settings':
                unreal.log('NANITE_ENABLED %s' % value.enabled)
        except Exception as exc:
            unreal.log('PROP_ERROR %s %s %s' % (prop, type(exc).__name__, str(exc)))
    unreal.log('SUBSYSTEM %s' % unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem))

if __name__ == '__main__':
    main()
