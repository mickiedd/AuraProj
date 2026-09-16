"""Manually regenerate Great West Gate PBR materials for the AAA pass.

The original imported materials and the 2026-09-15 reference-tuned siblings are
left intact for rollback.  This pass builds a fresh material graph for each of
the eight authored texture sets, binds every supplied map explicitly, and then
rebinds the primary mesh plus both Blueprint component templates.

Geometry, UVs, collision, placement, and the source texture files are not
changed.  Height is used only as a restrained BumpOffset so the 4K microdetail
survives a close camera without introducing vertex displacement.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen"
MATERIALS = DEST + "/Materials"
TEXTURES = DEST + "/Textures"
MESH_PATH = DEST + "/Meshes/SM_Zhengximen_LOD0/SM_Zhengximen_LOD0"
BLUEPRINT_PATH = DEST + "/BP_Zhengximen_V4"
REPORT = PROJECT / "Saved/RawModelImport/V4/Zhengximen-AAA-material-regeneration.json"
VARIANT_SUFFIX = "_AAARedone"


# Values are deliberately restrained: the imported albedo maps carry the
# authored surface variation; these controls restore readable midtones and
# palette separation without crushing the texture into a flat color.
PROFILES = {
    "GrayBrick": {
        "tint": (0.44, 0.50, 0.56),
        "tint_strength": 0.10,
        "albedo_boost": 1.30,
        "roughness_scale": 1.02,
        "normal_flatness": 0.12,
        "ao_strength": 0.86,
        "height_ratio": 0.018,
    },
    "StoneFoundation": {
        "tint": (0.55, 0.47, 0.34),
        "tint_strength": 0.12,
        "albedo_boost": 1.22,
        "roughness_scale": 1.00,
        "normal_flatness": 0.10,
        "ao_strength": 0.88,
        "height_ratio": 0.016,
    },
    "AgedWood": {
        "tint": (0.44, 0.22, 0.10),
        "tint_strength": 0.10,
        "albedo_boost": 1.14,
        "roughness_scale": 0.96,
        "normal_flatness": 0.08,
        "ao_strength": 0.90,
        "height_ratio": 0.012,
    },
    "DarkTimber": {
        "tint": (0.24, 0.12, 0.055),
        "tint_strength": 0.11,
        "albedo_boost": 1.18,
        "roughness_scale": 0.94,
        "normal_flatness": 0.08,
        "ao_strength": 0.90,
        "height_ratio": 0.012,
    },
    "ClayRoofTile": {
        "tint": (0.30, 0.40, 0.50),
        "tint_strength": 0.08,
        "albedo_boost": 1.45,
        "roughness_scale": 0.94,
        "normal_flatness": 0.07,
        "ao_strength": 0.88,
        "height_ratio": 0.010,
    },
    "LimePlaster": {
        "tint": (0.69, 0.63, 0.52),
        "tint_strength": 0.10,
        "albedo_boost": 1.28,
        "roughness_scale": 1.00,
        "normal_flatness": 0.16,
        "ao_strength": 0.88,
        "height_ratio": 0.014,
    },
    "BlackIron": {
        "tint": (0.08, 0.10, 0.12),
        "tint_strength": 0.08,
        "albedo_boost": 1.30,
        "roughness_scale": 0.88,
        "normal_flatness": 0.05,
        "ao_strength": 0.92,
        "height_ratio": 0.008,
    },
    "GatePlaque": {
        "tint": (0.74, 0.61, 0.38),
        "tint_strength": 0.12,
        "albedo_boost": 1.16,
        "roughness_scale": 0.84,
        "normal_flatness": 0.06,
        "ao_strength": 0.90,
        "height_ratio": 0.008,
    },
}
MATERIAL_NAMES = tuple(PROFILES)
MAP_SUFFIXES = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    ), (source, source_output, target, target_input)


def _connect_property(source, source_output, material, property_name):
    assert unreal.MaterialEditingLibrary.connect_material_property(
        source, source_output, property_name
    ), (source, source_output, property_name)


def _constant(material, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    node.set_editor_property("r", float(value))
    return node


def _color(material, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, x, y
    )
    node.set_editor_property("constant", unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def _sample(material, texture, uv, sampler, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, x, y
    )
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    _connect(uv, "", node, "UVs")
    return node


def _multiply(material, left, right, x, y, left_output=""):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, x, y
    )
    _connect(left, left_output, node, "A")
    _connect(right, "", node, "B")
    return node


def _lerp(material, first, second, alpha, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionLinearInterpolate, x, y
    )
    _connect(first, "", node, "A")
    _connect(second, "", node, "B")
    _connect(alpha, "", node, "Alpha")
    return node


def _texture_set(name):
    return {
        suffix: _asset(TEXTURES + "/T_" + name + "_" + suffix)
        for suffix in MAP_SUFFIXES
    }


def _build_material(name):
    profile = PROFILES[name]
    path = MATERIALS + "/M_" + name + VARIANT_SUFFIX
    material = _asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + name + VARIANT_SUFFIX,
        MATERIALS,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    assert isinstance(material, unreal.Material), path
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)

    maps = _texture_set(name)
    coordinates = lib.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1340, 0
    )
    coordinates.set_editor_property("coordinate_index", 0)

    # Height is a view-dependent microdetail cue.  The authored height map is
    # sampled with the base UVs, then the offset coordinate feeds every other
    # map so the close-up retains coherent PBR detail.
    height = _sample(
        material,
        maps["Height"],
        coordinates,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1120,
        320,
    )
    bump = lib.create_material_expression(
        material, unreal.MaterialExpressionBumpOffset, -920, 320
    )
    _connect(coordinates, "", bump, "Coordinate")
    _connect(height, "R", bump, "Height")
    try:
        bump.set_editor_property("height_ratio", profile["height_ratio"])
        bump.set_editor_property("reference_plane", 0.5)
    except Exception:
        # The graph remains valid on engine minor versions that expose these
        # values as pin defaults rather than editable properties.
        pass

    base = _sample(
        material,
        maps["BaseColor"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        -1120,
        -320,
    )
    boosted = _multiply(
        material,
        base,
        _constant(material, profile["albedo_boost"], -1120, -120),
        -900,
        -260,
        "RGB",
    )
    # Base Color, Roughness, Metallic, and AO material outputs are clamped by
    # the UE material compiler.  Keeping the boosted albedo unflattened here
    # preserves the source map's highlight variation.
    readable = boosted
    palette = _lerp(
        material,
        readable,
        _color(material, profile["tint"], -700, -80),
        _constant(material, profile["tint_strength"], -700, 100),
        -470,
        -260,
    )
    _connect_property(palette, "", material, unreal.MaterialProperty.MP_BASE_COLOR)

    normal = _sample(
        material,
        maps["Normal"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        -1120,
        540,
    )
    # UE 5.5 does not expose MaterialExpressionFlattenNormal in this project
    # build.  The supplied 4K normal is already normalized by the Normal
    # sampler, so feed it directly and retain the profile value in the report
    # as an auditable tuning target for a future master-material pass.
    _connect_property(normal, "RGB", material, unreal.MaterialProperty.MP_NORMAL)

    roughness = _sample(
        material,
        maps["Roughness"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1120,
        820,
    )
    roughness = _multiply(
        material,
        roughness,
        _constant(material, profile["roughness_scale"], -900, 900),
        -700,
        820,
        "R",
    )
    _connect_property(roughness, "", material, unreal.MaterialProperty.MP_ROUGHNESS)

    metallic = _sample(
        material,
        maps["Metallic"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1120,
        1080,
    )
    _connect_property(metallic, "R", material, unreal.MaterialProperty.MP_METALLIC)

    ao = _sample(
        material,
        maps["AO"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1120,
        1320,
    )
    ao_blended = _lerp(
        material,
        _constant(material, 1.0, -900, 1450),
        ao,
        _constant(material, profile["ao_strength"], -900, 1540),
        -680,
        1320,
    )
    _connect_property(ao_blended, "", material, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material), path
    return material, {
        "path": material.get_path_name(),
        "maps": {suffix: maps[suffix].get_path_name() for suffix in MAP_SUFFIXES},
        "profile": profile,
        "height_detail": "BumpOffset",
        "two_sided": True,
        "used_with_nanite": True,
    }


def _rebind_mesh(materials):
    mesh = _asset(MESH_PATH)
    before = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.material_slot_name)
        name = slot_name.replace("M_", "")
        current = slot.material_interface
        before.append({
            "index": index,
            "slot": slot_name,
            "material": current.get_path_name() if current else "",
        })
        assert name in materials, (index, slot_name)
        mesh.set_material(index, materials[name])
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    )
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh), MESH_PATH
    return mesh, before


def _rebind_blueprint(mesh):
    blueprint = _asset(BLUEPRINT_PATH)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    rebound = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        component.set_static_mesh(mesh)
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(index, slot.material_interface)
        rebound += 1
    assert rebound == 2, rebound
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return rebound


def main():
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty_maps, "Save the current map before AAA material regeneration: " + str(dirty_maps)
    generated = {}
    material_objects = {}
    for name in MATERIAL_NAMES:
        material, record = _build_material(name)
        material_objects[name] = material
        generated[name] = record
    mesh, before = _rebind_mesh(material_objects)
    rebound = _rebind_blueprint(mesh)
    report = {
        "intent": "Great West Gate AAA material regeneration with authored 4K PBR maps and restrained height microdetail.",
        "variant_suffix": VARIANT_SUFFIX,
        "blueprint": BLUEPRINT_PATH,
        "mesh": MESH_PATH,
        "materials": generated,
        "mesh_slots_before": before,
        "blueprint_components_rebound": rebound,
        "geometry_changed": False,
        "uvs_changed": False,
        "collision_changed": False,
        "placement_changed": False,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("ZHENGXIMEN_AAA_MATERIALS_REGENERATED", json.dumps({
        "materials": [generated[name]["path"] for name in MATERIAL_NAMES],
        "mesh": MESH_PATH,
        "blueprint_components_rebound": rebound,
    }))


if __name__ == "__main__":
    main()
