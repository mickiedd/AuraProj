"""Report imported V3 texture dimensions and source labels."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
EAL = unreal.EditorAssetLibrary

def main():
    cfgs = json.loads((ROOT / 'packages.json').read_text())
    rows = []
    for cfg in cfgs:
        report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        for source, asset_path in sorted(report['textures'].items()):
            tex = EAL.load_asset(asset_path)
            details = {'package': cfg['name'], 'source': source, 'asset': asset_path,
                       'size': [tex.blueprint_get_size_x(), tex.blueprint_get_size_y()],
                       'srgb': tex.get_editor_property('srgb')}
            for prop in ('imported_size', 'power_of_two_mode', 'compression_settings', 'mip_gen_settings'):
                try:
                    details[prop] = str(tex.get_editor_property(prop))
                except Exception as exc:
                    details[prop] = type(exc).__name__
            try:
                src = tex.get_editor_property('source')
                details['source_size'] = [src.get_size_x(), src.get_size_y()]
            except Exception as exc:
                details['source_size'] = type(exc).__name__
            rows.append(details)
    for row in rows:
        if '_4K' in row['source'] and row['size'] != [4096, 4096]:
            print('V3_TEXTURE_MISMATCH', json.dumps(row))
    print('V3_TEXTURE_COUNT', len(rows))

if __name__ == '__main__':
    main()
