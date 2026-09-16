"""Report classes and load state for imported V3 assets."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
EAL = unreal.EditorAssetLibrary

def main():
    for cfg in json.loads((ROOT / 'packages.json').read_text()):
        report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        for entry in report['imports']:
            for path in entry['meshes']:
                asset = EAL.load_asset(path)
                if asset is None or not isinstance(asset, unreal.StaticMesh):
                    print('V3_ASSET_TYPE_MISMATCH', cfg['name'], path, 'asset=', asset, 'class=', asset.get_class().get_path_name() if asset else 'None')
                    break
    print('V3_ASSET_TYPE_SCAN_COMPLETE')

if __name__ == '__main__':
    main()
