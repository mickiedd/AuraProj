"""Read-only measurements of provisional wall markers and saved landmark meshes."""
import json
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'QA/Canton_Continuation/Walled_WallGate_Baseline.json'
MAP = '/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL'


def vec(v):
    return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    walls = []
    gates = []
    for actor in actors:
        label = actor.get_actor_label()
        if label.startswith('PROVISIONAL_Wall_'):
            walls.append({'label': label, 'location_cm': vec(actor.get_actor_location()),
                          'scale': vec(actor.get_actor_scale3d()),
                          'yaw_deg': round(float(actor.get_actor_rotation().yaw), 3)})
        elif label.startswith('PROVISIONAL_Landmark_'):
            meshes = []
            for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                mesh = comp.static_mesh
                if not mesh:
                    continue
                bounds = mesh.get_bounds()
                meshes.append({'component': comp.get_name(), 'mesh': mesh.get_name(),
                               'local_origin': vec(bounds.origin),
                               'local_extent': vec(bounds.box_extent),
                               'relative_location': vec(comp.get_editor_property('relative_location')),
                               'relative_scale': vec(comp.get_editor_property('relative_scale3d')),
                               'relative_rotation': str(comp.get_editor_property('relative_rotation'))})
            gates.append({'label': label, 'location_cm': vec(actor.get_actor_location()),
                          'yaw_deg': round(float(actor.get_actor_rotation().yaw), 3),
                          'meshes': meshes})
    assert len(gates) == 9 and walls
    OUT.write_text(json.dumps({'map': MAP, 'wall_count': len(walls),
                               'landmarks': gates, 'walls': walls}, indent=2) + '\n')
    unreal.log('CANTON_WALL_GATE_PROBE ' + json.dumps({'walls': len(walls),
                                                       'landmarks': len(gates)}))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({'error': traceback.format_exc()}, indent=2) + '\n')
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
