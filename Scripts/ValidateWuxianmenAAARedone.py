"""Validate the Wuxianmen AAA-redone material graph and every mesh binding."""
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V4"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen"
REPORT = ROOT / "Wuxianmen_V4-aaa-redone.json"
OUTPUT = ROOT / "Wuxianmen_V4-aaa-redone-validation.json"
SUFFIX = "_AAA_Redone"
MAPS = ("BaseColor", "Normal", "Roughness", "Metallic", "AO", "Height")


def _asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    assert value, path
    return value


def _path(value):
    return value.get_path_name() if value else ""


def main():
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    assert report["variant_suffix"] == SUFFIX
    assert report["mesh_count_rebound"] == 40
    materials = {}
    texture_checks = []
    for name, record in report["materials"].items():
        material = _asset(record["path"])
        assert isinstance(material, unreal.Material), record["path"]
        assert material.get_editor_property("used_with_nanite")
        assert material.get_editor_property("two_sided")
        blend_mode = str(material.get_editor_property("blend_mode")).upper()
        assert "OPAQUE" in blend_mode, (record["path"], blend_mode)
        assert set(record["maps"]) == set(MAPS), (name, record["maps"])
        for channel, texture_path in record["maps"].items():
            texture = _asset(texture_path)
            assert isinstance(texture, unreal.Texture2D), texture_path
            size = [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]
            if channel != "Height":
                assert size[0] == 4096, (channel, texture_path, size)
            texture_checks.append({"material": name, "channel": channel, "path": texture_path, "size": size})
        used_textures = {
            _path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material)
        }
        assert set(record["maps"].values()).issubset(used_textures), (name, used_textures)
        materials[name] = material

    mesh_records = report["mesh_records"]
    assert len(mesh_records) == 40
    for record in mesh_records:
        mesh = _asset(record["path"])
        assert isinstance(mesh, unreal.StaticMesh), record["path"]
        assert record["rebound_slots"]
        for slot in mesh.get_editor_property("static_materials"):
            assert SUFFIX in _path(slot.material_interface), (record["path"], _path(slot.material_interface))

    blueprint = _asset(report["blueprint"])
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    gathered = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    unique = set()
    component_records = []
    for handle in gathered:
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.StaticMeshComponent) or not component.static_mesh:
            continue
        key = (component.get_name(), component.static_mesh.get_path_name())
        if key in unique:
            continue
        unique.add(key)
        mats = [_path(component.get_material(i)) for i in range(component.get_num_materials())]
        assert mats and all(SUFFIX in path for path in mats), (component.get_name(), mats)
        component_records.append({"component": component.get_name(), "mesh": component.static_mesh.get_path_name(), "materials": mats})
    assert len(component_records) == 8, len(component_records)

    result = {
        "passed": True,
        "blueprint": report["blueprint"],
        "materials": len(materials),
        "textures_checked": len(texture_checks),
        "meshes_checked": len(mesh_records),
        "blueprint_handles_gathered": len(gathered),
        "blueprint_components_checked": len(component_records),
        "material_suffix": SUFFIX,
        "height_maps_in_graph_manifest": True,
        "components": component_records,
    }
    OUTPUT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("WUXIANMEN_AAA_REDONE_VALIDATION_PASS", json.dumps({
        "materials": len(materials),
        "textures": len(texture_checks),
        "meshes": len(mesh_records),
        "blueprint_components": len(component_records),
        "gathered_handles": len(gathered),
    }))


if __name__ == "__main__":
    main()
