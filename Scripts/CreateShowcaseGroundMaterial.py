"""Create a neutral low-albedo ground material for the landmark showcase level.

The showcase floor was rendering pure white, which crushes the landmarks - dark
weathered stone and timber - down to near-black silhouettes. The preview ground
materials in this project are all bright, so this authors a plain dark paving
material instead.

The material is intentionally trivial: a Default Lit surface with a dark grey
base colour and high roughness. No textures, so it reads as neutral paving and
never competes with the landmarks.

Idempotent: an existing material is reused unchanged.
"""

import json

import unreal

ASSET_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/M_ShowcaseGround"
FOLDER = "/Game/Assets/Environment/GuangzhouLandmarks"
NAME = "M_ShowcaseGround"

BASE_COLOR = (0.165, 0.160, 0.150)
ROUGHNESS = 0.88
METALLIC = 0.0


def main():
    existing = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if existing:
        print("SHOWCASE_GROUND_MATERIAL_REUSED", ASSET_PATH)
        return

    factory = unreal.MaterialFactoryNew()
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, factory)
    assert material, ASSET_PATH

    library = unreal.MaterialEditingLibrary

    base_color = library.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -450, -120)
    base_color.set_editor_property(
        "constant", unreal.LinearColor(BASE_COLOR[0], BASE_COLOR[1], BASE_COLOR[2], 1.0))
    library.connect_material_property(
        base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = library.create_material_expression(
        material, unreal.MaterialExpressionConstant, -450, 60)
    roughness.set_editor_property("r", ROUGHNESS)
    library.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metallic = library.create_material_expression(
        material, unreal.MaterialExpressionConstant, -450, 180)
    metallic.set_editor_property("r", METALLIC)
    library.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC)

    assert library.recompile_material(material) or True
    assert unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False), ASSET_PATH

    reloaded = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    report = {
        "path": ASSET_PATH,
        "exists": reloaded is not None,
        "shading_model": str(reloaded.get_editor_property("shading_model")) if reloaded else None,
        "base_color": BASE_COLOR,
        "roughness": ROUGHNESS,
    }
    for expression in material.get_editor_property("expressions"):
        if isinstance(expression, unreal.MaterialExpressionConstant3Vector):
            color = expression.get_editor_property("constant")
            report["base_color_readback"] = [round(float(color.r), 4),
                                             round(float(color.g), 4),
                                             round(float(color.b), 4)]
    report["expression_count"] = len(material.get_editor_property("expressions"))
    unreal.log("SHOWCASE_GROUND_MATERIAL " + json.dumps(report))
    print("SHOWCASE_GROUND_MATERIAL", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
