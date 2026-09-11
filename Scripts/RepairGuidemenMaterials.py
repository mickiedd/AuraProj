"""Repair Guidemen plaque/inscription and shared PBR material graphs.

The GLB's six plaque/inscription images do not produce saved mips through the
first Interchange pass. This rerunnable repair imports all embedded maps as
normal Texture2D assets and wires explicit BaseColor/Normal/ORM channels.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
).resolve()
assert PROJECT_ROOT == Path("C:/Git/AuraProj").resolve(), "Wrong Unreal project"

DESTINATION = "/Game/Assets/Environment/GuangzhouLandmarks/Guidemen"
TEXTURE_ROOT = PROJECT_ROOT / "Saved" / "RawModelImport" / "GuidemenTextures"
REPORT_PATH = PROJECT_ROOT / "Saved" / "RawModelImport" / "Guidemen-material-repair.json"

MAPS = {
    "M_Guidemen_Stone": {
        "basecolor": "stone_basecolor_4k.png",
        "normal": "stone_normal_2k_final.png",
        "orm": "stone_orm_2k_final.png",
    },
    "M_Guidemen_AgedWood": {
        "basecolor": "wood_basecolor_4k.png",
        "normal": "wood_normal_2k_final.png",
        "orm": "wood_orm_2k_final.png",
    },
    "M_Guidemen_ClayRoof": {
        "basecolor": "roof_basecolor_4k.png",
        "normal": "roof_normal_2k_final.png",
        "orm": "roof_orm_2k_final.png",
    },
    "M_Guidemen_Plaque": {
        "basecolor": "plaque_basecolor_2k.png",
        "normal": "plaque_normal_2k.png",
        "orm": "plaque_orm_2k.png",
    },
    "M_Guidemen_Inscription": {
        "basecolor": "inscription_basecolor_2k.png",
        "normal": "inscription_normal_2k.png",
        "orm": "inscription_orm_2k.png",
    },
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
imported = {}
removed_duplicates = []


def import_texture(filename, color, normal):
    source = TEXTURE_ROOT / filename
    assert source.is_file(), source
    destination = DESTINATION + "/" + source.stem
    # Interchange preserves the embedded .png suffix as _png in the asset
    # name. Reuse those nine already-valid shared maps when available.
    interchange_destination = DESTINATION + "/" + source.stem + "_png"
    texture = unreal.EditorAssetLibrary.load_asset(interchange_destination)
    if texture is None:
        texture = unreal.EditorAssetLibrary.load_asset(destination)
    if texture is None:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = source.stem
        task.automated = True
        task.save = True
        task.replace_existing = False
        tools.import_asset_tasks([task])
        objects = task.get_objects()
        assert objects, filename
        texture = objects[0]
    texture.set_editor_property("srgb", color)
    texture.set_editor_property(
        "compression_settings",
        unreal.TextureCompressionSettings.TC_NORMALMAP
        if normal
        else unreal.TextureCompressionSettings.TC_DEFAULT
        if color
        else unreal.TextureCompressionSettings.TC_MASKS,
    )
    imported[filename] = texture.get_path_name()
    return texture


def rebuild_material(material_name, maps):
    material = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + material_name)
    assert isinstance(material, unreal.Material), material_name
    library.delete_all_material_expressions(material)
    material.set_editor_property("used_with_nanite", True)
    specs = [
        ("basecolor", unreal.MaterialProperty.MP_BASE_COLOR, "RGB", True, False, -400, 0),
        ("normal", unreal.MaterialProperty.MP_NORMAL, "RGB", False, True, -400, 220),
        ("orm", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, "R", False, False, -400, 440),
        ("orm", unreal.MaterialProperty.MP_ROUGHNESS, "G", False, False, -150, 440),
        ("orm", unreal.MaterialProperty.MP_METALLIC, "B", False, False, 100, 440),
    ]
    nodes = {}
    for map_name, material_property, output, color, normal, x, y in specs:
        if map_name not in nodes:
            texture = import_texture(maps[map_name], color, normal)
            node = library.create_material_expression(
                material, unreal.MaterialExpressionTextureSample, x, y
            )
            node.texture = texture
            node.sampler_type = (
                unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
                if normal
                else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
                if color
                else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
            )
            nodes[map_name] = node
        assert library.connect_material_property(nodes[map_name], output, material_property)
    library.recompile_material(material)


for material_name, maps in MAPS.items():
    rebuild_material(material_name, maps)

assert unreal.EditorAssetLibrary.save_directory(DESTINATION), "Save failed"

# The first repair invocation created these nine same-content assets before
# the embedded-name normalization above was added. They are now unreferenced
# by the repaired materials and are removed by exact path only.
for filename in [
    "stone_basecolor_4k.png",
    "stone_normal_2k_final.png",
    "stone_orm_2k_final.png",
    "wood_basecolor_4k.png",
    "wood_normal_2k_final.png",
    "wood_orm_2k_final.png",
    "roof_basecolor_4k.png",
    "roof_normal_2k_final.png",
    "roof_orm_2k_final.png",
]:
    duplicate = DESTINATION + "/" + Path(filename).stem
    if unreal.EditorAssetLibrary.does_asset_exist(duplicate):
        assert unreal.EditorAssetLibrary.delete_asset(duplicate), duplicate
        removed_duplicates.append(duplicate)
assert unreal.EditorAssetLibrary.save_directory(DESTINATION), "Final save failed"
result = {
    "destination": DESTINATION,
    "repaired_materials": list(MAPS),
    "texture_assets": imported,
    "removed_duplicate_texture_assets": removed_duplicates,
    "orm_channels": "R=AO, G=Roughness, B=Metallic",
    "basecolor_srgb": True,
    "normal_and_orm_linear": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GUIDEMEN_MATERIALS_REPAIRED", json.dumps(result))
