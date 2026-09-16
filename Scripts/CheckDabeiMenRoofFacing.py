"""Check whether the DabeiMen roof mesh actually has upward-facing surface area.

The isolated overhead capture showed the GNG_ROOF part rendering as thin strips
rather than a solid roof plate, which usually means one of two things:

  * the shell's faces are wound inside-out (fixable: render the material
    two-sided), or
  * the shell only has an underside/soffit and the top tile surface is missing
    from the source model (not fixable at import time).

This script answers that from the source GLB geometry, offline, by sampling
triangles and measuring their area-weighted geometric normal direction, and by
comparing the geometric normal against the authored vertex normals.

Usage:
    python Scripts/CheckDabeiMenRoofFacing.py <glb> [out.json]
"""
import json
import struct
import sys
from array import array

COMPONENT_FORMAT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}

SAMPLE_TRIANGLES = 60000


def load_glb(path):
    with open(path, "rb") as fh:
        data = fh.read()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF", magic
    offset, gltf, bin_chunk = 12, None, b""
    while offset < length:
        clen, ctype = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8: offset + 8 + clen]
        if ctype == 0x4E4F534A:
            gltf = json.loads(payload.decode("utf-8"))
        elif ctype == 0x004E4942:
            bin_chunk = payload
        offset += 8 + clen + ((4 - clen % 4) % 4)
    return gltf, bin_chunk


def read_bulk(gltf, bin_chunk, index):
    """Read an accessor as a flat python array, fast-path for tightly packed data."""
    acc = gltf["accessors"][index]
    fmt = COMPONENT_FORMAT[acc["componentType"]]
    ncomp = TYPE_COUNT[acc["type"]]
    count = acc["count"]
    bv = gltf["bufferViews"][acc["bufferView"]]
    base = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride") or (struct.calcsize(fmt) * ncomp)
    item_size = struct.calcsize(fmt) * ncomp
    if stride == item_size:
        # tightly packed: one bulk unpack
        buf = bin_chunk[base: base + count * item_size]
        out = array(fmt)
        out.frombytes(buf)
        return out, ncomp
    out = array(fmt)
    for i in range(count):
        out.extend(struct.unpack_from("<" + fmt * ncomp, bin_chunk, base + i * stride))
    return out, ncomp


def main():
    src = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else None

    gltf, bin_chunk = load_glb(src)
    meshes = gltf["meshes"]
    roof_index = next(i for i, m in enumerate(meshes) if "ROOF" in (m.get("name") or "").upper())
    mesh = meshes[roof_index]
    prim = mesh["primitives"][0]

    pos, _ = read_bulk(gltf, bin_chunk, prim["attributes"]["POSITION"])
    nrm, _ = read_bulk(gltf, bin_chunk, prim["attributes"]["NORMAL"])
    idx, _ = read_bulk(gltf, bin_chunk, prim["indices"])

    tri_count = len(idx) // 3
    step = max(1, tri_count // SAMPLE_TRIANGLES)

    up_area = down_area = side_area = 0.0
    winding_agrees = winding_opposes = winding_side = 0
    up_author_area = down_author_area = 0.0

    for t in range(0, tri_count, step):
        i0, i1, i2 = idx[3 * t], idx[3 * t + 1], idx[3 * t + 2]
        ax, ay, az = pos[3 * i0], pos[3 * i0 + 1], pos[3 * i0 + 2]
        bx, by, bz = pos[3 * i1], pos[3 * i1 + 1], pos[3 * i1 + 2]
        cx, cy, cz = pos[3 * i2], pos[3 * i2 + 1], pos[3 * i2 + 2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx = uy * vz - uz * vy
        ny = uz * vx - ux * vz
        nz = ux * vy - uy * vx
        area = 0.5 * (nx * nx + ny * ny + nz * nz) ** 0.5
        if area <= 0.0:
            continue
        # Source space is Z-up, so the roof's outward face is +Z.
        nzu = nz / (2.0 * area)
        if nzu > 0.25:
            up_area += area
        elif nzu < -0.25:
            down_area += area
        else:
            side_area += area

        # Authored vertex normal (normalised average of the three corners).
        anx = (nrm[3 * i0] + nrm[3 * i1] + nrm[3 * i2]) / 3.0
        any_ = (nrm[3 * i0 + 1] + nrm[3 * i1 + 1] + nrm[3 * i2 + 1]) / 3.0
        anz = (nrm[3 * i0 + 2] + nrm[3 * i1 + 2] + nrm[3 * i2 + 2]) / 3.0
        alen = (anx * anx + any_ * any_ + anz * anz) ** 0.5
        if alen > 1e-9:
            dot = (nx * anx + ny * any_ + nz * anz) / (2.0 * area * alen)
            if dot > 0.5:
                winding_agrees += 1
            elif dot < -0.5:
                winding_opposes += 1
            else:
                winding_side += 1
            if anz / alen > 0.25:
                up_author_area += area
            elif anz / alen < -0.25:
                down_author_area += area

    total = up_area + down_area + side_area
    sampled = winding_agrees + winding_opposes + winding_side
    report = {
        "source": src,
        "roof_mesh": mesh.get("name"),
        "source_triangles": tri_count,
        "sampled_triangles": sampled,
        "sampling_step": step,
        "geometric_normal_area_by_direction": {
            "up_percent": round(100.0 * up_area / total, 3),
            "down_percent": round(100.0 * down_area / total, 3),
            "side_percent": round(100.0 * side_area / total, 3),
        },
        "authored_normal_area_by_direction": {
            "up_percent": round(100.0 * up_author_area / total, 3),
            "down_percent": round(100.0 * down_author_area / total, 3),
        },
        "winding_vs_authored_normals": {
            "agrees_percent": round(100.0 * winding_agrees / sampled, 3),
            "opposes_percent": round(100.0 * winding_opposes / sampled, 3),
            "grazing_percent": round(100.0 * winding_side / sampled, 3),
        },
    }
    report["conclusion"] = (
        "faces point up: roof has a top surface"
        if report["geometric_normal_area_by_direction"]["up_percent"] > 25.0
        else "no meaningful upward-facing area: roof top surface is missing or inverted")
    text = json.dumps(report, indent=2)
    if out_path:
        with open(out_path, "w", encoding="utf-8") as fh:
            fh.write(text)
    print(text)


if __name__ == "__main__":
    main()
