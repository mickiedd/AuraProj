"""Author the open Dadongmen gate leaves and their material/detail layer.

The source mesh is the open-door variant, so the former baked closed leaves are
gone.  This pass places paired timber leaves on their vertical hinges inside
the tunnel, rotates all plank/brace/hardware detail with each leaf, and adds a
stone tunnel floor to close the visual gap at the threshold.  Components are
visual-only and use NoCollision so the authored collision proxy remains intact.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
RAW = PROJECT / "Saved/RawModelImport/V4"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen"
BP_PATH = DEST + "/BP_Dadongmen_V4"
PREVIEW_PATH = DEST + "/L_Dadongmen_V4_Preview"
REPORT_PATH = RAW / "Dadongmen-detail-tuning.json"
PREFIX = "Detail_Dadongmen_Door_"
WOOD_PATH = DEST + "/Materials/M_Dadongmen_Wood_ReferenceTuned"
STONE_PATH = DEST + "/Materials/M_Dadongmen_Stone_ReferenceTuned"
IRON_PATH = DEST + "/Materials/M_Dadongmen_Iron_ReferenceTuned"
DOOR_WOOD_PATH = DEST + "/Materials/M_Dadongmen_DoorWood_ReferenceTuned"
APPROACH_PATH = DEST + "/Materials/M_Dadongmen_Approach_ReferenceTuned"
APPROACH_WATER_PATH = DEST + "/Materials/M_Dadongmen_ApproachWater_ReferenceTuned"
SOURCE_DIRT_PATH = DEST + "/Materials/M_Dadongmen_Dirt_ReferenceTuned"
SOURCE_WATER_PATH = DEST + "/Materials/M_Dadongmen_Water_ReferenceTuned"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
SPHERE_PATH = "/Engine/BasicShapes/Sphere.Sphere"


def asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def make_iron_material():
    material = unreal.EditorAssetLibrary.load_asset(IRON_PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Dadongmen_Iron_ReferenceTuned",
            DEST + "/Materials",
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    assert material
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    base = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property("constant", unreal.LinearColor(0.025, 0.019, 0.014, 1.0))
    metallic = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 180)
    metallic.set_editor_property("r", 0.68)
    roughness = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 360)
    roughness.set_editor_property("r", 0.42)
    assert lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert lib.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    assert lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    glow = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 540)
    glow.set_editor_property("constant", unreal.LinearColor(0.012, 0.004, 0.001, 1.0))
    assert lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def make_constant_material(path, name, color, roughness=0.78, metallic=0.0, emissive=None, translucent=False, opacity=1.0):
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew()
        )
    assert material
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    base = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -420, 0)
    base.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -420, 190)
    rough.set_editor_property("r", roughness)
    assert lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if metallic:
        metal = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -420, 380)
        metal.set_editor_property("r", metallic)
        assert lib.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    if emissive:
        glow = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -420, 570)
        glow.set_editor_property("constant", unreal.LinearColor(*emissive, 1.0))
        assert lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        alpha = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -420, 760)
        alpha.set_editor_property("r", opacity)
        assert lib.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    else:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def hide_source_ground_material(material, name):
    """Make the two procedural ground slabs invisible without editing the source mesh."""
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    alpha = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -420, 0)
    alpha.set_editor_property("r", 0.0)
    assert lib.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_nanite", True)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    print("HIDDEN_SOURCE_GROUND", name, material.get_path_name())


def configure(component, mesh, material, location, dimensions, rotation=(0.0, 0.0, 0.0)):
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property("relative_location", unreal.Vector(*location))
    component.set_editor_property(
        "relative_rotation", unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
    )
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


def add_component(bp, subsystem, library, root, name, mesh, material, location, dimensions, rotation=(0.0, 0.0, 0.0), existing=None):
    current = next((c for c in (existing or {}).values() if c.get_name().startswith(PREFIX + name)), None)
    if current:
        return configure(current, mesh, material, location, dimensions, rotation), False
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
    return configure(component, mesh, material, location, dimensions, rotation), True


def _rotate_local(center, yaw, local_x, local_y):
    """Convert a panel-local XY offset to source-space centimeters."""
    import math

    radians = math.radians(yaw)
    c, s = math.cos(radians), math.sin(radians)
    return (center[0] + c * local_x - s * local_y, center[1] + s * local_x + c * local_y)


def detail_specs():
    """Return named components in centimeters for an open, hinged gate."""
    specs = []
    # The opening is 5.8 m wide.  Each 2.7 m leaf rotates 90 degrees around
    # the outer jamb, tucking into the tunnel instead of blocking the arch.
    for face, hinge_y, inward in (("Front", 840.0, -1.0), ("Rear", -840.0, 1.0)):
        panels = {}
        for side, hinge_x, yaw in (("L", -273.0, -90.0), ("R", 273.0, 90.0)):
            center = (hinge_x, hinge_y + inward * 135.0)
            panels[side] = (center, yaw)
            specs.append((face + "_Leaf_" + side, "door_wood", (center[0], center[1], 210.0), (270.0, 14.0, 410.0), yaw))
            for i, local_x in enumerate((-102.0, -51.0, 0.0, 51.0, 102.0)):
                px, py = _rotate_local(center, yaw, local_x, inward * 8.0)
                specs.append((face + "_Plank_" + side + "_" + str(i), "door_wood", (px, py, 210.0), (42.0, 8.0, 394.0), yaw))
            for brace, z in (("Low", 102.0), ("High", 318.0)):
                bx, by = _rotate_local(center, yaw, 0.0, inward * 10.0)
                specs.append((face + "_Brace_" + side + "_" + brace, "door_wood", (bx, by, z), (270.0, 10.0, 34.0), yaw))
        # Three strap/stud columns are distributed across the two open leaves.
        for i, (side, local_x) in enumerate((("L", -72.0), ("L", 72.0), ("R", 0.0))):
            center, yaw = panels[side]
            sx, sy = _rotate_local(center, yaw, local_x, inward * 15.0)
            specs.append((face + "_IronStrap_" + str(i), "iron", (sx, sy, 210.0), (15.0, 8.0, 382.0), yaw))
            for j, z in enumerate((104.0, 210.0, 316.0)):
                dx, dy = _rotate_local(center, yaw, local_x, inward * 21.0)
                specs.append((face + "_Stud_" + str(i) + "_" + str(j), "iron", (dx, dy, z), (22.0, 18.0, 22.0), yaw))
    # A shallow stone sill grounds the door leaves and follows the reference's
    # worn threshold without changing the imported collision proxy.
    specs.append(("Threshold", "stone", (0.0, 0.0, 18.0), (600.0, 80.0, 36.0), 0.0))
    specs.append(("Interior_Floor", "stone", (0.0, 0.0, 4.0), (580.0, 1800.0, 8.0), 0.0))
    # The imported source contains 20 m x 12 m dirt and 18 m x 8 m water
    # slabs.  The reference reads as a narrow approach with a small water
    # edge, so replace their presentation with compact package-local pieces.
    specs.append(("Approach_Path", "approach", (700.0, -1280.0, -5.0), (1200.0, 900.0, 10.0), 0.0))
    specs.append(("Approach_Water", "approach_water", (-950.0, -1240.0, -7.0), (850.0, 520.0, 8.0), 0.0))
    return specs


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before Dadongmen door tuning: " + str(dirty)
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    bp = asset(BP_PATH)
    cube = asset(CUBE_PATH)
    sphere = asset(SPHERE_PATH)
    wood = asset(WOOD_PATH)
    stone = asset(STONE_PATH)
    iron = make_iron_material()
    door_wood = make_constant_material(
        DOOR_WOOD_PATH,
        "M_Dadongmen_DoorWood_ReferenceTuned",
        (0.22, 0.085, 0.03),
        roughness=0.72,
        emissive=(0.12, 0.022, 0.005),
    )
    approach = make_constant_material(
        APPROACH_PATH,
        "M_Dadongmen_Approach_ReferenceTuned",
        (0.36, 0.25, 0.14),
        roughness=0.93,
    )
    approach_water = make_constant_material(
        APPROACH_WATER_PATH,
        "M_Dadongmen_ApproachWater_ReferenceTuned",
        (0.035, 0.12, 0.13),
        roughness=0.28,
        translucent=True,
        opacity=0.62,
    )
    hide_source_ground_material(asset(SOURCE_DIRT_PATH), "Dirt")
    hide_source_ground_material(asset(SOURCE_WATER_PATH), "Water")
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

    materials = {
        "wood": wood,
        "door_wood": door_wood,
        "stone": stone,
        "iron": iron,
        "approach": approach,
        "approach_water": approach_water,
    }
    created = 0
    for name, kind, location, dimensions, rotation in detail_specs():
        mesh = sphere if "Stud" in name else cube
        # The glTF importer's axis conversion is already baked into the
        # imported mesh and the Blueprint component.  Keep authored source
        # coordinates so the overlay follows the visible façade and tunnel
        # depth directly in the Blueprint root space.
        world_location = location
        component, was_created = add_component(
            bp,
            subsystem,
            library,
            root,
            name,
            mesh,
            materials[kind],
            world_location,
            dimensions,
            rotation=(0.0, rotation, 0.0),
            existing=existing,
        )
        existing[component.get_name()] = component
        created += int(was_created)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), BP_PATH

    # Read back the saved Blueprint instance so the report records what the
    # player-facing asset actually contains rather than only template handles.
    assert level_editor.load_level(PREVIEW_PATH), PREVIEW_PATH
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    preview_actor = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == "Preview_Dadongmen_V4")
    components = preview_actor.get_components_by_class(unreal.StaticMeshComponent)
    detail_components = [c for c in components if c.get_name().startswith(PREFIX)]
    assert len(detail_components) == len(detail_specs()), (len(detail_components), len(detail_specs()))
    for component in detail_components:
        assert str(component.get_collision_profile_name()) == "NoCollision", component.get_name()
        assert component.get_material(0), component.get_name()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, PREVIEW_PATH)

    report = {
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-5ea059eb-7975-4c7f-9156-c4dddb1d47d8.png",
        "latest_visual_reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-d1ab45f2-8f83-4b07-8210-8bf81a29ca2d.png",
        "ground_reference_intent": "Replace the two oversized imported dirt/water slabs with a compact approach and water edge.",
        "blueprint": BP_PATH,
        "preview_level": PREVIEW_PATH,
        "source_mesh": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Meshes/SM_Dadongmen_OpenDoor_LOD0/SM_Dadongmen_OpenDoor_LOD0",
        "source_mesh_rollback": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Meshes/SM_Dadongmen_LOD0/SM_Dadongmen_LOD0",
        "added_detail_components": len(detail_components),
        "created_this_run": created,
        "detail_prefix": PREFIX,
        "materials": {kind: materials[kind].get_path_name() for kind in materials},
        "components": {
            PREFIX + name: {
                "material_group": kind,
                "source_location_cm": list(location),
                "blueprint_location_cm": list(location),
                "dimensions_cm": list(dimensions),
                "rotation": [0.0, rotation, 0.0],
            }
            for name, kind, location, dimensions, rotation in detail_specs()
        },
        "collision_profile": "NoCollision",
        "geometry_changed": True,
        "intent": "Open the paired Dadongmen timber leaves onto their outer jamb hinges, keep the gate passage clear, and add a continuous stone tunnel floor while preserving the original collision proxy for rollback.",
    }
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("DADONGMEN_DOOR_DETAIL_COMPLETE", json.dumps({"components": len(detail_components), "created": created, "blueprint": BP_PATH, "report": str(REPORT_PATH)}))


if __name__ == "__main__":
    main()
