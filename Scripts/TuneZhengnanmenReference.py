"""Tune the Zhengnanmen V3 actor toward the supplied Great South Gate reference.

The source model already has the correct three-level silhouette, roof ornaments,
signboard, arched gate and collision.  The visible facade reads hollow from the
front and side because the lattice is backed by the preview background.  This
pass adds shallow dark timber infill panels behind each level's lattice on both
facades and side walls, keeping the existing source components and collision
unchanged.  The Blueprint is the authority, so the preview map and showcase
instance inherit the same repair.
"""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "Saved/RawModelImport/V3"
BP_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3"
PREVIEW_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/L_Zhengnanmen_AAA_V3_Preview"
SHOWCASE_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
REPORT_PATH = RAW / "Zhengnanmen_AAA_V3-reference-tuning.json"
PREFIX = "ReferenceInfill_"
MATERIAL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/Materials/M_DoorWood"


PANELS = (
    # name, location (cm), dimensions (cm)
    ("L0_F", (0.0, 585.0, 925.0), (2240.0, 8.0, 255.0)),
    ("L0_B", (0.0, -585.0, 925.0), (2240.0, 8.0, 255.0)),
    ("L0_L", (-1140.0, 0.0, 925.0), (8.0, 1120.0, 255.0)),
    ("L0_R", (1140.0, 0.0, 925.0), (8.0, 1120.0, 255.0)),
    ("L1_F", (0.0, 555.0, 1295.0), (2140.0, 8.0, 270.0)),
    ("L1_B", (0.0, -555.0, 1295.0), (2140.0, 8.0, 270.0)),
    ("L1_L", (-1110.0, 0.0, 1295.0), (8.0, 1060.0, 270.0)),
    ("L1_R", (1110.0, 0.0, 1295.0), (8.0, 1060.0, 270.0)),
    ("L2_F", (0.0, 525.0, 1665.0), (1960.0, 8.0, 300.0)),
    ("L2_B", (0.0, -525.0, 1665.0), (1960.0, 8.0, 300.0)),
    ("L2_L", (-1035.0, 0.0, 1665.0), (8.0, 980.0, 300.0)),
    ("L2_R", (1035.0, 0.0, 1665.0), (8.0, 980.0, 300.0)),
)


def p(obj):
    return obj.get_path_name() if obj else ""


def component_handles(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    values = []
    root = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if library.is_root_component(data):
            root = handle
        if library.is_component(data):
            component = library.get_object(data)
            if component:
                values.append((handle, component))
    assert root, "Blueprint has no root component"
    return subsystem, library, root, values


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


def update_reports(visible_count):
    import_report_path = RAW / "Zhengnanmen_AAA_V3-import.json"
    import_report = json.loads(import_report_path.read_text(encoding="utf-8"))
    import_report["visible_components"] = visible_count
    import_report["reference_tuning"] = {
        "added_visible_components": visible_count - 7,
        "panel_count": len(PANELS),
        "material": MATERIAL_PATH,
        "collision_unchanged": True,
    }
    import_report_path.write_text(json.dumps(import_report, indent=2), encoding="utf-8")

    placement_path = RAW / "v3-level-placement.json"
    placement = json.loads(placement_path.read_text(encoding="utf-8"))
    for actor in placement["actors"]:
        if actor["name"] == "Zhengnanmen_AAA_V3":
            actor["visible_components"] = visible_count
            actor["reference_tuning"] = {
                "panel_count": len(PANELS),
                "facades": ["front", "rear", "left", "right"],
            }
    placement_path.write_text(json.dumps(placement, indent=2), encoding="utf-8")


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    assert bp and bp.generated_class(), BP_PATH
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    assert cube and material, (p(cube), p(material))

    subsystem, library, root, components = component_handles(bp)
    existing = {component.get_name(): component for _handle, component in components}
    created = []
    for name, location, dimensions in PANELS:
        if any(component.get_name().startswith(PREFIX + name) for component in existing.values()):
            continue
        created.append(add_panel(bp, subsystem, library, root, name, location, dimensions, cube, material))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), BP_PATH
    # Reload the preview and showcase maps so serialized instances are checked
    # through the same Blueprint class used by the placement validator.  The
    # subobject-data API can return duplicate template handles, so counts come
    # from instantiated actors rather than the template handle list.
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    preview_actor = next(
        actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        if actor.get_actor_label() == "Preview_Zhengnanmen_AAA_V3"
    )
    preview_components = preview_actor.get_components_by_class(unreal.StaticMeshComponent)
    visible_count = sum(1 for c in preview_components if c.get_editor_property("visible"))
    collision_count = sum(1 for c in preview_components if not c.get_editor_property("visible"))
    assert visible_count == 7 + len(PANELS)
    assert level_editor.load_level(SHOWCASE_PATH), SHOWCASE_PATH
    showcase_actor = next(
        actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        if actor.get_actor_label() == "GuangzhouLandmark_Zhengnanmen_AAA_V3"
    )
    showcase_components = showcase_actor.get_components_by_class(unreal.StaticMeshComponent)
    assert sum(1 for c in showcase_components if c.get_editor_property("visible")) == visible_count
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, SHOWCASE_PATH)
    update_reports(visible_count)

    report = {
        "passed": True,
        "blueprint": BP_PATH,
        "preview_level": PREVIEW_PATH,
        "showcase_level": SHOWCASE_PATH,
        "added_panel_count": len(created),
        "visible_components": visible_count,
        "collision_components": collision_count,
        "panel_names": [component.get_name() for component in created],
        "material": MATERIAL_PATH,
        "panel_dimensions_cm": {PREFIX + name: list(dimensions) for name, _location, dimensions in PANELS},
    }
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("ZHENGNANMEN_REFERENCE_TUNING_COMPLETE", json.dumps(report))


if __name__ == "__main__":
    main()
