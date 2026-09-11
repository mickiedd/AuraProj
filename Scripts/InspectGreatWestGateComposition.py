"""Read-only inspection of the live Great West Gate composition and alignment."""
import json

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
PART_TAG = "ImportedGreatWestGate"
ROOT_TAG = "GreatWestGateRoot"


loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
         if loaded else None)
assert world, LEVEL_PATH

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
parts = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
roots = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
adjacent = [actor for actor in actors if "ImportedGuidemenBuilding" in [str(tag) for tag in actor.tags]]


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z]


def actor_row(actor):
    rotation = actor.get_actor_rotation()
    location = actor.get_actor_location()
    return {
        "name": actor.get_name(),
        "label": actor.get_actor_label(),
        "class": actor.get_class().get_name(),
        "location": [location.x, location.y, location.z],
        "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
        "parent": actor.get_attach_parent_actor().get_actor_label()
                   if actor.get_attach_parent_actor() else None,
        "bounds": bounds(actor),
    }


result = {
    "level": LEVEL_PATH,
    "part_count": len(parts),
    "root_count": len(roots),
    "adjacent_guidemen_count": len(adjacent),
    "parts": [actor_row(actor) for actor in sorted(parts, key=lambda a: a.get_actor_label())],
    "roots": [actor_row(actor) for actor in roots],
    "adjacent_guidemen": [actor_row(actor) for actor in adjacent],
}
print("GREAT_WEST_GATE_COMPOSITION_INSPECTION", json.dumps(result))
