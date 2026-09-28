"""Create six editable local ground layers while preserving a neutral soil fallback."""
import json
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
MATERIAL = "/Game/Canton/DistrictPrototype/Terrain/M_Canton_Landscape_PROVISIONAL"
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.monotonic()


def finish(_delta):
    if time.monotonic() - started < 25:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        assets = unreal.EditorAssetLibrary
        assert not assets.does_asset_exist(MATERIAL), "Landscape material already exists"
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Canton_Landscape_PROVISIONAL", "/Game/Canton/DistrictPrototype/Terrain",
            unreal.Material, unreal.MaterialFactoryNew())
        assert mat
        surfaces = [
            ("Soil", (.21, .18, .14)),
            ("Earth", (.24, .20, .16)),
            ("Pebble", (.29, .27, .23)),
            ("Grass", (.17, .23, .12)),
            ("Damp", (.13, .12, .10)),
            ("Stone", (.36, .34, .30)),
        ]
        previous = None
        for name, color in surfaces:
            layer = unreal.MaterialEditingLibrary.create_material_expression(
                mat, unreal.MaterialExpressionLandscapeLayerWeight)
            layer.set_editor_property("parameter_name", name)
            layer.set_editor_property("const_base", unreal.Vector(*surfaces[0][1]))
            layer.set_editor_property("preview_weight", 0.0)
            swatch = unreal.MaterialEditingLibrary.create_material_expression(
                mat, unreal.MaterialExpressionConstant3Vector)
            swatch.set_editor_property("constant", unreal.LinearColor(*color, 1))
            assert unreal.MaterialEditingLibrary.connect_material_expressions(swatch, "", layer, "Layer")
            if previous is not None:
                assert unreal.MaterialEditingLibrary.connect_material_expressions(previous, "", layer, "Base")
            previous = layer
        assert unreal.MaterialEditingLibrary.connect_material_property(
            previous, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant)
        rough.set_editor_property("r", .92)
        assert unreal.MaterialEditingLibrary.connect_material_property(
            rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        assert unreal.CantonTerrainLibrary.configure_district_landscape_material(world, mat)
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        packages = [p for p in packages if "/Canton/DistrictPrototype/" in p.get_name()]
        if packages:
            assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
        (OUT / "Landscape_Material.json").write_text(json.dumps({
            "material": MATERIAL, "paintable_layers": [name for name, _ in surfaces],
            "default_unpainted_color": "Soil", "roughness": .92,
            "textures": "none; source-neutral swatches",
            "historical_color_claim": False,
        }, indent=2) + "\n")
    except Exception:
        (OUT / "Material_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
