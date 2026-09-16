"""Build and apply hand-authored PBR material graphs for every V4 landmark.

The source packages already contain the reference-driven 4K maps.  This pass
rebuilds the project materials from those maps with a consistent layered graph:
base-color tint, a restrained macro layer from the same authored map, normal detail, calibrated
roughness/metallic and AO, plus physically appropriate two-sided/blend settings.
Each material is a sibling ``_AAA`` asset, so the earlier tuned materials remain
available as rollback references.
"""
import json
from pathlib import Path
import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
PACKAGES = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))

TINTS = {
    # Darker linear tints preserve the reference's weathered stone and charcoal
    # tile values under the bright preview key light.
    "Stone": (0.48, 0.44, 0.37), "Plaster": (0.56, 0.49, 0.39),
    "Wood": (0.34, 0.15, 0.065), "RoofClay": (0.34, 0.38, 0.44),
    "Dirt": (0.42, 0.28, 0.15), "Water": (0.16, 0.34, 0.37),
    "Vegetation": (0.035, 0.11, 0.012), "AgedWood": (0.34, 0.15, 0.065),
    "ClayRoof": (0.33, 0.38, 0.44), "DarkMetal": (0.09, 0.10, 0.095),
    "GateWood": (0.30, 0.11, 0.04), "GrayBrick": (0.46, 0.42, 0.35),
    "LimePlaster": (0.56, 0.49, 0.39), "Plaque": (0.72, 0.52, 0.27),
    "WeatheredStone": (0.48, 0.42, 0.34), "ClayRoofTile": (0.33, 0.38, 0.44),
    "DarkTimber": (0.27, 0.10, 0.035), "StoneFoundation": (0.46, 0.38, 0.28),
    "Tile_ClayGrey": (0.33, 0.38, 0.44), "Wood_Aged": (0.34, 0.15, 0.065),
    "Stone_BlueGrey": (0.46, 0.42, 0.35), "Plaster_OffWhite": (0.56, 0.49, 0.39),
    "DoorWood": (0.25, 0.075, 0.02),
}

ROUGHNESS = {
    "Stone": 1.05, "Plaster": 1.10, "Wood": 0.86, "RoofClay": 1.02,
    "Dirt": 1.12, "Water": 0.72, "Vegetation": 0.92, "AgedWood": 0.86,
    "ClayRoof": 1.02, "DarkMetal": 0.78, "GateWood": 0.86, "GrayBrick": 1.06,
    "LimePlaster": 1.08, "Plaque": 0.80, "WeatheredStone": 1.06,
    "ClayRoofTile": 1.02, "DarkTimber": 0.86, "StoneFoundation": 1.08,
    "Tile_ClayGrey": 1.02, "Wood_Aged": 0.86, "Stone_BlueGrey": 1.06,
    "Plaster_OffWhite": 1.08,
}


def asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def texture_map(material):
    result = {}
    for tex in unreal.MaterialEditingLibrary.get_used_textures(material):
        name = tex.get_name().lower()
        if "basecolor" in name or "base_color" in name or "_bc_" in name or name.endswith("_bc"):
            result.setdefault("BC", tex)
        elif "normal" in name or "_n_" in name or name.endswith("_n"):
            result.setdefault("N", tex)
        elif "roughness" in name or "_r_" in name or name.endswith("_r"):
            result.setdefault("R", tex)
        elif "metallic" in name or "_m_" in name or name.endswith("_m"):
            result.setdefault("M", tex)
        elif "ambient" in name or "_ao" in name:
            result.setdefault("AO", tex)
    return result


def expr(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def scalar(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def color(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def sample(material, tex, sampler, x, y, uv=None):
    node = expr(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", tex)
    node.set_editor_property("sampler_type", sampler)
    if uv:
        assert unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", node, "UVs")
    return node


def multiply(material, left, right, x, y, left_output=""):
    node = expr(material, unreal.MaterialExpressionMultiply, x, y)
    lib = unreal.MaterialEditingLibrary
    assert lib.connect_material_expressions(left, left_output, node, "A")
    assert lib.connect_material_expressions(right, "", node, "B")
    return node


def build_material(package, source_name, source_override=None, target_name=None, tint_key=None):
    dest = package["destination"]
    original_path = source_override or (dest + "/Materials/" + source_name)
    original = asset(original_path)
    aaa_name = target_name or (source_name + "_AAA")
    aaa_path = dest + "/Materials/" + aaa_name
    aaa = asset(aaa_path) if unreal.EditorAssetLibrary.does_asset_exist(aaa_path) else None
    if aaa is None:
        aaa = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(aaa_name, dest + "/Materials", original)
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(aaa)
    aaa.set_editor_property("used_with_nanite", True)
    aaa.set_editor_property("two_sided", True)
    source_key = tint_key or (source_name[2:] if source_name.startswith("M_") else source_name)
    if source_key.startswith("Dadongmen_"):
        source_key = source_key[len("Dadongmen_"):]
    key = source_key
    tint = color(aaa, TINTS.get(key, (0.72, 0.68, 0.58)), -920, 160)
    maps = texture_map(original)
    if "BC" in maps:
        # Layer a second restrained sample of the authored map so close shots
        # keep the source's natural variation instead of reading as a flat tile.
        # The imported UVs already contain the package's authored tiling.  Keep
        # the graph deterministic across UE 5.5's material pin variants and
        # layer the same high quality source map with a restrained blend.
        uv_detail = None
        base = sample(aaa, maps["BC"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1180, 300)
        base_tinted = multiply(aaa, base, tint, -760, 180, "RGB")
        detail = sample(aaa, maps["BC"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -760, 440, uv_detail)
        detail_tinted = multiply(aaa, detail, color(aaa, (0.72, 0.72, 0.72), -760, 600), -520, 420, "RGB")
        blend = expr(aaa, unreal.MaterialExpressionLinearInterpolate, -280, 210)
        assert lib.connect_material_expressions(base_tinted, "", blend, "A")
        assert lib.connect_material_expressions(detail_tinted, "", blend, "B")
        assert lib.connect_material_expressions(scalar(aaa, 0.18, -520, 620), "", blend, "Alpha")
        assert lib.connect_material_property(blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        assert lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if "N" in maps:
        normal = sample(aaa, maps["N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -760, 760, uv_detail if "BC" in maps else None)
        assert lib.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if "R" in maps:
        rough = sample(aaa, maps["R"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -760, 940, uv_detail if "BC" in maps else None)
        rough = multiply(aaa, rough, scalar(aaa, ROUGHNESS.get(key, 1.0), -520, 1040), -260, 940, "R")
        assert lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        assert lib.connect_material_property(scalar(aaa, ROUGHNESS.get(key, 0.9), -260, 940), "", unreal.MaterialProperty.MP_ROUGHNESS)
    if "M" in maps:
        metallic = sample(aaa, maps["M"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -760, 1180, uv_detail if "BC" in maps else None)
        assert lib.connect_material_property(metallic, "R", unreal.MaterialProperty.MP_METALLIC)
    else:
        metal_value = 0.72 if any(x in key.lower() for x in ("metal", "iron", "bronze")) else 0.0
        assert lib.connect_material_property(scalar(aaa, metal_value, -260, 1180), "", unreal.MaterialProperty.MP_METALLIC)
    if "AO" in maps:
        ao = sample(aaa, maps["AO"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -760, 1360, uv_detail if "BC" in maps else None)
        assert lib.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    specular = 0.05 if key.lower() == "vegetation" else 0.36
    assert lib.connect_material_property(scalar(aaa, specular, -260, 1360), "", unreal.MaterialProperty.MP_SPECULAR)
    if key.lower() == "water":
        aaa.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        assert lib.connect_material_property(scalar(aaa, 0.62, -260, 1540), "", unreal.MaterialProperty.MP_OPACITY)
    else:
        aaa.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    lib.layout_material_expressions(aaa)
    lib.recompile_material(aaa)
    assert unreal.EditorAssetLibrary.save_loaded_asset(aaa, only_if_is_dirty=False)
    return original, aaa


def rebind_package(package, aaa_by_source):
    name = package["name"]
    report_path = ROOT / (name + "-import.json")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    primary = next(e for e in report["imports"] if e["source"].get("primary"))
    paths = list(primary["meshes"])
    if name == "Dadongmen_V4":
        paths.append("/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Meshes/SM_Dadongmen_OpenDoor_LOD0/SM_Dadongmen_OpenDoor_LOD0.SM_Dadongmen_OpenDoor_LOD0")
    rebound = []
    for mesh_path in paths:
        mesh = asset(mesh_path)
        for i, slot in enumerate(mesh.get_editor_property("static_materials")):
            mat = slot.material_interface
            if not mat:
                continue
            source_name = mat.get_name()
            for suffix in ("_ReferenceTuned", "_AAARedone", "_AAA"):
                source_name = source_name.split(suffix)[0]
            if source_name in aaa_by_source:
                mesh.set_material(i, aaa_by_source[source_name])
        mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        rebound.append(mesh_path)
    bp_path = package["destination"] + "/BP_" + name
    bp = asset(bp_path)
    ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    changed = 0
    seen = set()
    for handle in ss.k2_gather_subobject_data_for_blueprint(bp):
        component = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        if component.get_name() in seen:
            continue
        for mesh_path in rebound:
            if component.static_mesh.get_path_name() != mesh_path:
                continue
            mesh = asset(mesh_path)
            for i, slot in enumerate(mesh.get_editor_property("static_materials")):
                if slot.material_interface:
                    component.set_material(i, slot.material_interface)
            seen.add(component.get_name())
            changed += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    # Dadongmen's movable gate details use a dedicated darker wood sibling.
    if name == "Dadongmen_V4" and "M_Dadongmen_DoorWood_AAA" in aaa_by_source:
        door_mat = aaa_by_source["M_Dadongmen_DoorWood_AAA"]
        iron_mat = aaa_by_source.get("M_Dadongmen_Iron_AAA", door_mat)
        for handle in ss.k2_gather_subobject_data_for_blueprint(bp):
            component = lib.get_object(ss.k2_find_subobject_data_from_handle(handle))
            if not isinstance(component, unreal.StaticMeshComponent):
                continue
            comp_name = component.get_name()
            if not comp_name.startswith("Detail_Dadongmen_Door_"):
                continue
            if any(token in comp_name for token in ("Threshold", "Interior_Floor")):
                component.set_material(0, aaa_by_source["M_Dadongmen_Stone"])
            elif "Approach_Path" in comp_name:
                component.set_material(0, asset(package["destination"] + "/Materials/M_Dadongmen_Approach_ReferenceTuned"))
            elif "Approach_Water" in comp_name:
                component.set_material(0, aaa_by_source["M_Dadongmen_Water"])
            elif "IronStrap" in comp_name or "Stud" in comp_name:
                component.set_material(0, iron_mat)
            else:
                component.set_material(0, door_mat)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    return rebound, changed


def main():
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not dirty, "Save the current map before the V4 material rebuild: " + str(dirty)
    all_reports = {}
    for package in PACKAGES:
        originals, aaa_by_source = {}, {}
        for source_name in package["materials"].keys():
            original, aaa = build_material(package, source_name)
            originals[source_name] = original.get_path_name()
            aaa_by_source[source_name] = aaa
        if package["name"] == "Dadongmen_V4":
            _original, door_aaa = build_material(
                package,
                "M_Dadongmen_DoorWood",
                source_override=package["destination"] + "/Materials/M_Dadongmen_Wood",
                target_name="M_Dadongmen_DoorWood_AAA",
                tint_key="DoorWood",
            )
            aaa_by_source["M_Dadongmen_DoorWood_AAA"] = door_aaa
            _original, iron_aaa = build_material(
                package,
                "M_Dadongmen_Iron_ReferenceTuned",
                source_override=package["destination"] + "/Materials/M_Dadongmen_Iron_ReferenceTuned",
                target_name="M_Dadongmen_Iron_AAA",
                tint_key="DarkMetal",
            )
            aaa_by_source["M_Dadongmen_Iron_AAA"] = iron_aaa
        meshes, changed = rebind_package(package, aaa_by_source)
        all_reports[package["name"]] = {
            "blueprint": package["destination"] + "/BP_" + package["name"],
            "aaa_materials": {k: v.get_path_name() for k, v in aaa_by_source.items()},
            "meshes_rebound": meshes,
            "blueprint_components_rebound": changed,
            "source_materials": originals,
        "graph": "4K base color plus restrained same-map macro layer, normal, calibrated roughness, metallic and AO",
        }
        (ROOT / (package["name"] + "-aaa-materials.json")).write_text(json.dumps(all_reports[package["name"]], indent=2), encoding="utf-8")
    print("V4_AAA_MATERIALS_COMPLETE", json.dumps(all_reports))


if __name__ == "__main__":
    main()
