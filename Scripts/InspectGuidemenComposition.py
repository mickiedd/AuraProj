"""Read-only inspection for the live Guidemen upgrade composition and viewport."""
import json
import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
UPGRADE_TAG = "GuidemenRealismUpgrade"


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z]


world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
group = [actor for actor in actors if UPGRADE_TAG in [str(tag) for tag in actor.tags]]
adjacent = [actor for actor in actors if "ImportedGreatWestGate" in [str(tag) for tag in actor.tags]]
viewport = None
try:
    viewport = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
except Exception as exc:
    viewport = {"error": str(exc)}
result = {
    "world": world.get_path_name(),
    "group_count": len(group),
    "group": [
        {
            "label": actor.get_actor_label(),
            "location": [actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z],
            "rotation": [actor.get_actor_rotation().roll, actor.get_actor_rotation().pitch, actor.get_actor_rotation().yaw],
            "scale": [actor.get_actor_scale3d().x, actor.get_actor_scale3d().y, actor.get_actor_scale3d().z],
            "bounds": bounds(actor),
            "components": [component.get_name() for component in actor.get_components_by_class(unreal.StaticMeshComponent)],
        }
        for actor in sorted(group, key=lambda item: item.get_actor_label())
    ],
    "adjacent_group_count": len(adjacent),
    "adjacent_rotations": sorted({round(actor.get_actor_rotation().yaw, 3) for actor in adjacent}),
    "viewport": str(viewport),
    "actor_add_component_by_class": hasattr(unreal.Actor, "add_component_by_class"),
}
print("GUIDEMEN_COMPOSITION_INSPECTION", json.dumps(result))
