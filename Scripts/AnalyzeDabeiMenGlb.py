"""Offline structural analysis of GreatNorthGate_DabeiMen_UE5_Nanite_6M.glb.

Parses the GLB container with the standard library only (no engine, no glTF
dependency) so we can pick correct Interchange import flags before launching
the editor. Reports:

  * asset header / generator
  * per-mesh primitive counts, triangle counts, material bindings
  * per-node local transforms and mesh instancing
  * world-space AABB per mesh instance (column-major node matrices)
  * authored up-axis inference from the geometry distribution
  * material -> texture bindings

Usage:
    python Scripts/AnalyzeDabeiMenGlb.py <path-to.glb> [out.json]
"""

import json
import struct
import sys
from collections import defaultdict

COMPONENT_FORMAT = {
    5120: ("b", 1),
    5121: ("B", 1),
    5122: ("h", 2),
    5123: ("H", 2),
    5125: ("I", 4),
    5126: ("f", 4),
}

TYPE_COUNT = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}

MODE_NAMES = {0: "POINTS", 1: "LINES", 3: "LINE_STRIP", 4: "TRIANGLES",
              5: "TRIANGLE_STRIP", 6: "TRIANGLE_FAN"}


def load_glb(path):
    with open(path, "rb") as fh:
        data = fh.read()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ValueError("not a GLB container: magic=%r" % magic)
    offset = 12
    gltf = None
    bin_chunk = b""
    chunks = []
    while offset < length:
        clen, ctype = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8: offset + 8 + clen]
        chunks.append((ctype, clen))
        if ctype == 0x4E4F534A:
            gltf = json.loads(payload.decode("utf-8"))
        elif ctype == 0x004E4942:
            bin_chunk = payload
        offset += 8 + clen + ((4 - clen % 4) % 4)
    if gltf is None:
        raise ValueError("no JSON chunk found")
    return gltf, bin_chunk, version, length, chunks


def read_accessor(gltf, bin_chunk, index):
    acc = gltf["accessors"][index]
    fmt, size = COMPONENT_FORMAT[acc["componentType"]]
    ncomp = TYPE_COUNT[acc["type"]]
    count = acc["count"]
    if "bufferView" not in acc:
        return [0] * (count * ncomp)
    bv = gltf["bufferViews"][acc["bufferView"]]
    base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride") or (size * ncomp)
    out = []
    for i in range(count):
        off = base + i * stride
        out.extend(struct.unpack_from("<" + fmt * ncomp, bin_chunk, off))
    return out


def mat_identity():
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def mat_mul(a, b):
    """Column-major 4x4 multiply: result = a * b."""
    out = [0.0] * 16
    for c in range(4):
        for r in range(4):
            out[c * 4 + r] = sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
    return out


def trs_to_matrix(node):
    if "matrix" in node:
        return list(node["matrix"])
    t = node.get("translation", [0, 0, 0])
    r = node.get("rotation", [0, 0, 0, 1])
    s = node.get("scale", [1, 1, 1])
    x, y, z, w = r
    rot = [
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0,
        0, 0, 0, 1,
    ]
    for c in range(3):
        for r_i in range(3):
            rot[c * 4 + r_i] *= s[c]
    rot[12], rot[13], rot[14] = t[0], t[1], t[2]
    return rot


def xform_point(m, p):
    x, y, z = p
    return (
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
    )


def main():
    src = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else None

    gltf, bin_chunk, version, length, chunks = load_glb(src)
    meshes = gltf.get("meshes", [])
    nodes = gltf.get("nodes", [])
    materials = gltf.get("materials", [])
    accessors = gltf.get("accessors", [])
    textures = gltf.get("textures", [])
    images = gltf.get("images", [])

    report = {
        "source": src,
        "container": {
            "version": version,
            "declared_length": length,
            "chunks": [{"type": hex(t), "length": l} for t, l in chunks],
            "json_chunk_bytes": len(json.dumps(gltf)),
            "bin_chunk_bytes": len(bin_chunk),
        },
        "asset": gltf.get("asset", {}),
        "counts": {
            "meshes": len(meshes),
            "nodes": len(nodes),
            "materials": len(materials),
            "textures": len(textures),
            "images": len(images),
            "accessors": len(accessors),
        },
    }

    # ---- per-mesh geometry -------------------------------------------------
    mesh_info = []
    total_tris = 0
    total_verts = 0
    for mi, mesh in enumerate(meshes):
        prims = []
        mesh_tris = 0
        mesh_verts = 0
        mats = set()
        for prim in mesh.get("primitives", []):
            mode = prim.get("mode", 4)
            pos_idx = prim["attributes"].get("POSITION")
            vcount = accessors[pos_idx]["count"] if pos_idx is not None else 0
            if "indices" in prim:
                icount = accessors[prim["indices"]]["count"]
            else:
                icount = vcount
            if mode == 4:
                tris = icount // 3
            elif mode in (5, 6):
                tris = max(0, icount - 2)
            else:
                tris = 0
            has_uv = "TEXCOORD_0" in prim["attributes"]
            has_n = "NORMAL" in prim["attributes"]
            prims.append({
                "mode": MODE_NAMES.get(mode, str(mode)),
                "vertices": vcount,
                "indices": icount,
                "triangles": tris,
                "material_index": prim.get("material"),
                "material": materials[prim["material"]].get("name") if prim.get("material") is not None else None,
                "attributes": sorted(prim["attributes"].keys()),
                "has_uv0": has_uv,
                "has_normals": has_n,
                "position_min": accessors[pos_idx].get("min") if pos_idx is not None else None,
                "position_max": accessors[pos_idx].get("max") if pos_idx is not None else None,
            })
            mesh_tris += tris
            mesh_verts += vcount
            if prim.get("material") is not None:
                mats.add(prim["material"])
        mesh_info.append({
            "index": mi,
            "name": mesh.get("name", "<mesh_%d>" % mi),
            "primitive_count": len(prims),
            "triangles": mesh_tris,
            "vertices": mesh_verts,
            "material_indices": sorted(mats),
            "materials": [materials[i].get("name") for i in sorted(mats)],
            "primitives": prims,
        })
        total_tris += mesh_tris
        total_verts += mesh_verts

    report["meshes"] = mesh_info
    report["totals"] = {
        "source_triangles": total_tris,
        "source_vertices": total_verts,
        "meshes": len(meshes),
        "primitive_count": sum(m["primitive_count"] for m in mesh_info),
    }

    # ---- node graph --------------------------------------------------------
    parents = {}
    for ni, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child] = ni

    def world_matrix(ni, cache):
        if ni in cache:
            return cache[ni]
        local = trs_to_matrix(nodes[ni])
        pi = parents.get(ni)
        if pi is None:
            result = local
        else:
            result = mat_mul(world_matrix(pi, cache), local)
        cache[ni] = result
        return result

    cache = {}
    instances = defaultdict(list)
    node_info = []
    world_min = [float("inf")] * 3
    world_max = [float("-inf")] * 3
    for ni, node in enumerate(nodes):
        m = world_matrix(ni, cache)
        entry = {
            "index": ni,
            "name": node.get("name", "<node_%d>" % ni),
            "mesh": node.get("mesh"),
            "mesh_name": meshes[node["mesh"]].get("name") if node.get("mesh") is not None else None,
            "has_matrix": "matrix" in node,
            "translation": node.get("translation"),
            "rotation": node.get("rotation"),
            "scale": node.get("scale"),
            "children": node.get("children", []),
            "parent": parents.get(ni),
            "world_translation": [m[12], m[13], m[14]],
        }
        if node.get("mesh") is not None:
            instances[node["mesh"]].append(ni)
            # world AABB from the accessor min/max corners
            aabb_min = None
            aabb_max = None
            for prim in meshes[node["mesh"]].get("primitives", []):
                pidx = prim["attributes"].get("POSITION")
                if pidx is None:
                    continue
                amin = accessors[pidx].get("min")
                amax = accessors[pidx].get("max")
                if amin is None or amax is None:
                    continue
                corners = [(amin[0] if a == 0 else amax[0],
                            amin[1] if b == 0 else amax[1],
                            amin[2] if c == 0 else amax[2])
                           for a in (0, 1) for b in (0, 1) for c in (0, 1)]
                for corner in corners:
                    wp = xform_point(m, corner)
                    if aabb_min is None:
                        aabb_min = list(wp)
                        aabb_max = list(wp)
                    else:
                        aabb_min = [min(aabb_min[i], wp[i]) for i in range(3)]
                        aabb_max = [max(aabb_max[i], wp[i]) for i in range(3)]
            entry["world_aabb_min"] = aabb_min
            entry["world_aabb_max"] = aabb_max
            if aabb_min:
                world_min = [min(world_min[i], aabb_min[i]) for i in range(3)]
                world_max = [max(world_max[i], aabb_max[i]) for i in range(3)]
        node_info.append(entry)

    report["nodes"] = node_info
    report["instancing"] = {
        meshes[mi].get("name", "<mesh_%d>" % mi): {
            "node_count": len(nis),
            "nodes": nis,
            "rendered_triangles": mesh_info[mi]["triangles"] * len(nis),
        }
        for mi, nis in sorted(instances.items())
        if len(nis) > 1
    }
    report["scene_world_aabb"] = {
        "min": [round(v, 4) for v in world_min],
        "max": [round(v, 4) for v in world_max],
        "size": [round(world_max[i] - world_min[i], 4) for i in range(3)],
    }

    # ---- up-axis inference -------------------------------------------------
    size = [world_max[i] - world_min[i] for i in range(3)]
    # Rank all three axes - the vertical axis is the one that is neither of the
    # two widest.  Considering only X and Y here would misclassify a tall narrow
    # object whose height happens to exceed its depth.
    ranked = sorted([("x", size[0]), ("y", size[1]), ("z", size[2])], key=lambda kv: -kv[1])
    widest = [ranked[0][0], ranked[1][0]]
    vertical = ranked[2][0]
    if widest == ["x", "y"]:
        authored = "Y-up (glTF native, no correction needed)"
    elif widest == ["x", "z"]:
        authored = "Z-up (needs -90 deg roll on Interchange import)"
    elif widest == ["y", "z"]:
        authored = "X-up (unusual - inspect before importing)"
    else:
        authored = "ambiguous"
    report["axis_inference"] = {
        "aabb_size_xyz": [round(v, 4) for v in size],
        "widest_axes": widest,
        "narrowest_axis": vertical,
        "authored_up_axis": authored,
        "note": ("glTF convention is Y-up. When the two widest extents are X and Z "
                 "the content is authored Z-up and needs a -90 deg roll on import "
                 "(with bake_meshes=True, or Interchange ignores the rotation)."),
    }

    # ---- materials ---------------------------------------------------------
    mat_info = []
    for mi, mat in enumerate(materials):
        pbr = mat.get("pbrMetallicRoughness", {})
        tex_slots = {}
        if "baseColorTexture" in pbr:
            ti = pbr["baseColorTexture"]["index"]
            tex_slots["baseColor"] = {
                "texture": textures[ti].get("name") if ti < len(textures) else None,
                "image": images[textures[ti]["source"]].get("name") if "source" in textures[ti] else None,
                "mime": images[textures[ti]["source"]].get("mimeType") if "source" in textures[ti] else None,
            }
        if "metallicRoughnessTexture" in pbr:
            ti = pbr["metallicRoughnessTexture"]["index"]
            tex_slots["metallicRoughness"] = {
                "image": images[textures[ti]["source"]].get("name") if "source" in textures[ti] else None,
            }
        if "normalTexture" in mat:
            ti = mat["normalTexture"]["index"]
            tex_slots["normal"] = {
                "image": images[textures[ti]["source"]].get("name") if "source" in textures[ti] else None,
            }
        if "occlusionTexture" in mat:
            ti = mat["occlusionTexture"]["index"]
            tex_slots["occlusion"] = {
                "image": images[textures[ti]["source"]].get("name") if "source" in textures[ti] else None,
            }
        if "emissiveTexture" in mat:
            ti = mat["emissiveTexture"]["index"]
            tex_slots["emissive"] = {
                "image": images[textures[ti]["source"]].get("name") if "source" in textures[ti] else None,
            }
        mat_info.append({
            "index": mi,
            "name": mat.get("name", "<material_%d>" % mi),
            "double_sided": mat.get("doubleSided"),
            "alpha_mode": mat.get("alphaMode", "OPAQUE"),
            "base_color_factor": pbr.get("baseColorFactor"),
            "metallic_factor": pbr.get("metallicFactor"),
            "roughness_factor": pbr.get("roughnessFactor"),
            "textures": tex_slots,
        })
    report["materials"] = mat_info
    report["images"] = [{"index": i, "name": im.get("name"), "mime": im.get("mimeType"),
                         "buffer_view_bytes": gltf["bufferViews"][im["bufferView"]]["byteLength"] if "bufferView" in im else None}
                        for i, im in enumerate(images)]

    text = json.dumps(report, indent=2)
    if out_path:
        with open(out_path, "w", encoding="utf-8") as fh:
            fh.write(text)
    print(text)


if __name__ == "__main__":
    main()
