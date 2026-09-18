"""Remove only zero-roll probe actors; leave the private probe assets intact for audit."""
import unreal


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in list(actors.get_all_level_actors()):
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = component.static_mesh if component else None
        if mesh and "ReferenceTuned20260917ZeroRollProbe" in mesh.get_path_name():
            actors.destroy_actor(actor)
            removed += 1
        elif actor.get_actor_label().startswith("V5ZEROPROBE_"):
            actors.destroy_actor(actor)
            removed += 1
    print("V5_ZERO_ROLL_PROBE_CLEANUP", removed, "saved_map=False")


if __name__ == "__main__":
    main()
