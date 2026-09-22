"""Read-only probe run before building the per-landmark lights.

Two questions, both cheap to answer here and expensive to get wrong inside a
level rebuild that saves:

1. Is the shared editor clean? The showcase builder loads and re-saves
   L_GuangzhouLandmarkShowcase, so an unrelated dirty package would be swept
   along with it.
2. Do the exact SpotLightComponent properties and the attach call used by
   CreateGuangzhouLandmarkShowcase.py exist under those names in this build?
   A misspelled set_editor_property is swallowed by try/except at runtime and
   leaves the light on its default, which looks exactly like a configured light
   in a screenshot.

Read-only: nothing is spawned, moved or saved.
"""

import unreal

print("PROBE_DIRTY_CONTENT", [package.get_name()
                              for package in unreal.EditorLoadingAndSavingUtils
                              .get_dirty_content_packages()])
print("PROBE_DIRTY_MAPS", [package.get_name()
                           for package in unreal.EditorLoadingAndSavingUtils
                           .get_dirty_map_packages()])

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print("PROBE_CURRENT_LEVEL", world.get_path_name() if world else None)
print("PROBE_ENGINE_VERSION", unreal.SystemLibrary.get_engine_version())

for attribute in ("intensity", "attenuation_radius", "outer_cone_angle",
                  "inner_cone_angle", "cast_shadows", "source_radius",
                  "use_temperature", "temperature", "affects_world",
                  "intensity_units", "mobility"):
    print("PROBE_SPOTLIGHT_PROPERTY", attribute,
          hasattr(unreal.SpotLightComponent, attribute))

print("PROBE_LIGHT_UNITS", [name for name in dir(unreal.LightUnits)
                            if not name.startswith("_")])
print("PROBE_ATTACHMENT_RULES", [name for name in dir(unreal.AttachmentRule)
                                 if not name.startswith("_")])
print("PROBE_HAS_ATTACH", hasattr(unreal.Actor, "attach_to_actor"))
print("PROBE_HAS_GET_ATTACH_PARENT", hasattr(unreal.Actor, "get_attach_parent_actor"))
print("PROBE_ATTACH_DOC", unreal.Actor.attach_to_actor.__doc__)
print("PROBE_SPOTLIGHT_CLASS", unreal.SpotLight.static_class().get_name())
print("PROBE_HAS_SET_FOLDER_PATH", hasattr(unreal.Actor, "set_folder_path"))
