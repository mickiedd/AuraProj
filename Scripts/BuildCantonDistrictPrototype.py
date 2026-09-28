"""Create an isolated, explicitly nonhistorical Canton gate-district engineering test map."""
import hashlib
import json
import math
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
OUT.mkdir(parents=True, exist_ok=True)
CONTRACT = json.loads((ROOT / "Data/Canton_Prototype_Contract.json").read_text())
RAW = ROOT / CONTRACT["raw_height_path"]
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
assert hashlib.sha256(RAW.read_bytes()).hexdigest() == CONTRACT["raw_sha256"]
assert not CONTRACT["historically_accepted"]
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
started = time.monotonic()
tick_handle = None


def fail():
    error = traceback.format_exc()
    (OUT / "Build_Error.txt").write_text(error)
    unreal.log_error(error)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.SystemLibrary.quit_editor()


def make_material(name, color, roughness):
    folder = "/Game/Canton/DistrictPrototype/Materials"
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    assert material, name
    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector)
    tint.set_editor_property("constant", unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(
        tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant)
    rough.set_editor_property("r", roughness)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert assets.save_loaded_asset(material)
    return material


def z_at(x, y):
    fx = max(0.0, min(2016.0, x / 200))
    fy = max(0.0, min(2016.0, y / 200))
    col, row = math.floor(fx), math.floor(fy)
    next_col, next_row = min(2016, col + 1), min(2016, row + 1)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u) * (1-v) * sample(col, row) + u * (1-v) * sample(next_col, row) +
            (1-u) * v * sample(col, next_row) + u * v * sample(next_col, next_row))


def place(label, x, y, length_cm, width_cm, height_cm, material,
          yaw=0, surface_offset_cm=0, collision=True, cast_shadow=True):
    z = z_at(x, y) + surface_offset_cm + height_cm / 2
    dzdx = (z_at(x + length_cm / 2, y) - z_at(x - length_cm / 2, y)) / length_cm
    dzdy = (z_at(x, y + width_cm / 2) - z_at(x, y - width_cm / 2)) / width_cm
    rotation = unreal.Rotator(pitch=math.degrees(math.atan(dzdx)),
                              roll=-math.degrees(math.atan(dzdy)), yaw=yaw)
    actor = actor_api.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(x, y, z), rotation)
    assert actor, label
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, material)
    actor.static_mesh_component.set_cast_shadow(cast_shadow)
    actor.set_actor_scale3d(unreal.Vector(length_cm / 100, width_cm / 100, height_cm / 100))
    actor.set_actor_enable_collision(collision)
    actor.set_editor_property("tags", ["Canton.Provisional.DistrictImplementation"])
    return actor


def build_geometry():
    global cube
    cube = assets.load_asset("/Engine/BasicShapes/Cube")
    assert cube
    palette = {
        "stone": ((.36, .34, .30), .87),
        "earth": ((.24, .20, .16), .94),
        "pebble": ((.29, .27, .23), .90),
        "soil": ((.21, .18, .14), .97),
        "grass": ((.17, .23, .12), .96),
        "damp": ((.035, .033, .030), .82),
        "drain": ((.19, .20, .20), .81),
        "plot": ((.31, .24, .18), .92),
    }
    mats = {key: make_material("M_District_" + key.title(), *spec)
            for key, spec in palette.items()}
    for existing in actor_api.get_all_level_actors():
        if isinstance(existing, unreal.LandscapeProxy):
            existing.set_editor_property("landscape_material", mats["soil"])
    # Local prototype coordinates are a diagnostic sample of the modern-context raster.
    # They are not a surveyed location for Wenmingmen or any historical street.
    x0, y0 = 180000, 130000
    counts = {key: 0 for key in palette}
    # Two-metre segment cadence follows the 2 m Landscape sampling; slab top is
    # sampled independently at each segment. Thin mesh surfaces keep the base intact.
    for y in range(y0 - 9900, y0 + 10000, 200):
        place("District_Main_Stone_%d" % y, x0, y, 600, 200, 12, mats["stone"], surface_offset_cm=7)
        counts["stone"] += 1
        for side in (-1, 1):
            place("District_Main_EarthShoulder_%d_%d" % (side, y),
                  x0 + side * 425, y, 250, 200, 5, mats["earth"], surface_offset_cm=3)
            counts["earth"] += 1
    # Side lane intersects the principal axis without imposing a false period alignment.
    for x in range(x0 - 5900, x0 + 10000, 200):
        if abs(x - x0) < 400:
            continue
        place("District_MixedLane_%d" % x, x, y0 + 2000, 200, 320, 9,
              mats["pebble"], surface_offset_cm=5)
        counts["pebble"] += 1
    # Parcel bounds stay in GIS until local survey and grading evidence exist.
    # A covered surface gutter and lower-bound flow markers are engineering tests.
    for y in range(y0 - 9000, y0 + 10000, 400):
        place("District_CoveredGutter_%d" % y, x0 + 650, y,
              35, 400, 7, mats["drain"], surface_offset_cm=5)
        counts["drain"] += 1
    # Sparse deterministic edge growth; all points are outside the paved + gutter buffer.
    for i in range(40):
        side = -1 if i % 2 else 1
        x = x0 + side * (950 + (i % 4) * 160)
        y = y0 - 9300 + i * 460
        if abs(y - (y0 + 2000)) < 300:
            continue
        place("District_Weed_%02d" % i, x, y, 55, 55, 28,
              mats["grass"], surface_offset_cm=14, collision=False)
        counts["grass"] += 1
    for i in range(14):
        x = x0 + (900 if i % 2 else -900)
        y = y0 - 7600 + i * 1100
        if abs(y - (y0 + 2000)) < 300:
            continue
        place("District_LocalDamp_%02d" % i, x, y, 90, 130, 1,
              mats["damp"], surface_offset_cm=.5, collision=False,
              cast_shadow=False)
        counts["damp"] += 1
    # The named gate asset is a scale/interface stand-in, not a georeferenced placement.
    gate_class = unreal.load_class(None,
        "/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen/BP_Wenmingmen.BP_Wenmingmen_C")
    assert gate_class, "Wenmingmen Blueprint unavailable"
    gate = actor_api.spawn_actor_from_class(gate_class,
        unreal.Vector(x0, y0 - 8500, z_at(x0, y0 - 8500) + 333),
        unreal.Rotator())
    assert gate
    gate.set_actor_label("Wenmingmen_PROVISIONAL_Scale_Interface_Only")
    gate.set_editor_property("tags", ["Canton.Provisional.GateScaleOnly"])
    sun = actor_api.spawn_actor_from_class(unreal.DirectionalLight,
        unreal.Vector(0, 0, 30000), unreal.Rotator(pitch=-42, yaw=-35))
    sun.light_component.set_intensity(4)
    fill = actor_api.spawn_actor_from_class(unreal.DirectionalLight,
        unreal.Vector(0, 0, 30000), unreal.Rotator(pitch=-50, yaw=140))
    fill.light_component.set_intensity(2)
    fill.light_component.set_editor_property("cast_shadows", False)
    actor_api.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator())
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(x0 - 22000, y0 - 26000, 17000),
        unreal.Rotator(pitch=-23, yaw=47))
    return counts


def complete(_delta):
    global tick_handle
    if time.monotonic() - started < 45:
        return
    unreal.unregister_slate_post_tick_callback(tick_handle)
    try:
        assert unreal.CantonTerrainLibrary.add_district_edit_layers(world)
        counts = build_geometry()
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        packages = [p for p in packages if "/Canton/DistrictPrototype/" in p.get_name()]
        if packages:
            assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
        (OUT / "Build_Result.json").write_text(json.dumps({
            "map": MAP, "historically_accepted": False,
            "evidence_stage": "INITIAL_PRE_REFINEMENT_SNAPSHOT",
            "superseded_for_current_counts_by": "QA/Canton_District/Reload_Validation.json",
            "diagnostic_center_cm": [180000, 130000],
            "district_size_m": [200, 200], "instance_counts": counts,
            "gate_asset": "/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen/BP_Wenmingmen",
            "edit_layers": ["Base_Imported", "Urban_Grading", "Road_Corridors", "Drainage"],
            "edit_layers_are_unmodified": True,
        }, indent=2) + "\n")
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
    except Exception:
        fail()


try:
    assert not assets.does_asset_exist(MAP), "District map already exists; refusing overwrite"
    world = unreal.CantonTerrainLibrary.create_district_world()
    assert world, "WP world creation failed"
    assert unreal.CantonTerrainLibrary.import_provisional_terrain(world, str(RAW))
    heights = memoryview(RAW.read_bytes()).cast("H")
    tick_handle = unreal.register_slate_post_tick_callback(complete)
except Exception:
    fail()
