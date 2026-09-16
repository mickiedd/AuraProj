"""Add textured structural infill to the South Gate preview level.

The supplied GLB already contains the stone base, roof shells, columns,
railings, brackets and repeated decorative pieces. Its three timber floors
also contain dark core volumes, but the perimeter reads hollow because the
façade is mostly an open frame. This script adds shallow modular wall skins,
filled window panels and side infill behind the imported detail. It is
idempotent and only edits the isolated high-fidelity preview level.
"""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
REPORT_PATH = Path("C:/Git/AuraProj/Saved/RawModelImport/GreatSouthGate_Zhengnanmen_IntactEnhancement.json")
LABEL_PREFIX = "Zhengnanmen_IntactFill_"

# World-space centimetres, matching the imported scene bounds.
LEVELS = (
    {"index": 0, "half_width": 1100.0, "half_depth": 605.0, "center_z": 960.0, "height": 255.0},
    {"index": 1, "half_width": 995.0, "half_depth": 550.0, "center_z": 1327.5, "height": 280.0},
    {"index": 2, "half_width": 895.0, "half_depth": 495.0, "center_z": 1725.0, "height": 335.0},
)


def imported_asset(name):
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        if path.rsplit("/", 1)[-1].split(".", 1)[0] == name:
            return unreal.EditorAssetLibrary.load_asset(path)
    raise RuntimeError("Imported asset not found: " + name)


def delete_previous_fill(actors):
    old = [actor for actor in actors if actor.get_actor_label().startswith(LABEL_PREFIX)]
    if old:
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actors(old)
    return len(old)


def spawn_cube(cube, material, label, location, size):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_label(label)
    # Editor Python spawns default to editor-only in this project. Clear that
    # flag so the structural infill is present in PIE and scene captures.
    try:
        actor.set_editor_property("is_editor_only_actor", False)
    except Exception:
        pass
    actor.set_actor_hidden_in_game(False)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(cube)
    component.set_material(0, material)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_collision_profile_name("NoCollision")
    return actor


assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH), LEVEL_PATH
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors_before = actor_subsystem.get_all_level_actors()
removed_count = delete_previous_fill(actors_before)

cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
assert cube, "Engine cube mesh is unavailable"
red = imported_asset("M_Wood_RedLacquer")
dark = imported_asset("M_Wood_DarkAged")
assert red and dark, "Imported timber materials are unavailable"

# The imported cores are already solid volumes. Recolor their exposed timber
# surfaces so they no longer read as black voids between the roofs.
core_material_overrides = 0
for actor in actor_subsystem.get_all_level_actors():
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or not component.static_mesh:
        continue
    if component.static_mesh.get_name() in {"N_FloorCore_0", "N_FloorCore_1", "N_FloorCore_2"}:
        component.set_material(0, red)
        core_material_overrides += 1

created = []
for level in LEVELS:
    i = level["index"]
    hw = level["half_width"]
    hd = level["half_depth"]
    z = level["center_z"]
    h = level["height"]

    # Nine front/back bays sit behind the ten imported columns. Each dark
    # window is a solid shallow panel, not an actual hole.
    bay_width = (2.0 * hw) / 9.0
    panel_height = h - 42.0
    window_height = h - 96.0
    for side in (-1.0, 1.0):
        face_y = side * (hd + 8.0)
        window_y = side * (hd + 18.0)
        for bay in range(9):
            x = -hw + bay_width * (bay + 0.5)
            created.append(
                spawn_cube(
                    cube,
                    red,
                    f"{LABEL_PREFIX}L{i}_Facade_{'F' if side > 0 else 'B'}_{bay:02d}",
                    (x, face_y, z),
                    (bay_width - 14.0, 14.0, panel_height),
                )
            )
            created.append(
                spawn_cube(
                    cube,
                    dark,
                    f"{LABEL_PREFIX}L{i}_Window_{'F' if side > 0 else 'B'}_{bay:02d}",
                    (x, window_y, z + 3.0),
                    (bay_width - 62.0, 5.0, window_height),
                )
            )

        # Continuous sill and head members close remaining horizontal seams
        # and tie the panels into the original beams.
        for trim_name, trim_z in (
            ("Sill", z - h * 0.5 + 18.0),
            ("Head", z + h * 0.5 - 18.0),
        ):
            created.append(
                spawn_cube(
                    cube,
                    red,
                    f"{LABEL_PREFIX}L{i}_{trim_name}_{'F' if side > 0 else 'B'}",
                    (0.0, side * (hd + 13.0), trim_z),
                    (2.0 * hw + 30.0, 24.0, 24.0),
                )
            )

    # Side infill closes the two end walls while retaining imported columns
    # and brackets in front of it.
    side_bay_width = (2.0 * hd) / 5.0
    for side in (-1.0, 1.0):
        face_x = side * (hw + 8.0)
        window_x = side * (hw + 18.0)
        for bay in range(5):
            y = -hd + side_bay_width * (bay + 0.5)
            created.append(
                spawn_cube(
                    cube,
                    red,
                    f"{LABEL_PREFIX}L{i}_Side_{'R' if side > 0 else 'L'}_{bay:02d}",
                    (face_x, y, z),
                    (14.0, side_bay_width - 14.0, panel_height),
                )
            )
            created.append(
                spawn_cube(
                    cube,
                    dark,
                    f"{LABEL_PREFIX}L{i}_SideWindow_{'R' if side > 0 else 'L'}_{bay:02d}",
                    (window_x, y, z + 3.0),
                    (5.0, side_bay_width - 62.0, window_height),
                )
            )

        for trim_name, trim_z in (
            ("Sill", z - h * 0.5 + 18.0),
            ("Head", z + h * 0.5 - 18.0),
        ):
            created.append(
                spawn_cube(
                    cube,
                    red,
                    f"{LABEL_PREFIX}L{i}_{trim_name}_{'R' if side > 0 else 'L'}",
                    (side * (hw + 13.0), 0.0, trim_z),
                    (24.0, 2.0 * hd + 30.0, 24.0),
                )
            )

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Failed to save enhanced preview level"

report = {
    "level": LEVEL_PATH,
    "destination": DEST,
    "removed_previous_fill_actors": removed_count,
    "created_fill_actors": len(created),
    "core_material_overrides": core_material_overrides,
    "design": {
        "front_back_bays_per_level": 9,
        "side_bays_per_side_per_level": 5,
        "window_panels_are_solid": True,
        "materials": {"structure": red.get_path_name(), "window_insets": dark.get_path_name()},
        "collision": "disabled_on_fill_panels",
    },
    "passed": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
print("GREAT_SOUTH_GATE_ZHENGNANMEN_INTACT_ENHANCEMENT", report)
