"""Validate all V3 building details after the focused geometry corrections."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / 'Saved/RawModelImport/V3'
ROOF = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/RoofClean/RoofClean'
OLD_ROOF = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/SM_XiaobeiMen_Gate_Nanite/SM_XiaobeiMen_RoofTile_Green_LOD0'
CORRECTED_WOOD = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/GateDoorsAligned/SM_XiaobeiMen_Wood_Weathered_LOD0_DoorAligned'
CORRECTED_METAL = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/Meshes/GateDoorsAligned/SM_XiaobeiMen_Metal_Fittings_LOD0_DoorAligned'


def p(obj):
    return obj.get_path_name() if obj else ''


def main():
    configs = json.loads((RAW / 'packages.json').read_text())
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    blank = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    rows = []
    for cfg in configs:
        report = json.loads((RAW / (cfg['name'] + '-import.json')).read_text())
        bp = unreal.EditorAssetLibrary.load_asset(report['blueprint'])
        assert bp and bp.generated_class(), report['blueprint']
        actor = actor_subsystem.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, -100000))
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        visible = [c for c in components if c.get_editor_property('visible')]
        hidden = [c for c in components if not c.get_editor_property('visible')]
        assert visible and hidden, cfg['name']
        for c in visible:
            assert c.static_mesh
            assert str(c.get_collision_profile_name()) == 'NoCollision'
            for i in range(c.get_num_materials()):
                assert p(c.get_material(i)).startswith(cfg['destination'] + '/Materials/'), (cfg['name'], c.get_name(), p(c.get_material(i)))
        for c in hidden:
            assert c.static_mesh and c.get_editor_property('hidden_in_game')
            assert str(c.get_collision_profile_name()) == 'BlockAll'
        mesh_names = [p(c.static_mesh) for c in visible]
        if cfg['name'] == 'Xiaobeimen_Production_V3':
            assert any(name.startswith(ROOF) for name in mesh_names), mesh_names
            assert any(name.startswith(CORRECTED_WOOD) for name in mesh_names), mesh_names
            assert any(name.startswith(CORRECTED_METAL) for name in mesh_names), mesh_names
            assert OLD_ROOF not in mesh_names, mesh_names
        actor_subsystem.destroy_actor(actor)
        rows.append({'name': cfg['name'], 'visible': len(visible), 'collision': len(hidden), 'materials_package_local': True})
    # Preview levels must instantiate the same corrected Production component.
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    preview_checks = []
    for cfg in configs:
        report = json.loads((RAW / (cfg['name'] + '-import.json')).read_text())
        assert levels.load_level(report['preview_level']), report['preview_level']
        building = next(a for a in actor_subsystem.get_all_level_actors() if a.get_actor_label() == 'Preview_' + cfg['name'])
        meshes = [p(c.static_mesh) for c in building.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
        if cfg['name'] == 'Xiaobeimen_Production_V3':
            assert any(name.startswith(ROOF) for name in meshes), meshes
            assert OLD_ROOF not in meshes, meshes
        preview_checks.append({'name': cfg['name'], 'mesh_components': len(meshes), 'preview_corrected': True})
    assert levels.load_level('/Game/Scifi_desert_city/Level/L_showcase_level')
    showcase = next(a for a in actor_subsystem.get_all_level_actors()
                    if a.get_actor_label() == 'GuangzhouLandmark_Xiaobeimen_Production_V3')
    showcase_meshes = [p(c.static_mesh) for c in showcase.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
    assert any(name.startswith(ROOF) for name in showcase_meshes), showcase_meshes
    assert OLD_ROOF not in showcase_meshes, showcase_meshes
    showcase_check = {'label': showcase.get_actor_label(), 'mesh_components': len(showcase_meshes), 'corrected': True}
    result = {'passed': True, 'blueprints': rows, 'preview_levels': preview_checks,
              'showcase': showcase_check,
              'door_geometry': json.loads((ROOT / 'Saved/GateDoorFix/editor-validation.json').read_text()),
              'roof_geometry': json.loads((ROOT / 'Saved/GateDetailFix/roof-validation.json').read_text())}
    output = ROOT / 'Saved/RawModelImport/V3/all-details-validation.json'
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('V3_ALL_DETAILS_VALIDATED', json.dumps(result))


if __name__ == '__main__':
    main()
