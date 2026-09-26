"""Prepare the Zhengdongmen / Great East Gate package for Unreal import.

Runs on the plain Python venv (no `unreal` module): splits the single combined
LOD0 GLB into one GLB per material group and stages a 4K texture set.

WHY SPLIT THE GLB
-----------------
`Zhengdongmen_LOD0.glb` is one file holding seven meshes, one per material, all
at identity. Imported combined it becomes one StaticMesh with seven slots, and
the slot order is then whatever Interchange happened to produce. Split into one
GLB per material it becomes seven StaticMeshes with exactly one slot each, so the
material binding is by construction - the same reasoning that drove
ImportZhengximenLandmark.py to use that package's per-material modular GLBs.

The split keeps the source buffer, accessors, bufferViews, images and materials
untouched and only prunes the scene graph (meshes / nodes / scene), so every
retained mesh is byte-identical to the source. Unused accessors are legal in
glTF and are simply never referenced.

WHY 4K BASE COLOUR AND NOT THE SHIPPED 8K
-----------------------------------------
The Compact package ships BaseColor at 8192x8192, but its own README says those
were upscaled from ~1254px illustrations. Measured rather than assumed - an
8K -> 4K -> 8K round trip reproduces the 8K to a mean absolute error of
0.49-0.62 of 255 (JPEG-noise level), and a 2K round trip to 0.69-0.93. So the 8K
carries no detail above ~2K, and 4K already oversamples the source by 3.3x. The
staged 4K set is therefore visually identical to the 8K at a quarter of the
runtime memory, and matches the project's existing 4K landmark convention.

Support maps are the Compact package's 1K PNGs, used as shipped. They were
generated to match the Compact artwork; pairing them with the Procedural
package's 4K normals would mix two different illustration sets on one material.

WHY THE COMPACT ARTWORK AND NOT THE PROCEDURAL ONE
--------------------------------------------------
Both packages carry identical geometry (verified: same 7 meshes, same 50,442
triangles, same POSITION bounds). They differ only in texture artwork. The
Compact set is the newer, far more convincing illustration: real masonry courses
with mortar and weathering, curved ceramic roof-tile profiles, timber grain,
weathered limewash, and a timber door with iron studs and ring handles - against
the Procedural set's flat procedural swatches.

The staged meshes are then updated by TuneZhengdongmenReference.py with the
maintained continuous walls, intact arch and fitted doors. The byte-identical
split described above is the preserved baseline step, not the final mesh.

Outputs (both under Raw3DPacket/Zhengdongmen/prepared/):
  Meshes/SM_ZDM_<Group>.glb            one per material group
  Textures/T_ZDM_<Group>_<Kind>.png    BaseColor 4K, other channels 1K
"""

import json
import struct
import runpy
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
STAGE = PROJECT / "Raw3DPacket" / "Zhengdongmen"
MESH_SOURCE = STAGE / "Zhengdongmen_UE5_Compact_8K" / "Meshes" / "Zhengdongmen_LOD0.glb"
TEX_SOURCE = STAGE / "Zhengdongmen_UE5_Compact_8K" / "Textures"
OUT = STAGE / "prepared"

# Material groups in the GLB's own mesh order. The name after SM_ZDM_LOD0_ is the
# material name without its M_ZDM_ prefix. Ridge is the one group the supplied
# package has no artwork for — the roof ridges were sharing the RoofTile material —
# so its textures are authored here instead of being sourced.
GROUPS = ["Stone", "Wood", "RoofTile", "Ridge", "Plaster", "Iron", "DoorWood", "Sign"]
GENERATED = {"Ridge": "Scripts/MakeZhengdongmenRidgeTextures.py"}

# Compact package filenames are 8K for BaseColor and 1K for everything else.
CHANNELS = {
    "BaseColor": ("T_ZDM_{g}_BaseColor_8K.jpg", 4096),
    "Normal": ("T_ZDM_{g}_Normal_1K.png", None),
    "Roughness": ("T_ZDM_{g}_Roughness_1K.png", None),
    "Metallic": ("T_ZDM_{g}_Metallic_1K.png", None),
    "AO": ("T_ZDM_{g}_AO_1K.png", None),
    "Height": ("T_ZDM_{g}_Height_1K.png", None),
}


def read_glb(path):
    with open(path, "rb") as handle:
        magic, version, _ = struct.unpack("<4sII", handle.read(12))
        assert magic == b"glTF", (path, magic)
        assert version == 2, version
        chunk_length, chunk_type = struct.unpack("<I4s", handle.read(8))
        assert chunk_type == b"JSON", chunk_type
        document = json.loads(handle.read(chunk_length).decode("utf-8"))
        bin_length, bin_type = struct.unpack("<I4s", handle.read(8))
        assert bin_type == b"BIN\x00", bin_type
        binary = handle.read(bin_length)
    return document, binary


def write_glb(path, document, binary):
    payload = json.dumps(document, separators=(",", ":")).encode("utf-8")
    payload += b" " * ((4 - len(payload) % 4) % 4)
    body = binary + b"\x00" * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(payload) + 8 + len(body)
    with open(path, "wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, total))
        handle.write(struct.pack("<I4s", len(payload), b"JSON"))
        handle.write(payload)
        handle.write(struct.pack("<I4s", len(body), b"BIN\x00"))
        handle.write(body)


def split_meshes():
    document, binary = read_glb(MESH_SOURCE)
    mesh_dest = OUT / "Meshes"
    mesh_dest.mkdir(parents=True, exist_ok=True)

    # Map each source mesh to the node that instantiates it, so the split scene
    # keeps the original node name (which becomes the imported asset name).
    node_for_mesh = {}
    for index, node in enumerate(document.get("nodes", [])):
        if "mesh" in node:
            node_for_mesh.setdefault(node["mesh"], index)

    written = []
    for index, mesh in enumerate(document["meshes"]):
        name = mesh.get("name", "")
        group = next((g for g in GROUPS if name.endswith("_" + g)), None)
        if group is None:
            raise SystemExit("mesh {!r} does not map to a material group".format(name))
        source_node = node_for_mesh.get(index)
        if source_node is None:
            raise SystemExit("mesh {!r} is not referenced by any node".format(name))

        split = dict(document)
        # Keep every accessor / bufferView / material / image: unused ones are
        # legal and pruning them would risk breaking a shared reference. Only the
        # scene graph is rewritten, so the retained mesh is byte-identical.
        split["meshes"] = [mesh]
        split["nodes"] = [dict(document["nodes"][source_node], mesh=0)]
        split["nodes"][0].pop("children", None)
        split["scenes"] = [{"name": "Scene", "nodes": [0]}]
        split["scene"] = 0

        target = mesh_dest / "SM_ZDM_{}.glb".format(group)
        write_glb(target, split, binary)
        written.append({"group": group, "file": target.name,
                        "source_mesh": name, "triangles":
                        document["accessors"][mesh["primitives"][0]["indices"]]["count"] // 3,
                        "bytes": target.stat().st_size})
    return written


def stage_textures():
    tex_dest = OUT / "Textures"
    tex_dest.mkdir(parents=True, exist_ok=True)
    written = []
    for group in GROUPS:
        if group in GENERATED:
            continue
        for kind, (pattern, resize) in CHANNELS.items():
            source = TEX_SOURCE / pattern.format(g=group)
            if not source.exists():
                raise SystemExit("missing texture " + str(source))
            target = tex_dest / "T_ZDM_{}_{}.png".format(group, kind)
            with Image.open(source) as image:
                image = image.convert("RGB")
                if resize:
                    image = image.resize((resize, resize), Image.LANCZOS)
                image.save(target, "PNG", optimize=True)
            written.append({"group": group, "kind": kind, "file": target.name,
                            "source": source.name,
                            "size": list(Image.open(target).size),
                            "bytes": target.stat().st_size})
    # The groups with no artwork in the supplied package are authored here, so a
    # rebuild never silently falls back to someone else's maps.
    for group, script in GENERATED.items():
        runpy.run_path(str(PROJECT / script))["main"](tex_dest)
        for kind in CHANNELS:
            target = tex_dest / "T_ZDM_{}_{}.png".format(group, kind)
            written.append({"group": group, "kind": kind, "file": target.name,
                            "source": script, "authored": True,
                            "size": list(Image.open(target).size),
                            "bytes": target.stat().st_size})
    # The shipped plaque letterboxes three simplified characters into a square
    # canvas that the 4.34:1 quad then stretches. Replace the whole Sign set with
    # the board authored at its own aspect ratio and in traditional forms.
    plaque = runpy.run_path(str(PROJECT / "Scripts/MakeZhengdongmenPlaque.py"))["main"](tex_dest)
    for row in written:
        if row["group"] == "Sign":
            target = tex_dest / row["file"]
            row.update({"source": "MakeZhengdongmenPlaque.py", "size": list(Image.open(target).size),
                        "bytes": target.stat().st_size, "plaque": plaque["glyph"]})
    return written


def main():
    meshes = split_meshes()
    textures = stage_textures()
    # Apply the maintained reference repair after restoring the package baseline.
    runpy.run_path(str(PROJECT / "Scripts/TuneZhengdongmenReference.py"))["main"]()
    manifest = {
        "source_mesh_glb": str(MESH_SOURCE.relative_to(PROJECT)),
        "source_textures": str(TEX_SOURCE.relative_to(PROJECT)),
        "groups": GROUPS,
        "reference_tuned_triangles": json.loads((OUT / "reference-tuning.json").read_text())["total_triangles"],
        "meshes": meshes,
        "textures": textures,
        "total_source_triangles": sum(row["triangles"] for row in meshes),
        "basecolor_note": "staged at 4096; the shipped 8192 is a ~1254px upscale",
    }
    (OUT / "prepare-report.json").write_text(json.dumps(manifest, indent=2),
                                             encoding="utf-8")
    print(json.dumps({"meshes": len(meshes), "textures": len(textures),
                      "triangles": manifest["total_source_triangles"],
                      "out": str(OUT)}, indent=2))
    for row in meshes:
        print("  {:9s} {:>6d} tris  {:>8d} B  {}".format(
            row["group"], row["triangles"], row["bytes"], row["file"]))


if __name__ == "__main__":
    main()
