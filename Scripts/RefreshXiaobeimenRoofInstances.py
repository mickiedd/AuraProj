"""Refresh saved level overrides to use the cleaned Production V3 roof."""
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/GateDetailFix'
OLD = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/SM_XiaobeiMen_Gate_Nanite/SM_XiaobeiMen_RoofTile_Green_LOD0'
NEW = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/RoofClean/RoofClean'
LEVELS = ['/Game/Scifi_desert_city/Level/L_showcase_level']
for cfg in json.loads((ROOT / 'Saved/RawModelImport/V3/packages.json').read_text()):
    report = json.loads((ROOT / ('Saved/RawModelImport/V3/' + cfg['name'] + '-import.json')).read_text())
    LEVELS.append(report['preview_level'])
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
updated = []
for level in LEVELS:
    assert levels.load_level(level), level
    if level == '/Game/Scifi_desert_city/Level/L_showcase_level':
        map_file = ROOT / 'Content/Scifi_desert_city/Level/L_showcase_level.umap'
        backup = OUT / 'L_showcase_level.before-roof-clean.umap'
        if not backup.exists():
            shutil.copy2(map_file, backup)
    else:
        safe = level.rsplit('/', 1)[-1]
        map_file = ROOT / ('Content' + level.removeprefix('/Game') + '.umap')
        backup = OUT / (safe + '.before-roof-clean.umap')
        if map_file.exists() and not backup.exists():
            shutil.copy2(map_file, backup)
    count = 0
    for actor in actors.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if component.static_mesh and component.static_mesh.get_path_name() == OLD:
                component.set_static_mesh(unreal.EditorAssetLibrary.load_asset(NEW))
                count += 1
    assert levels.save_current_level()
    updated.append({'level': level, 'updated_components': count})
(OUT / 'roof-level-refresh.json').write_text(json.dumps(updated, indent=2), encoding='utf-8')
print('ROOF_LEVELS_REFRESHED', updated)

