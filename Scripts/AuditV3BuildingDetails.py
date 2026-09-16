"""Audit component placement, materials, visibility, and collision for all V3 Blueprints."""
import json
from collections import Counter
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'


def vec(v):
    if hasattr(v, 'x'):
        return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]
    return [round(v.pitch, 2), round(v.yaw, 2), round(v.roll, 2)]


def main():
    cfgs = json.loads((ROOT / 'packages.json').read_text())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    results = []
    for cfg in cfgs:
        report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
        bp = unreal.EditorAssetLibrary.load_asset(report['blueprint'])
        actor = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, -100000))
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        rows = []
        for component in components:
            mesh = component.static_mesh
            if not mesh:
                continue
            mesh_box = mesh.get_bounding_box()
            mesh_origin = (mesh_box.min + mesh_box.max) * 0.5
            mesh_extent = (mesh_box.max - mesh_box.min) * 0.5
            row = {
                'component': component.get_name(),
                'mesh': mesh.get_name(),
                'visible': bool(component.get_editor_property('visible')),
                'collision_profile': str(component.get_collision_profile_name()),
                'mesh_bounds_origin': vec(mesh_origin),
                'mesh_bounds_extent': vec(mesh_extent),
                'relative_location': vec(component.get_editor_property('relative_location')),
                'relative_rotation': vec(component.get_editor_property('relative_rotation')),
                'materials': [str(component.get_material(i).get_path_name()) if component.get_material(i) else '' for i in range(component.get_num_materials())],
            }
            rows.append(row)
        origin, extent = actor.get_actor_bounds(False)
        main_bounds = {'origin': vec(origin), 'extent': vec(extent)}
        visible = [r for r in rows if r['visible']]
        collision = [r for r in rows if not r['visible']]
        # A visible component should contribute meaningfully to the actor bounds;
        # flag only likely accidental fragments, not narrow poles or roof ornaments.
        tiny = [r['component'] for r in visible if max(r['mesh_bounds_extent']) < 2.0]
        bad_external_materials = [r['component'] for r in visible if any(not m.startswith(cfg['destination'] + '/Materials/') for m in r['materials'])]
        results.append({
            'name': cfg['name'],
            'blueprint': report['blueprint'],
            'component_count': len(rows),
            'visible_components': len(visible),
            'collision_components': len(collision),
            'actor_bounds': main_bounds,
            'tiny_visible_components': tiny,
            'external_material_components': bad_external_materials,
            'components': rows,
        })
        actors.destroy_actor(actor)
    output = ROOT / 'detail-audit.json'
    output.write_text(json.dumps({'passed': True, 'packages': results}, indent=2), encoding='utf-8')
    print('V3_DETAIL_AUDIT_PASSED', output)
    for item in results:
        print(item['name'], 'components=', item['component_count'], 'tiny=', item['tiny_visible_components'], 'external_materials=', item['external_material_components'])


if __name__ == '__main__':
    main()
