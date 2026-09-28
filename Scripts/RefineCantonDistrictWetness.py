"""Ground the provisional damp cards and make the local toggle legible."""
import json
import math
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
heights = memoryview(RAW.read_bytes()).cast("H")
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
started = time.monotonic()


def z_at(x, y):
    fx, fy = x / 200, y / 200
    col, row = math.floor(fx), math.floor(fy)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u)*(1-v)*sample(col,row) + u*(1-v)*sample(col+1,row) +
            (1-u)*v*sample(col,row+1) + u*v*sample(col+1,row+1))


def finish(_delta):
    if time.monotonic() - started < 22:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        material = assets.load_asset(
            "/Game/Canton/DistrictPrototype/Materials/M_District_Damp")
        assert material
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        tint = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant3Vector)
        tint.set_editor_property("constant", unreal.LinearColor(.035, .033, .030, 1))
        assert unreal.MaterialEditingLibrary.connect_material_property(
            tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant)
        rough.set_editor_property("r", .82)
        assert unreal.MaterialEditingLibrary.connect_material_property(
            rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        assert assets.save_loaded_asset(material)

        packages = set()
        count = 0
        for actor in actor_api.get_all_level_actors():
            if not actor.get_actor_label().startswith("District_LocalDamp_"):
                continue
            assert isinstance(actor, unreal.StaticMeshActor)
            point = actor.get_actor_location()
            scale = actor.get_actor_scale3d()
            actor.modify(True)
            actor.set_actor_scale3d(unreal.Vector(scale.x, scale.y, .01))
            actor.set_actor_location(unreal.Vector(point.x, point.y,
                                                   z_at(point.x, point.y) + 1.0),
                                     False, True)
            actor.static_mesh_component.set_cast_shadow(False)
            actor.static_mesh_component.set_material(0, material)
            actor.set_actor_enable_collision(False)
            packages.add(actor.get_package())
            count += 1
        assert count == 14, count
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        assert unreal.EditorLoadingAndSavingUtils.save_packages(list(packages), False)
        (OUT / "Wetness_Refinement.json").write_text(json.dumps({
            "wet_patches": count, "saved_actor_packages": len(packages),
            "card_thickness_cm": 1, "card_bottom_offset_cm": .5,
            "cast_shadow": False, "collision": False,
            "linear_base_color": [.035, .033, .030], "roughness": .82,
            "historical_wetness_claim": False,
        }, indent=2) + "\n")
        (OUT / "Wetness_Refinement_Error.txt").unlink(missing_ok=True)
    except Exception:
        (OUT / "Wetness_Refinement_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
