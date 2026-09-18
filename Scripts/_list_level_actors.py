"""List every actor in the editor level, so stale transient spawns are visible."""
import unreal
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
print("LEVEL_ACTOR_COUNT", len(actors))
for a in actors:
    loc = a.get_actor_location()
    print(f"  {a.get_name()[:52]:54s} {a.get_class().get_name()[:28]:30s} loc=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f})")
