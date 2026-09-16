"""Regenerate Wuxianmen's production PBR materials for hero and LOD meshes.

This is a Wuxianmen-only second pass over the earlier ``*_AAA`` siblings.  The
source package already contains authored 4K maps, so the rebuild focuses on
preserving their luminance and detail while adding a coherent, low-repeat UV
layout, restrained procedural macro variation, and height-driven parallax.
The previous material generations remain intact as rollback assets.
"""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen"
MATERIALS = DEST + "/Materials"
IMPORT_REPORT = ROOT / "Wuxianmen_V4-import.json"
REPORT = ROOT / "Wuxianmen_V4-aaa-redone.json"
VARIANT_SUFFIX = "_AAA_Redone"
MATERIAL_NAMES = (
    "WeatheredStone",
    "GrayBrick",
    "AgedWood",
    "GateWood",
    "ClayRoof",
    "LimePlaster",
    "DarkMetal",
    "Plaque",
)
MAP_SUFFIXES = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")


# The source texture sets carry the actual albedo variation.  These values are
# deliberately near-neutral: they steer the palette without multiplying the
# authored surface into the dark, flat result of the first reference pass.
PROFILES = {
    "WeatheredStone": {
        "macro_tiling": (0.72, 0.78),
        "micro_tiling": (1.55, 1.55),
        "macro_noise_scale": 2.0,
        "variation_strength": 0.12,
        "albedo_boost": 1.08,
        "tint": (1.04, 1.01, 0.96),
        "tint_strength": 0.16,
        "roughness_scale": 0.96,
        "ao_strength": 0.88,
        "height_ratio": 0.018,
    },
    "GrayBrick": {
        # The brick source map contains a dense tile sheet.  This lower macro
        # tiling makes individual courses read as masonry instead of a grid.
        "macro_tiling": (0.38, 0.46),
        "micro_tiling": (1.15, 1.25),
        "macro_noise_scale": 2.4,
        "variation_strength": 0.16,
        "albedo_boost": 1.10,
        "tint": (1.05, 1.02, 0.96),
        "tint_strength": 0.14,
        "roughness_scale": 0.98,
        "ao_strength": 0.90,
        "height_ratio": 0.022,
    },
    "AgedWood": {
        "macro_tiling": (0.78, 0.82),
        "micro_tiling": (2.10, 1.70),
        "macro_noise_scale": 2.2,
        "variation_strength": 0.11,
        "albedo_boost": 1.06,
        "tint": (1.05, 0.94, 0.84),
        "tint_strength": 0.13,
        "roughness_scale": 0.91,
        "ao_strength": 0.91,
        "height_ratio": 0.012,
    },
    "GateWood": {
        "macro_tiling": (0.86, 0.86),
        "micro_tiling": (2.25, 1.80),
        "macro_noise_scale": 2.2,
        "variation_strength": 0.10,
        "albedo_boost": 1.08,
        "tint": (1.06, 0.91, 0.76),
        "tint_strength": 0.15,
        "roughness_scale": 0.89,
        "ao_strength": 0.92,
        "height_ratio": 0.012,
    },
    "ClayRoof": {
        "macro_tiling": (0.88, 0.92),
        "micro_tiling": (1.75, 1.75),
        "macro_noise_scale": 2.0,
        "variation_strength": 0.13,
        "albedo_boost": 1.18,
        "tint": (0.93, 0.98, 1.06),
        "tint_strength": 0.16,
        "roughness_scale": 0.94,
        "ao_strength": 0.87,
        "height_ratio": 0.012,
    },
    "LimePlaster": {
        "macro_tiling": (0.74, 0.80),
        "micro_tiling": (1.45, 1.45),
        "macro_noise_scale": 2.1,
        "variation_strength": 0.10,
        "albedo_boost": 1.10,
        "tint": (1.06, 1.02, 0.92),
        "tint_strength": 0.14,
        "roughness_scale": 1.00,
        "ao_strength": 0.90,
        "height_ratio": 0.014,
    },
    "DarkMetal": {
        "macro_tiling": (1.05, 1.05),
        "micro_tiling": (2.40, 2.40),
        "macro_noise_scale": 2.5,
        "variation_strength": 0.07,
        "albedo_boost": 1.12,
        "tint": (0.94, 0.97, 1.00),
        "tint_strength": 0.10,
        "roughness_scale": 0.88,
        "ao_strength": 0.92,
        "height_ratio": 0.008,
    },
    "Plaque": {
        "macro_tiling": (1.0, 1.0),
        "micro_tiling": (1.35, 1.35),
        "macro_noise_scale": 1.8,
        "variation_strength": 0.06,
        "albedo_boost": 1.08,
        "tint": (1.08, 1.00, 0.84),
        "tint_strength": 0.11,
        "roughness_scale": 0.84,
        "ao_strength": 0.91,
        "height_ratio": 0.008,
    },
}


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(source, source_output, target, target_input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    ), (source, source_output, target, target_input)


def _connect_property(source, source_output, material, property_name):
    assert unreal.MaterialEditingLibrary.connect_material_property(
        source, source_output, property_name
    ), (source, source_output, property_name)


def _constant(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", float(value))
    return node


def _color(material, value, x, y):
    node = _expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def _coord(material, tiling, x, y):
    node = _expr(material, unreal.MaterialExpressionTextureCoordinate, x, y)
    node.set_editor_property("coordinate_index", 0)
    node.set_editor_property("u_tiling", float(tiling[0]))
    node.set_editor_property("v_tiling", float(tiling[1]))
    return node


def _sample(material, texture, uv, sampler, x, y):
    node = _expr(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    _connect(uv, "", node, "UVs")
    return node


def _multiply(material, left, right, x, y, left_output=""):
    node = _expr(material, unreal.MaterialExpressionMultiply, x, y)
    _connect(left, left_output, node, "A")
    _connect(right, "", node, "B")
    return node


def _lerp(material, first, second, alpha, x, y):
    node = _expr(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    _connect(first, "", node, "A")
    _connect(second, "", node, "B")
    _connect(alpha, "", node, "Alpha")
    return node


def _saturate(material, source, x, y, source_output=""):
    node = _expr(material, unreal.MaterialExpressionSaturate, x, y)
    _connect(source, source_output, node, "Input")
    return node


def _texture_map(material, source_name):
    result = {}
    for texture in unreal.MaterialEditingLibrary.get_used_textures(material):
        name = texture.get_name().lower()
        if "basecolor" in name or "base_color" in name or name.endswith("_bc"):
            result["BaseColor"] = texture
        elif "normal" in name or name.endswith("_n"):
            result["Normal"] = texture
        elif "roughness" in name or name.endswith("_r"):
            result["Roughness"] = texture
        elif "metallic" in name or name.endswith("_m"):
            result["Metallic"] = texture
        elif "ambient" in name or "_ao" in name:
            result["AO"] = texture
        elif "height" in name or name.endswith("_h"):
            result["Height"] = texture
    # The original V4 importer wires only the five runtime channels.  Resolve
    # Height explicitly from the imported project texture folder so the 4K
    # parallax source is not silently omitted from the regenerated graph.
    for suffix in MAP_SUFFIXES:
        if suffix in result:
            continue
        path = DEST + "/Textures/" + source_name + "/T_" + source_name + "_" + suffix + "_4K"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            result[suffix] = _asset(path)
    assert set(MAP_SUFFIXES).issubset(result), (material.get_path_name(), sorted(result))
    return result


def _build_material(name):
    profile = PROFILES[name]
    source_path = MATERIALS + "/M_" + name
    source = _asset(source_path)
    output_name = "M_" + name + VARIANT_SUFFIX
    output_path = MATERIALS + "/" + output_name
    material = (
        _asset(output_path)
        if unreal.EditorAssetLibrary.does_asset_exist(output_path)
        else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            output_name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew()
        )
    )
    assert isinstance(material, unreal.Material), output_path
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property("used_with_nanite", True)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    maps = _texture_map(source, name)
    macro_uv = _coord(material, profile["macro_tiling"], -1420, -40)
    detail_uv = _coord(material, profile["micro_tiling"], -1420, 760)

    # Height is used as a restrained parallax cue.  Keeping it in the same UV
    # space as the macro albedo prevents the wall courses from swimming.
    height = _sample(
        material,
        maps["Height"],
        macro_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1200,
        260,
    )
    bump = _expr(material, unreal.MaterialExpressionBumpOffset, -980, 260)
    _connect(macro_uv, "", bump, "Coordinate")
    _connect(height, "R", bump, "Height")
    bump.set_editor_property("height_ratio", float(profile["height_ratio"]))
    bump.set_editor_property("reference_plane", 0.5)

    base = _sample(
        material,
        maps["BaseColor"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        -1200,
        -260,
    )
    boosted = _multiply(
        material,
        base,
        _constant(material, profile["albedo_boost"], -1200, -80),
        -960,
        -260,
        "RGB",
    )
    tint = _multiply(
        material,
        boosted,
        _color(material, profile["tint"], -960, -40),
        -740,
        -260,
    )
    palette = _lerp(
        material,
        boosted,
        tint,
        _constant(material, profile["tint_strength"], -740, 100),
        -500,
        -260,
    )

    # Subtle macro variation keeps large walls and roof planes from reading as
    # a single repeated tile sheet, without replacing the authored albedo.
    variation = None
    try:
        noise = _expr(material, unreal.MaterialExpressionNoise, -1180, 1480)
        noise.set_editor_property("scale", float(profile["macro_noise_scale"]))
        noise.set_editor_property("quality", 2)
        noise.set_editor_property("levels", 4)
        noise.set_editor_property("output_min", 0.0)
        noise.set_editor_property("output_max", 1.0)
        # UE 5.5's Noise Position input is float3 while TextureCoordinate is
        # float2.  Leaving Position unconnected uses the stable world-position
        # path and avoids an implicit type conversion that changes between
        # engine minor versions.
        low = _constant(
            material,
            1.0 - profile["variation_strength"],
            -960,
            1520,
        )
        high = _constant(
            material,
            1.0 + profile["variation_strength"],
            -960,
            1640,
        )
        variation = _lerp(material, low, high, noise, -700, 1520)
    except Exception as exc:
        unreal.log_warning("Wuxianmen macro variation fallback for %s: %s" % (name, exc))
        variation = _constant(material, 1.0, -700, 1520)
    base_output = _multiply(material, palette, variation, -240, -260)
    _connect_property(base_output, "", material, unreal.MaterialProperty.MP_BASE_COLOR)

    # The supplied 4K normal remains the fine-scale detail layer.  The macro
    # height offset is still applied to the albedo/roughness/AO family.
    normal = _sample(
        material,
        maps["Normal"],
        detail_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        -1180,
        520,
    )
    _connect_property(normal, "RGB", material, unreal.MaterialProperty.MP_NORMAL)

    roughness = _sample(
        material,
        maps["Roughness"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1180,
        820,
    )
    roughness = _multiply(
        material,
        roughness,
        _constant(material, profile["roughness_scale"], -960, 940),
        -740,
        820,
        "R",
    )
    _connect_property(roughness, "", material, unreal.MaterialProperty.MP_ROUGHNESS)

    metallic = _sample(
        material,
        maps["Metallic"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1180,
        1100,
    )
    _connect_property(metallic, "R", material, unreal.MaterialProperty.MP_METALLIC)

    ao = _sample(
        material,
        maps["AO"],
        bump,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
        -1180,
        1360,
    )
    ao = _lerp(
        material,
        _constant(material, 1.0, -960, 1440),
        ao,
        _constant(material, profile["ao_strength"], -960, 1540),
        -740,
        1360,
    )
    _connect_property(ao, "", material, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _connect_property(
        _constant(material, 0.42, -240, 1100),
        "",
        material,
        unreal.MaterialProperty.MP_SPECULAR,
    )

    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return source, material, {
        "path": material.get_path_name(),
        "maps": {key: value.get_path_name() for key, value in maps.items()},
        "profile": profile,
        "graph": [
            "4K BaseColor with luminance-preserving boost/tint",
            "low-repeat macro UV with procedural variation",
            "4K height BumpOffset",
            "4K normal microdetail",
            "calibrated roughness, metallic, AO, and specular",
        ],
    }


def _rebind_all_meshes(materials):
    report = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    mesh_records = []
    seen = set()
    for entry in report["imports"]:
        if entry["source"].get("collision"):
            continue
        for mesh_path in entry["meshes"]:
            if mesh_path in seen:
                continue
            seen.add(mesh_path)
            mesh = _asset(mesh_path)
            mesh.modify()
            before = []
            rebound_slots = []
            static_materials = mesh.get_editor_property("static_materials")
            for index, slot in enumerate(static_materials):
                slot_name = str(slot.material_slot_name)
                current = slot.material_interface
                before.append(
                    {
                        "index": index,
                        "slot": slot_name,
                        "material": current.get_path_name() if current else "",
                    }
                )
                source_name = slot_name[2:] if slot_name.startswith("M_") else slot_name
                if source_name not in materials:
                    continue
                static_materials[index].material_interface = materials[source_name]
                mesh.set_material(index, materials[source_name])
                rebound_slots.append(slot_name)
            mesh.set_editor_property("static_materials", static_materials)
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property(
                "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
            )
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            assert unreal.EditorAssetLibrary.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
            mesh_records.append(
                {
                    "path": mesh_path,
                    "source": entry["source"]["relative"],
                    "before": before,
                    "rebound_slots": rebound_slots,
                    "nanite": "NaniteHigh" in mesh_path,
                }
            )
    assert len(mesh_records) == 40, len(mesh_records)
    return mesh_records


def _rebind_blueprint(materials):
    blueprint_path = DEST + "/BP_Wuxianmen_V4"
    blueprint = _asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    gathered = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    rebound = 0
    unique = set()
    for handle in gathered:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        key = (component.get_name(), component.static_mesh.get_path_name())
        if key in unique:
            continue
        unique.add(key)
        mesh = component.static_mesh
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.material_slot_name)
            source_name = slot_name[2:] if slot_name.startswith("M_") else slot_name
            if source_name in materials:
                component.set_material(index, materials[source_name])
        rebound += 1
    assert rebound == 8, rebound
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint_path, len(gathered), rebound


def main():
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty_maps, "Save the current map before Wuxianmen AAA rebuild: " + str(dirty_maps)

    generated = {}
    materials = {}
    source_materials = {}
    for name in MATERIAL_NAMES:
        source, material, record = _build_material(name)
        materials[name] = material
        source_materials[name] = source.get_path_name()
        generated[name] = record

    mesh_records = _rebind_all_meshes(materials)
    blueprint_path, gathered_count, blueprint_rebound = _rebind_blueprint(materials)
    result = {
        "intent": "Wuxianmen AAA material regeneration with readable daylight palette, low-repeat gray masonry, 4K PBR, height parallax, and fine normal detail.",
        "variant_suffix": VARIANT_SUFFIX,
        "blueprint": blueprint_path,
        "materials": generated,
        "source_materials": source_materials,
        "mesh_records": mesh_records,
        "mesh_count_rebound": len(mesh_records),
        "blueprint_handles_gathered": gathered_count,
        "blueprint_components_rebound": blueprint_rebound,
        "geometry_changed": False,
        "uvs_changed": False,
        "collision_changed": False,
        "placement_changed": False,
        "rollback_assets": [
            MATERIALS + "/M_" + name + "_ReferenceTuned"
            for name in MATERIAL_NAMES
        ]
        + [MATERIALS + "/M_" + name + "_AAA" for name in MATERIAL_NAMES],
    }
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(
        "WUXIANMEN_AAA_MATERIALS_REDONE",
        json.dumps(
            {
                "materials": [generated[name]["path"] for name in MATERIAL_NAMES],
                "mesh_count_rebound": len(mesh_records),
                "blueprint_components_rebound": blueprint_rebound,
            }
        ),
    )


if __name__ == "__main__":
    main()
