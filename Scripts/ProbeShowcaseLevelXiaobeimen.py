"""Read-only: what does Scifi_desert_city/Level/L_showcase_level hold?

The referencer query for BP_Xiaobeimen_Production_V3 came back with two levels,
not one, so before touching anything this loads the second level and reports
every actor that comes from that Blueprint or carries a landmark tag.

Loads the level (the editor is clean, verified beforehand) but saves nothing and
deletes nothing.
"""

import json

import unreal

LEVEL = "/Game/Scifi_desert_city/Level/L_showcase_level"
BP = ("/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"
      "/BP_Xiaobeimen_Production_V3")

TAGS = ("GuangzhouLandmarkShowcase", "GuangzhouLandmarkPark",
        "ImportedGuangzhouLandmark", "GuangzhouLandmarkLight")


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("PROBE2_WORLD_BEFORE", world.get_path_name() if world else None)
    print("PROBE2_DIRTY_MAPS_BEFORE", [str(p.get_name()) for p in
          unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
    print("PROBE2_DIRTY_CONTENT_BEFORE", [str(p.get_name()) for p in
          unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])

    print("PROBE2_EXISTS", unreal.EditorAssetLibrary.does_asset_exist(LEVEL))
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    print("PROBE2_LOAD_OK", loaded)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    print("PROBE2_WORLD_AFTER", world.get_path_name() if world else None)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    print("PROBE2_ACTOR_COUNT", len(actors))

    blueprint = unreal.EditorAssetLibrary.load_asset(BP)
    generated = blueprint.generated_class().get_path_name() if blueprint else None
    print("PROBE2_GENERATED_CLASS", generated)

    target_class_prefix = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3"

    rows = []
    for actor in actors:
        try:
            cls_path = actor.get_class().get_path_name()
        except Exception:
            cls_path = ""
        tags = [str(tag) for tag in actor.tags]
        label = actor.get_actor_label()
        is_target = cls_path.startswith(target_class_prefix)
        is_landmark = any(tag in TAGS for tag in tags) or label.startswith(
            ("Landmark_", "Light_", "Label_"))
        if is_target or is_landmark:
            rows.append({
                "label": label, "class": cls_path, "tags": tags,
                "is_target_blueprint": is_target,
                "location": [round(float(actor.get_actor_location().x), 2),
                             round(float(actor.get_actor_location().y), 2),
                             round(float(actor.get_actor_location().z), 2)],
            })
    print("PROBE2_INTERESTING", json.dumps(rows, indent=2))
    print("PROBE2_TARGET_ACTOR_COUNT",
          sum(1 for row in rows if row["is_target_blueprint"]))
    print("PROBE2_OK")


main()
