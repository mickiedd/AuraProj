"""Remove only transient V5 reference-tuning capture actors; never save the map."""
import unreal


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for actor in list(actors.get_all_level_actors()):
        if actor.get_actor_label().startswith("V5TUNE_"):
            actors.destroy_actor(actor)
            removed += 1
    print("V5_REFERENCE_CLEANUP", removed, "saved_map=False")


if __name__ == "__main__":
    main()
