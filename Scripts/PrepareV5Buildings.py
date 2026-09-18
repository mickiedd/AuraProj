"""Safely extract and inventory the three Raw3DModels/V5 packages.

The generated manifest is the source-of-truth for the Unreal import.  It keeps
the two Wuxianmen archives independent so each archive becomes its own
placeable Blueprint and records enough GLB metadata to prove that every mesh
instance was preserved.
"""
from __future__ import annotations

import hashlib
import json
import struct
import zipfile
from copy import deepcopy
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
SOURCE = Path("C:/Works/Raw3DModels/V5")
ROOT = PROJECT / "Saved/RawModelImport/V5"

PACKAGES = (
    {
        "archive": "Guidemen_GuideGate_UE5_ModelPlusGenerated4KMaterials.zip",
        "name": "Guidemen_V5_4K",
        "source": "Model/Guidemen_GuideGate_UE5_100M_Instanced.glb",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K",
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-5056c754-f7a5-4fa2-b351-3e6a30c55e5f.png",
        "expected_dimensions_cm": [5600.0, 1800.0, 1930.0],
        "import_offset_roll": 0.0,
        "blueprint_root_rotation": [0.0, 0.0, 0.0],
        "material_maps": {
            "M_WeatheredStone_4K": {"BaseColor": "Materials/SourcePBR/M_WeatheredStone_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_WeatheredStone_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_WeatheredStone_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_WeatheredStone_AO.jpg"},
            "M_AgedTimber_4K": {"BaseColor": "Materials/SourcePBR/M_AgedTimber_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_AgedTimber_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_AgedTimber_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_AgedTimber_AO.jpg"},
            "M_GateDoor_4K": {"BaseColor": "Materials/SourcePBR/M_GateDoor_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_GateDoor_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_GateDoor_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_GateDoor_AO.jpg"},
            "M_GrayClayTile_2K": {"BaseColor": "Materials/SourcePBR/M_GrayClayTile_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_GrayClayTile_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_GrayClayTile_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_GrayClayTile_AO.jpg"},
            "M_AgedPlaster_2K": {"BaseColor": "Materials/SourcePBR/M_AgedPlaster_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_AgedPlaster_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_AgedPlaster_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_AgedPlaster_AO.jpg"},
            "M_AgedMetal_Solid": {"BaseColor": "Materials/SourcePBR/M_AgedMetal_BaseColor.jpg", "Normal": "Materials/SourcePBR/M_AgedMetal_Normal.jpg", "MetallicRoughness": "Materials/SourcePBR/M_AgedMetal_MetallicRoughness.jpg", "AO": "Materials/SourcePBR/M_AgedMetal_AO.jpg"},
            "M_Sign_Guidemen_2K": {"BaseColor": "Materials/SourcePBR/M_Sign_Guidemen.jpg"},
            "M_Plaque_PorteDeGuide_2K": {"BaseColor": "Materials/SourcePBR/M_Plaque_PorteDeGuide.jpg"},
        },
    },
    {
        "archive": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials.zip",
        "name": "Wuxianmen_V5_4K_Core",
        "source": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Wuxianmen_UE5_50M_Instanced.glb",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core",
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-10d91708-3a42-4ffb-9459-30b4b4de0403.png",
        "expected_dimensions_cm": [3600.0, 720.0, 1670.0],
        "import_offset_roll": -90.0,
        "blueprint_root_rotation": [0.0, 0.0, 0.0],
        "material_maps": {
            "M_Stone": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Stone_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Stone_Normal_4K.png", "Roughness": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Stone_Roughness_4K.png"},
            "M_RoofTile": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Roof_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Roof_Normal_4K.png", "Roughness": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Roof_Roughness_4K.png"},
            "M_Wood": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Wood_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Wood_Normal_4K.png", "Roughness": "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Textures_4K/Wood_Roughness_4K.png"},
        },
    },
    {
        "archive": "Wuxianmen_UE5_HighDetail_Package.zip",
        "name": "Wuxianmen_V5_FullPBR",
        "source": "Wuxianmen_UE5_HighDetail_Package/Wuxianmen_UE5_50M_Instanced.glb",
        "destination": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR",
        "reference": "C:/Users/mickie/AppData/Local/Temp/codex-clipboard-10d91708-3a42-4ffb-9459-30b4b4de0403.png",
        "expected_dimensions_cm": [3600.0, 720.0, 1670.0],
        "import_offset_roll": -90.0,
        "blueprint_root_rotation": [0.0, 0.0, 0.0],
        "material_maps": {
            "M_Stone": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Stone_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Stone_Normal_4K.png", "MetallicRoughness": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Stone_MetallicRoughness_4K.png"},
            "M_RoofTile": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/RoofTile_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/RoofTile_Normal_4K.png", "MetallicRoughness": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/RoofTile_MetallicRoughness_4K.png"},
            "M_Wood": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Wood_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Wood_Normal_4K.png", "MetallicRoughness": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Wood_MetallicRoughness_4K.png"},
            "M_Plaster": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Plaster_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Plaster_Normal_4K.png", "MetallicRoughness": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Plaster_MetallicRoughness_4K.png"},
            "M_Iron": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Iron_BaseColor_4K.png", "Normal": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Iron_Normal_4K.png", "MetallicRoughness": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Iron_MetallicRoughness_4K.png"},
            "M_Plaque_Wuxianmen": {"BaseColor": "Wuxianmen_UE5_HighDetail_Package/Textures_4K/Plaque_Wuxianmen_BaseColor_2K.png"},
        },
    },
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _safe_extract(archive: Path) -> None:
    destination = ROOT.resolve()
    with zipfile.ZipFile(archive) as handle:
        for member in handle.infolist():
            target = (ROOT / member.filename).resolve()
            assert target.is_relative_to(destination), member.filename
        handle.extractall(ROOT)


def _glb_json(path: Path) -> dict:
    with path.open("rb") as handle:
        header = handle.read(20)
        magic, version, _, json_length, chunk_type = struct.unpack("<IIIII", header)
        assert magic == 0x46546C67 and version == 2 and chunk_type == 0x4E4F534A, path
        return json.loads(handle.read(json_length).decode("utf-8").rstrip(" \x00"))


def _normalize_png(package_name: str, relative: str) -> str:
    """Re-encode archive PNGs whose payloads decode but contain bad CRCs."""
    from PIL import Image, ImageFile

    source = ROOT / relative
    destination = ROOT / "RepairedTextures" / package_name / (source.stem + "_Repaired.png")
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        ImageFile.LOAD_TRUNCATED_IMAGES = True
        with Image.open(source) as image:
            image.load()
            image.save(destination, format="PNG", optimize=False)
    return destination.relative_to(ROOT).as_posix()


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    manifest = []
    for source_template in PACKAGES:
        source = deepcopy(source_template)
        archive = SOURCE / source["archive"]
        assert archive.is_file(), archive
        _safe_extract(archive)
        glb = ROOT / source["source"]
        assert glb.is_file(), glb
        doc = _glb_json(glb)
        material_names = [entry.get("name", f"Material_{index}") for index, entry in enumerate(doc.get("materials", []))]
        mesh_names = [entry.get("name", f"Mesh_{index}") for index, entry in enumerate(doc.get("meshes", []))]
        mesh_nodes = [node for node in doc.get("nodes", []) if "mesh" in node]
        referenced_meshes = sorted({node["mesh"] for node in mesh_nodes})
        repaired = {}
        for maps in source["material_maps"].values():
            for channel, relative in list(maps.items()):
                if relative.lower().endswith(".png"):
                    normalized = _normalize_png(source["name"], relative)
                    repaired[relative] = normalized
                    maps[channel] = normalized
        item = dict(source)
        item.update(
            archive_path=str(archive),
            archive_sha256=_sha256(archive),
            archive_bytes=archive.stat().st_size,
            root=str(ROOT),
            source_path=str(glb),
            source_sha256=_sha256(glb),
            source_bytes=glb.stat().st_size,
            glb_mesh_count=len(mesh_names),
            glb_referenced_mesh_count=len(referenced_meshes),
            glb_unreferenced_mesh_indices=sorted(set(range(len(mesh_names))) - set(referenced_meshes)),
            glb_mesh_names=mesh_names,
            glb_node_count=len(doc.get("nodes", [])),
            glb_mesh_instance_count=len(mesh_nodes),
            glb_material_names=material_names,
            glb_scene_count=len(doc.get("scenes", [])),
            repaired_textures=repaired,
        )
        for maps in item["material_maps"].values():
            for relative in maps.values():
                assert (ROOT / relative).is_file(), relative
        manifest.append(item)
    (ROOT / "packages.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps([
        {
            "name": item["name"],
            "meshes": item["glb_mesh_count"],
            "referenced_meshes": item["glb_referenced_mesh_count"],
            "instances": item["glb_mesh_instance_count"],
            "materials": item["glb_material_names"],
            "sha256": item["archive_sha256"],
        }
        for item in manifest
    ], indent=2))


if __name__ == "__main__":
    main()
