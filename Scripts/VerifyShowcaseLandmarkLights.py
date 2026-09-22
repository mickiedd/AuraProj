"""Final state check after the per-building light job.

Confirms three things and nothing else:

  * the shared editor was left clean - no dirty content packages, no dirty map
    packages, and the showcase level is still the one that is open;
  * the saved level holds 34 actors with 9 landmark lights, each attached to its
    own landmark and carrying its configured intensity;
  * no temporary actor from the capture probes survived.

Read-only.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
LIGHT_TAG = "GuangzhouLandmarkLight"

print("FINAL_DIRTY_CONTENT", [package.get_name()
                              for package in unreal.EditorLoadingAndSavingUtils
                              .get_dirty_content_packages()])
print("FINAL_DIRTY_MAPS", [package.get_name()
                           for package in unreal.EditorLoadingAndSavingUtils
                           .get_dirty_map_packages()])

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print("FINAL_CURRENT_LEVEL", world.get_path_name() if world else None)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
print("FINAL_ACTOR_COUNT", len(actors))
print("FINAL_TEMP_ACTORS", sorted(actor.get_actor_label() for actor in actors
                                  if actor.get_actor_label().startswith("TEMP_")))

lights = [actor for actor in actors
          if LIGHT_TAG in [str(tag) for tag in actor.tags]]
print("FINAL_LIGHT_COUNT", len(lights))
for actor in sorted(lights, key=lambda item: item.get_actor_label()):
    component = actor.get_component_by_class(unreal.SpotLightComponent)
    parent = actor.get_attach_parent_actor()
    print("FINAL_LIGHT", json.dumps({
        "label": actor.get_actor_label(),
        "attached_to": parent.get_actor_label() if parent else None,
        "intensity_cd": round(float(component.get_editor_property("intensity")), 4),
        "outer_cone_deg": round(float(component.get_editor_property("outer_cone_angle")), 4),
        "attenuation_radius_cm": round(
            float(component.get_editor_property("attenuation_radius")), 2),
        "cast_shadows": bool(component.get_editor_property("cast_shadows")),
        "temperature_k": round(float(component.get_editor_property("temperature")), 1),
    }))
