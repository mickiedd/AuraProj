import json
import os
from collections import deque

import unreal

SOURCE_ROOT = 'C:/Works/Crunch-master'
OUTPUT = 'C:/Git/AuraProj/Saved/Reports/CrunchMigration/source-dependency-file-list.json'
SEEDS = [
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Meshes/Crunch_Skeleton',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_01',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_02',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_03',
    '/Game/ParagonCrunch/Characters/Heroes/Crunch/Animations/Ability_Combo_04',
]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/ParagonCrunch'], True)
queue = deque(SEEDS)
seen = set()
rows = []
while queue:
    package = queue.popleft()
    if package in seen or not package.startswith('/Game/'):
        continue
    seen.add(package)
    source_path = os.path.join(SOURCE_ROOT, 'Content', package[len('/Game/'):].replace('/', os.sep) + '.uasset')
    row = {'package': package, 'sourcePath': source_path, 'exists': os.path.isfile(source_path)}
    try:
        deps = registry.get_dependencies(package, unreal.AssetRegistryDependencyOptions()) or []
        row['dependencies'] = sorted(set(str(value) for value in deps))
        for dep in row['dependencies']:
            if dep.startswith('/Game/') and dep not in seen:
                queue.append(dep)
    except Exception as exc:
        row['dependenciesError'] = str(exc)
    rows.append(row)
with open(OUTPUT, 'w', encoding='utf-8') as stream:
    json.dump({'schemaVersion': 1, 'seeds': SEEDS, 'packageCount': len(rows), 'rows': rows}, stream, indent=2, sort_keys=True)
unreal.log('CrunchDependencyFileListCount=%d' % len(rows))
