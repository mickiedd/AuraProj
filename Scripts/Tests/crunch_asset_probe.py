import json
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/Characters/Crunch', '/Game/ParagonCrunch'], True)
rows = []
for path in ['/Game/Characters/Crunch', '/Game/ParagonCrunch']:
    for data in registry.get_assets_by_path(path, recursive=True):
        package_name = str(data.package_name)
        asset_name = str(data.asset_name)
        class_path = getattr(data, 'asset_class_path', None)
        class_name = str(getattr(class_path, 'asset_name', class_path))
        object_path = package_name + '.' + asset_name
        rows.append({'object_path': object_path, 'package_name': package_name, 'asset_name': asset_name, 'asset_class': class_name})
out = 'C:/Git/AuraProj/Saved/Reports/CrunchMigration/source-asset-probe.json'
with open(out, 'w', encoding='utf-8') as fh:
    json.dump(rows, fh, indent=2)
unreal.log('CrunchAssetProbeCount=%d' % len(rows))
if rows:
    unreal.log('CrunchAssetProbeFirst=%s' % rows[0]['object_path'])
