"""Refresh the existing Wenmingmen assets from the edited GLB and texture files.

Run in the Aura UE 5.5 editor or Python commandlet after regenerating
``ContentSource/GuangzhouLandmarks/Wenmingmen/Wenmingmen_HighDetail.glb``.
The eight StaticMesh assets, eight materials, 25 textures, and BP_Wenmingmen
keep their package paths and Blueprint component names. No asset is deleted.

Interchange's explicit ``reimport_asset`` path is used once per mesh. This is
intentional: a scene import would also manage level actors and could disturb
the independently authored Blueprint. Unreal reuses each mesh's saved import
pipeline, including the source orientation established by the first import.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportWenmingmen import (  # noqa: E402
    DEST, EAL, IMPORT_MANIFEST, NANITE, PARTS, PROJECT, REPORT, SOURCE,
    STRUCTURAL, path, validate,
)

GLB = SOURCE / "Wenmingmen_HighDetail.glb"
UPDATE_REPORT = PROJECT / "Saved/RawModelImport/Wenmingmen-update.json"
MESH_DIR = DEST + "/Meshes"
BLUEPRINT_PATH = DEST + "/BP_Wenmingmen"


def sha256_file(filename):
    digest = hashlib.sha256()
    with filename.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _identity_transform(node):
    return (
        node.get("translation", [0, 0, 0]) == [0, 0, 0]
        and node.get("rotation", [0, 0, 0, 1]) == [0, 0, 0, 1]
        and node.get("scale", [1, 1, 1]) == [1, 1, 1]
        and node.get("matrix", [1, 0, 0, 0, 0, 1, 0, 0,
                                0, 0, 1, 0, 0, 0, 0, 1])
        == [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    )


def inspect_glb():
    """Reject changes that would remap a Blueprint component or material slot."""
    assert GLB.is_file(), GLB
    with GLB.open("rb") as stream:
        header = stream.read(12)
        assert len(header) == 12, "Incomplete GLB header"
        magic, version, declared_length = struct.unpack("<4sII", header)
        assert magic == b"glTF" and version == 2, (magic, version)
        assert declared_length == GLB.stat().st_size, "Incomplete GLB write"
        chunk_header = stream.read(8)
        assert len(chunk_header) == 8, "Missing GLB JSON chunk"
        json_length, chunk_type = struct.unpack("<II", chunk_header)
        assert chunk_type == 0x4E4F534A, "GLB must start with JSON"
        document = json.loads(stream.read(json_length))
        binary_header = stream.read(8)
        assert len(binary_header) == 8, "Missing GLB binary chunk"
        binary_length, binary_type = struct.unpack("<II", binary_header)
        assert binary_type == 0x004E4942, "Missing embedded GLB buffer"
        binary_start = stream.tell()
        assert binary_start + binary_length == declared_length, "Unexpected GLB chunks"
    meshes = document.get("meshes", [])
    materials = document.get("materials", [])
    nodes = document.get("nodes", [])
    accessors = document.get("accessors", [])
    assert len(meshes) == len(PARTS), [mesh.get("name") for mesh in meshes]
    assert len(materials) == len(PARTS), [mat.get("name") for mat in materials]
    expected_meshes = {"WM_" + part for part in PARTS}
    assert {mesh.get("name") for mesh in meshes} == expected_meshes
    assert all(_identity_transform(node) for node in nodes), "GLB node transforms changed"
    mesh_nodes = [node for node in nodes if "mesh" in node]
    assert len(mesh_nodes) == len(PARTS)
    assert {node.get("name") for node in mesh_nodes} == expected_meshes
    assert {node["mesh"] for node in mesh_nodes} == set(range(len(meshes)))
    assert len(document.get("buffers", [])) == 1
    views = document.get("bufferViews", [])
    view_hashes = {}

    def fingerprint_accessor(stream, accessor_index):
        accessor = accessors[accessor_index]
        assert "bufferView" in accessor and "sparse" not in accessor, accessor_index
        view_index = accessor["bufferView"]
        view = views[view_index]
        assert view.get("buffer", 0) == 0
        if view_index not in view_hashes:
            start = binary_start + view.get("byteOffset", 0)
            size = view["byteLength"]
            assert start >= binary_start and start + size <= binary_start + binary_length
            stream.seek(start)
            payload = stream.read(size)
            assert len(payload) == size, view_index
            view_hashes[view_index] = hashlib.sha256(payload).hexdigest()
        return {"accessor": accessor, "view": view, "view_sha256": view_hashes[view_index]}

    triangles = {}
    geometry_hashes = {}
    with GLB.open("rb") as stream:
        for index, mesh in enumerate(meshes):
            part = mesh["name"][3:]
            primitive, = mesh.get("primitives", [])
            expected_material = "M_" + ("inscription_decal" if part == "plaque_inscription" else part)
            assert materials[primitive["material"]].get("name") == expected_material, part
            assert primitive.get("mode", 4) == 4, part
            accessor = accessors[primitive["indices"]]
            assert accessor["count"] > 0 and accessor["count"] % 3 == 0, part
            assert any(node["mesh"] == index and node["name"] == mesh["name"] for node in mesh_nodes)
            triangles[part] = accessor["count"] // 3
            geometry = {
                "name": mesh["name"],
                "indices": fingerprint_accessor(stream, primitive["indices"]),
                "attributes": {
                    key: fingerprint_accessor(stream, accessor_index)
                    for key, accessor_index in sorted(primitive.get("attributes", {}).items())
                },
            }
            geometry_hashes[part] = hashlib.sha256(
                json.dumps(geometry, sort_keys=True, separators=(",", ":")).encode("utf-8")
            ).hexdigest()
    return triangles, geometry_hashes


def texture_sources(expected_names):
    files = {}
    for filename in sorted((SOURCE / "textures").iterdir()):
        if filename.suffix.lower() not in (".jpg", ".png"):
            continue
        assert filename.stem not in files, "Duplicate texture stem: " + filename.stem
        files[filename.stem] = filename
    assert set(files) == set(expected_names), (sorted(files), sorted(expected_names))
    return files


def _component_rows(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    result = {}
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        if not isinstance(component, unreal.StaticMeshComponent):
            continue
        if path(component) in seen:
            continue
        seen.add(path(component))
        name = component.get_name().split("_GEN_VARIABLE", 1)[0]
        assert name.startswith("SMC_") and name not in result, name
        position = component.get_editor_property("relative_location")
        rotation = component.get_editor_property("relative_rotation")
        scale = component.get_editor_property("relative_scale3d")
        result[name] = {
            "mesh": path(component.static_mesh),
            "material": path(component.get_material(0)),
            "location": [position.x, position.y, position.z],
            "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "scale": [scale.x, scale.y, scale.z],
            "collision": str(component.get_collision_profile_name()),
        }
    return result


def check_native_contract(data):
    assert data.get("validated") and data.get("blueprint") == BLUEPRINT_PATH
    assert len(data.get("meshes", [])) == len(PARTS)
    assert len(data.get("materials", {})) == len(PARTS)
    assert len(data.get("textures", {})) == 25
    blueprint = EAL.load_asset(BLUEPRINT_PATH)
    assert isinstance(blueprint, unreal.Blueprint) and blueprint.generated_class()
    rows = {row["part"]: row for row in data["meshes"]}
    assert set(rows) == set(PARTS)
    components = _component_rows(blueprint)
    assert set(components) == {"SMC_" + part for part in PARTS}, sorted(components)
    for part, row in rows.items():
        expected_mesh = MESH_DIR + "/WM_" + part + ".WM_" + part
        material_name = "M_" + ("inscription_decal" if part == "plaque_inscription" else part)
        expected_material = DEST + "/Materials/" + material_name + "." + material_name
        assert row["mesh"] == expected_mesh and row["material"] == expected_material, row
        assert components["SMC_" + part]["mesh"] == expected_mesh, part
        assert components["SMC_" + part]["material"] == expected_material, part
        assert isinstance(EAL.load_asset(expected_mesh), unreal.StaticMesh), expected_mesh
        assert isinstance(EAL.load_asset(expected_material), unreal.Material), expected_material
    for name, asset_path in data["textures"].items():
        expected = DEST + "/Textures/" + name + "." + name
        assert asset_path == expected and isinstance(EAL.load_asset(asset_path), unreal.Texture2D), name
    return blueprint, components, rows


def refresh_texture(name, filename, asset_path):
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = DEST + "/Textures"
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.replace_existing_settings = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert asset_path in [str(item) for item in task.imported_object_paths], (
        name, [str(item) for item in task.imported_object_paths]
    )
    texture = EAL.load_asset(asset_path)
    assert isinstance(texture, unreal.Texture2D) and path(texture) == asset_path, name
    normal = name.endswith("_Normal")
    orm = name.endswith("_ORM")
    texture.set_editor_property("srgb", not (normal or orm))
    texture.set_editor_property("compression_settings", (
        unreal.TextureCompressionSettings.TC_NORMALMAP if normal else
        unreal.TextureCompressionSettings.TC_MASKS if orm else
        unreal.TextureCompressionSettings.TC_DEFAULT
    ))
    if normal:
        texture.set_editor_property("flip_green_channel", True)
    assert EAL.save_loaded_asset(texture), name


def refresh_mesh(part, row, expected_triangles):
    mesh = EAL.load_asset(row["mesh"])
    before_triangles = mesh.get_num_triangles(0)
    params = unreal.ImportAssetParameters()
    params.reimport_asset = mesh
    params.is_automated = True
    params.replace_existing = True
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    imported = manager.import_asset(MESH_DIR, manager.create_source_data(str(GLB)), params)
    imported_paths = [path(item) for item in imported]
    assert row["mesh"] in imported_paths, (part, imported_paths)
    mesh = EAL.load_asset(row["mesh"])
    assert isinstance(mesh, unreal.StaticMesh) and path(mesh) == row["mesh"]
    slots = mesh.get_editor_property("static_materials")
    expected_name = "M_" + ("inscription_decal" if part == "plaque_inscription" else part)
    assert len(slots) == 1 and str(slots[0].material_slot_name) == expected_name, (
        part, [str(slot.material_slot_name) for slot in slots]
    )
    mesh.set_material(0, EAL.load_asset(row["material"]))
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", part in NANITE)
    if part in NANITE:
        nanite.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        nanite.set_editor_property("fallback_percent_triangles", 1.0)
        nanite.set_editor_property("fallback_relative_error", 0.0)
    mesh.set_editor_property("nanite_settings", nanite)
    if part in STRUCTURAL:
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("double_sided_geometry", True)
    assert EAL.save_loaded_asset(mesh), part
    fallback_triangles = mesh.get_num_triangles(0)
    assert fallback_triangles > 0, part
    # For Nanite meshes, GetNumTriangles(0) reports the generated raster
    # fallback, not the dense Nanite source. The import log and GLB preflight
    # establish the source count; only conventional meshes can be compared
    # directly to that count through this Python API.
    if part not in NANITE:
        assert abs(fallback_triangles - expected_triangles) <= max(100, expected_triangles * 0.01), (
            part, expected_triangles, fallback_triangles
        )
    return {"fallback_before": before_triangles, "fallback_after": fallback_triangles,
            "source": expected_triangles, "asset": row["mesh"]}


def main():
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), (
        "Save the current level before updating Wenmingmen"
    )
    source_data = json.loads(IMPORT_MANIFEST.read_text(encoding="utf-8"))
    blueprint, before_components, rows = check_native_contract(source_data)
    source_triangles, geometry_hashes = inspect_glb()
    textures = texture_sources(source_data["textures"])
    glb_hash = sha256_file(GLB)
    texture_hashes = {name: sha256_file(filename) for name, filename in textures.items()}
    prior = json.loads(UPDATE_REPORT.read_text(encoding="utf-8")) if UPDATE_REPORT.exists() else {}
    compatible = prior.get("validated") and prior.get("blueprint") == BLUEPRINT_PATH
    changed_parts = [part for part in PARTS if (
        not compatible or prior.get("mesh_geometry_sha256", {}).get(part) != geometry_hashes[part]
    )]
    changed_textures = [name for name in sorted(textures) if (
        not compatible or prior.get("texture_sha256", {}).get(name) != texture_hashes[name]
    )]
    print("WENMINGMEN_UPDATE_PLAN", json.dumps({
        "meshes": changed_parts,
        "textures": changed_textures,
        "source_triangles": source_triangles,
    }))
    for name in changed_textures:
        refresh_texture(name, textures[name], source_data["textures"][name])
        print("WENMINGMEN_TEXTURE_REFRESHED", name)
    mesh_results = {}
    if changed_parts:
        for part in changed_parts:
            mesh_results[part] = refresh_mesh(part, rows[part], source_triangles[part])
            print("WENMINGMEN_MESH_REFRESHED", part, mesh_results[part]["fallback_after"])
        assert EAL.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True)
    after_components = _component_rows(blueprint)
    assert after_components == before_components, (
        "Blueprint component references or transforms changed", before_components, after_components
    )
    source_data["update"] = {
        "glb_sha256": glb_hash,
        "texture_sha256": texture_hashes,
        "mesh_geometry_sha256": geometry_hashes,
        "source_triangles": source_triangles,
        "mesh_reimported": changed_parts,
        "textures_reimported": changed_textures,
    }
    validate(blueprint, source_data, completion_marker="WENMINGMEN_UPDATE_VALIDATED")
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    result = {
        "validated": True,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "blueprint": BLUEPRINT_PATH,
        "glb": str(GLB),
        "glb_sha256": glb_hash,
        "texture_sha256": texture_hashes,
        "mesh_geometry_sha256": geometry_hashes,
        "source_triangles": source_triangles,
        "meshes_reimported": changed_parts,
        "textures_reimported": changed_textures,
        "mesh_results": mesh_results,
        "component_contract_preserved": True,
        "bounds_cm": source_data["bounds_cm"],
    }
    UPDATE_REPORT.parent.mkdir(parents=True, exist_ok=True)
    temporary = UPDATE_REPORT.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(UPDATE_REPORT)
    print("WENMINGMEN_UPDATE_COMPLETE", json.dumps({
        "meshes_reimported": len(result["meshes_reimported"]),
        "textures_reimported": len(changed_textures),
        "blueprint": BLUEPRINT_PATH,
        "bounds_cm": result["bounds_cm"],
    }))


if __name__ == "__main__":
    main()
