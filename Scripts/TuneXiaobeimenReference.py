"""Tune the Xiaobeimen V3 Small North Gate toward the supplied reference.

The source model already carries the two-level roof, brackets, plaque, arched
door, stonework, moss, flags and collision.  Its timber lattice reads hollow
in the preview because there is no dark backing behind the openings.  This
pass adds shallow package-local aged-wood infill behind both tower levels on
all four facades, leaving the authored source geometry and collision intact.
"""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "Saved/RawModelImport/V3"
BP_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3"
PREVIEW_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/L_Xiaobeimen_AAA_V3_Preview"
SHOWCASE_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
REPORT_PATH = RAW / "Xiaobeimen_AAA_V3-reference-tuning.json"
PREFIX = "ReferenceInfill_"
MATERIAL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/Materials/M_AgedWood"

# Coordinates are in the Blueprint's authored centimeter space.  The two
# levels follow the Window_Lattice bounds (roughly z 1.0–1.7 m); the 8 cm
# backing sits behind the lattice and leaves the center plaque and gate clear.
PANELS = (
    ("L0_F", (0.0, 300.0, 1190.0), (1450.0, 8.0, 340.0)),
    ("L0_B", (0.0, -300.0, 1190.0), (1450.0, 8.0, 340.0)),
    ("L0_L", (-700.0, 0.0, 1190.0), (8.0, 560.0, 340.0)),
    ("L0_R", (700.0, 0.0, 1190.0), (8.0, 560.0, 340.0)),
    ("L1_F", (0.0, 300.0, 1545.0), (1450.0, 8.0, 340.0)),
    ("L1_B", (0.0, -300.0, 1545.0), (1450.0, 8.0, 340.0)),
    ("L1_L", (-700.0, 0.0, 1545.0), (8.0, 560.0, 340.0)),
    ("L1_R", (700.0, 0.0, 1545.0), (8.0, 560.0, 340.0)),
)


def configure_panel(component, location, dimensions, cube, material):
    component.set_static_mesh(cube)
    component.set_material(0, material)
    component.set_editor_property("relative_location", unreal.Vector(*location))
    component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    component.set_editor_property(
        "relative_scale3d",
        unreal.Vector(dimensions[0] / 100.0, dimensions[1] / 100.0, dimensions[2] / 100.0),
    )
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("visible", True)
    component.set_editor_property("hidden_in_game", False)
    component.set_editor_property("cast_shadow", True)
    return component


def add_panel(bp, subsystem, library, root, name, location, dimensions, cube, material):
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", root)
    params.set_editor_property("new_class", unreal.StaticMeshComponent)
    params.set_editor_property("blueprint_context", bp)
    params.set_editor_property("conform_transform_to_parent", False)
    handle, reason = subsystem.add_new_subobject(params)
    assert library.is_handle_valid(handle), str(reason)
    subsystem.rename_subobject(handle, unreal.Text(PREFIX + name))
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    component = library.get_object(data)
    assert component, name
    return configure_panel(component, location, dimensions, cube, material)


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    assert bp and bp.generated_class() and cube and material, (BP_PATH, MATERIAL_PATH)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    root = None
    existing = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if library.is_root_component(data):
            root = handle
        if library.is_component(data):
            component = library.get_object(data)
            if component:
                existing[component.get_name()] = component
    assert root, "Blueprint has no root component"

    created = []
    for name, location, dimensions in PANELS:
        current = next((component for component in existing.values()
                        if component.get_name().startswith(PREFIX + name)), None)
        if current:
            configure_panel(current, location, dimensions, cube, material)
            continue
        created.append(add_panel(bp, subsystem, library, root, name, location, dimensions, cube, material))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), BP_PATH

    # Counts come from spawned instances: SubobjectData can expose duplicate
    # template handles for imported components.
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    preview_actor = next(actor for actor in actors if actor.get_actor_label() == "Preview_Xiaobeimen_AAA_V3")
    preview_components = preview_actor.get_components_by_class(unreal.StaticMeshComponent)
    visible_count = sum(1 for c in preview_components if c.get_editor_property("visible"))
    collision_count = sum(1 for c in preview_components if not c.get_editor_property("visible"))
    assert visible_count == 38 + len(PANELS), (visible_count, len(PANELS))
    assert collision_count == 5, collision_count
    preview_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(preview_world, PREVIEW_PATH)

    assert level_editor.load_level(SHOWCASE_PATH), SHOWCASE_PATH
    showcase_actor = next(
        actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        if actor.get_actor_label() == "GuangzhouLandmark_Xiaobeimen_AAA_V3"
    )
    showcase_components = showcase_actor.get_components_by_class(unreal.StaticMeshComponent)
    assert sum(1 for c in showcase_components if c.get_editor_property("visible")) == visible_count
    assert sum(1 for c in showcase_components if not c.get_editor_property("visible")) == collision_count
    showcase_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(showcase_world, SHOWCASE_PATH)

    import_report_path = RAW / "Xiaobeimen_AAA_V3-import.json"
    import_report = json.loads(import_report_path.read_text(encoding="utf-8"))
    import_report["visible_components"] = visible_count
    import_report["reference_tuning"] = {
        "added_visible_components": len(PANELS),
        "panel_count": len(PANELS),
        "levels": 2,
        "facades": ["front", "rear", "left", "right"],
        "material": MATERIAL_PATH,
        "collision_unchanged": True,
    }
    import_report_path.write_text(json.dumps(import_report, indent=2), encoding="utf-8")

    placement_path = RAW / "v3-level-placement.json"
    placement = json.loads(placement_path.read_text(encoding="utf-8"))
    for actor in placement["actors"]:
        if actor["name"] == "Xiaobeimen_AAA_V3":
            actor["visible_components"] = visible_count
            actor["reference_tuning"] = {
                "panel_count": len(PANELS),
                "levels": 2,
                "facades": ["front", "rear", "left", "right"],
            }
    placement_path.write_text(json.dumps(placement, indent=2), encoding="utf-8")

    report = {
        "passed": True,
        "blueprint": BP_PATH,
        "preview_level": PREVIEW_PATH,
        "showcase_level": SHOWCASE_PATH,
        "added_panel_count": len(created),
        "visible_components": visible_count,
        "collision_components": collision_count,
        "panel_names": [PREFIX + name for name, _location, _dimensions in PANELS],
        "material": MATERIAL_PATH,
        "panel_dimensions_cm": {PREFIX + name: list(dimensions) for name, _location, dimensions in PANELS},
    }
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("XIAOBEIMEN_REFERENCE_TUNING_COMPLETE", json.dumps(report))


if __name__ == "__main__":
    main()
