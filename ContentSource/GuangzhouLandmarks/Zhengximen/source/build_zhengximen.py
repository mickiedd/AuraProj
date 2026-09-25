"""Rebuild the Zhengximen modular meshes from the fixed generator.

The package's own `generate_zhengximen_package.py` is a flat script: from
`builders={}` onward it runs the whole pipeline and writes to `/mnt/data`, so it
cannot simply be executed here. This driver loads only its *definitions* (everything
before that marker), calls `build_gate()` directly, and writes just the meshes the
Unreal import consumes.

Why a driver rather than running the generator: the generator would also regenerate
the 48 textures, the five FBX files, the preview renders and a zip, all aimed at
hardcoded `/mnt/data` paths. Only the geometry changed.

The geometry fix itself lives in the generator (the `wall` / `wall_end` /
`upper_wall` / `upper_wall_end` boxes), not here - per the tuning skill's rule that
the generator is the editable source and a binary mesh is never edited directly.

Usage:
    python build_zhengximen.py [--detail 4]
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path

import numpy as np
import trimesh

SOURCE = Path(__file__).resolve().parent
GENERATOR = SOURCE / "generate_zhengximen_package.py"
PROJECT = SOURCE.parents[3]
PACKAGE = (PROJECT / "Raw3DPacket" / "Zhengximen_GreatWestGate_UE5_Package"
           / "Zhengximen_GreatWestGate_UE5")
MESH = PACKAGE / "Meshes"
MODULAR = MESH / "Modular"
REPORT = PROJECT / "Saved" / "RawModelImport" / "zhengximen-source-rebuild.json"

# The generator was written on a machine where the package lived at
# /mnt/data/Zhengximen_GreatWestGate_UE5, and it mkdir's its whole output tree at
# import time. Pointing that at the real extracted package instead means the mkdirs
# are no-ops on existing directories and, importantly, get_pbr() can still find
# Textures/T_<material>_BaseColor.png. Nothing is written there: this driver only
# execs the definitions, never the pipeline that follows them.
ORIGINAL_ROOT_LITERAL = "Path('/mnt/data/Zhengximen_GreatWestGate_UE5')"

# The marker that separates the generator's definitions from its pipeline.
PIPELINE_MARKER = "\nbuilders={}"

WALL_MATERIAL = "AgedWood"
PAVILION = {"lower_half_width": 9.4, "lower_half_depth": 3.7,
            "upper_half_width": 7.85, "upper_half_depth": 2.9}


def load_definitions(generator=None):
    """exec everything above the pipeline marker and hand back its namespace."""
    generator = Path(generator) if generator else GENERATOR
    text = generator.read_text(encoding="utf-8")
    if PIPELINE_MARKER not in text:
        raise SystemExit("pipeline marker not found in " + str(generator))
    head = text.split(PIPELINE_MARKER, 1)[0]
    if ORIGINAL_ROOT_LITERAL in head:
        head = head.replace(ORIGINAL_ROOT_LITERAL,
                            "Path({!r})".format(str(PACKAGE)))
    namespace = {"__name__": "zhengximen_generator", "__file__": str(generator)}
    # The generator's texture pass also runs at module level, guarded by this flag,
    # and it needs CJK fonts that are not installed here. Only the geometry changed,
    # so skip it - the package's existing 48 maps are left untouched.
    os.environ["REUSE_TEXTURES"] = "1"
    exec(compile(head, str(generator), "exec"), namespace)  # noqa: S102
    for required in ("build_gate", "scene_from_builder", "mat_colors"):
        if required not in namespace:
            raise SystemExit("generator is missing " + required)
    return namespace


def arch_gaps(mesh, gate_half=2.25, spring=3.4, arch_r=2.25):
    """Rays aimed straight down the gateway: does the door close the arch?

    A ray fired at the arch opening from in front of the wall should stop on the
    door inside it. If it comes out the far side, or hits nothing, the vault is open
    above the gate - which is what a plain rectangular door under a taller arch
    leaves behind.
    """
    origins, directions, labels = [], [], []
    for z in np.linspace(0.35, spring + arch_r - 0.15, 9):
        half = gate_half if z < spring else min(
            gate_half, math.sqrt(max(0.0, arch_r ** 2 - (z - spring) ** 2)))
        for t in np.linspace(-0.85, 0.85, 15):
            origins.append((t * half, -7.0, z))
            directions.append((0.0, 1.0, 0.0))
            labels.append((round(float(t * half), 3), round(float(z), 3)))
    locations, index_ray, _ = mesh.ray.intersects_location(
        ray_origins=np.array(origins), ray_directions=np.array(directions),
        multiple_hits=False)
    first = {}
    for loc, ray_index in zip(locations, index_ray):
        first.setdefault(int(ray_index), float(loc[1]))
    open_samples = [labels[i] for i in range(len(origins))
                    if i not in first or first[i] > 0.0]
    return {"samples": len(origins), "open": len(open_samples),
            "open_examples": open_samples[:12]}


def door_fit(mesh, gate_half=2.25, spring=3.4, arch_r=2.25, samples=44):
    """How closely does the door meet the arc?

    Rays are aimed at fractions of the arch's half-width at each height, so this
    samples the seam between door and arch rather than the middle of the door. A
    leaf that only approximates the curve shows up here as open rays at 0.97-0.99 of
    the half-width, even though the centre of the door is solid.
    """
    fractions = (0.995, 0.98, 0.96, 0.93, 0.90)
    origins, directions, labels = [], [], []
    # Stops just short of the apex: the opening closes to a point there, so every
    # fraction collapses to x = 0 and the ray only grazes the leaf's top edge. That
    # is a property of the sample, not a gap.
    for i in range(samples + 1):
        z = (spring + arch_r) * 0.99 * i / samples
        if z < spring:
            half = gate_half
        else:
            half = min(gate_half, math.sqrt(max(0.0, arch_r ** 2 - (z - spring) ** 2)))
        for f in fractions:
            for sign in (-1.0, 1.0):
                origins.append((sign * f * half, -7.0, z))
                directions.append((0.0, 1.0, 0.0))
                labels.append((round(sign * f * half, 3), round(float(z), 3)))
    locations, index_ray, _ = mesh.ray.intersects_location(
        ray_origins=np.array(origins), ray_directions=np.array(directions),
        multiple_hits=False)
    first = {}
    for loc, ray_index in zip(locations, index_ray):
        first.setdefault(int(ray_index), float(loc[1]))
    open_samples = [labels[i] for i in range(len(origins))
                    if i not in first or first[i] > 0.0]
    return {"samples": len(origins), "open": len(open_samples),
            "open_examples": open_samples[:12]}


def glb_attributes(path):
    """The vertex attributes actually present in a GLB."""
    import struct
    with open(path, "rb") as handle:
        struct.unpack("<4sII", handle.read(12))
        chunk_length, _ = struct.unpack("<I4s", handle.read(8))
        document = json.loads(handle.read(chunk_length).decode("utf-8"))
    attributes = set()
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            attributes.update(primitive.get("attributes", {}).keys())
    return sorted(attributes)


def group_trimesh(builder, name):
    group = builder.groups[name]
    if not group.faces:
        return None
    return trimesh.Trimesh(vertices=np.array(group.vertices, float),
                           faces=np.array(group.faces, int),
                           process=False, validate=False)


# Which way a ray fired at each face travels, and therefore the sign of the
# coordinate the first hit must have if the wall is closed. A ray that reaches the
# far wall (or nothing) has passed straight through the building.
FACES = {
    "front": {"origin": lambda t, w, d, z: (t * w, -d - 3.0, z), "dir": (0, 1, 0),
              "expect": -1},
    "rear": {"origin": lambda t, w, d, z: (t * w, d + 3.0, z), "dir": (0, -1, 0),
             "expect": 1},
    "left": {"origin": lambda t, w, d, z: (-w - 3.0, t * d, z), "dir": (1, 0, 0),
             "expect": -1},
    "right": {"origin": lambda t, w, d, z: (w + 3.0, t * d, z), "dir": (-1, 0, 0),
              "expect": 1},
}


def face_gaps(mesh, face, half_width, half_depth, z0, z1, samples=220):
    """Fire rays perpendicular at one face and report the ones that pass through.

    This is the check that actually detects the "hollow pavilion" defect. A bounding
    box cannot: the broken version still had panels, just with a slot between each
    pair and nothing at all on the ends, so its bounds looked fine. A ray either
    meets the wall it was aimed at or it does not.
    """
    spec = FACES[face]
    span = np.linspace(-0.97, 0.97, samples)
    origins, directions, labels = [], [], []
    for z in np.linspace(z0, z1, 5):
        for t in span:
            origins.append(spec["origin"](t, half_width, half_depth, z))
            directions.append(spec["dir"])
            labels.append((round(float(t), 3), round(float(z), 3)))
    locations, index_ray, _ = mesh.ray.intersects_location(
        ray_origins=np.array(origins), ray_directions=np.array(directions),
        multiple_hits=False)
    first = {}
    for loc, ray_index in zip(locations, index_ray):
        first.setdefault(int(ray_index), loc)
    open_samples = []
    for i in range(len(origins)):
        if i not in first:
            open_samples.append(labels[i])
            continue
        # The coordinate that varies along the ray. The near wall sits on the
        # expected side of the building's midline, so a hit on that side means the
        # ray met the wall it was aimed at; a hit on the far side (or past the
        # midline) means it went through.
        axis = 0 if spec["dir"][0] else 1
        if float(first[i][axis]) * spec["expect"] <= 1.0:
            open_samples.append(labels[i])
    return {"samples": len(origins), "open": len(open_samples),
            "open_examples": open_samples[:12]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--detail", type=int, default=4)
    parser.add_argument("--generator", default=None,
                        help="check this generator instead of the staged one "
                             "(used to measure the pre-fix baseline)")
    parser.add_argument("--check-only", action="store_true",
                        help="run the wall checks without writing any mesh")
    args = parser.parse_args()

    namespace = load_definitions(args.generator)
    print("definitions loaded from", (args.generator or GENERATOR), flush=True)

    builder = namespace["build_gate"](args.detail)
    stats = {name: {"vertices": len(group.vertices), "faces": len(group.faces)}
             for name, group in builder.groups.items() if group.faces}
    print("built detail=%d: %d material groups" % (args.detail, len(stats)), flush=True)

    written = []
    if not args.check_only:
        TextureVisuals = namespace["TextureVisuals"]
        PBRMaterial = namespace["PBRMaterial"]
        MODULAR.mkdir(parents=True, exist_ok=True)
        for name, group in builder.groups.items():
            if not group.faces:
                continue
            mesh = trimesh.Trimesh(vertices=np.array(group.vertices),
                                   faces=np.array(group.faces), process=False)
            # UVs are the whole point of this export - see the note in the
            # generator's scene_from_builder. face_colors alone drops TEXCOORD_0.
            base = np.array(namespace["mat_colors"][name], float) / 255.0
            mesh.visual = TextureVisuals(
                uv=np.array(group.uvs, float),
                material=PBRMaterial(baseColorFactor=[base[0], base[1], base[2], 1.0]))
            out = MODULAR / ("SM_Zhengximen_{}.glb".format(name))
            out.write_bytes(trimesh.Scene(mesh).export(file_type="glb"))
            written.append({"material": name, "path": str(out),
                            "bytes": out.stat().st_size, **stats[name],
                            "attributes": glb_attributes(out)})

        lod0 = MESH / "SM_Zhengximen_LOD0.glb"
        scene = namespace["scene_from_builder"](builder, textured=False)
        lod0.write_bytes(scene.export(file_type="glb"))
        written.append({"material": "LOD0", "path": str(lod0),
                        "bytes": lod0.stat().st_size,
                        "attributes": glb_attributes(lod0)})
        print("wrote LOD0", lod0.stat().st_size, "bytes", flush=True)

        # A textured copy for review renders only. Not a package asset and not what
        # UE imports - UE binds its own 4K maps through the material instances - but
        # it is the only way to see, outside the engine, whether the UVs that were
        # just restored actually land the maps on the building.
        review = (PROJECT / "Saved" / "Reports" / "Zhengximen"
                  / "zhengximen-lod0-textured.glb")
        review.parent.mkdir(parents=True, exist_ok=True)
        textured = namespace["scene_from_builder"](builder, textured=True)
        review.write_bytes(textured.export(file_type="glb"))
        written.append({"material": "LOD0-textured", "path": str(review),
                        "bytes": review.stat().st_size,
                        "attributes": glb_attributes(review)})
        print("wrote textured review GLB", review.stat().st_size, "bytes", flush=True)

    missing_uv = [row["material"] for row in written
                  if "TEXCOORD_0" not in row.get("attributes", [])]
    if missing_uv:
        print("MISSING UVs on:", missing_uv, flush=True)
    else:
        print("UV check: all {} written GLBs carry TEXCOORD_0".format(len(written)),
              flush=True)

    # ---- verify the wall fix on the rebuilt geometry ------------------------
    wall = group_trimesh(builder, WALL_MATERIAL)
    checks = {"wall_material": WALL_MATERIAL}
    if wall is None:
        checks["error"] = "no geometry in " + WALL_MATERIAL
    else:
        for storey, half_w, half_d, z0, z1 in (
                ("lower", PAVILION["lower_half_width"], PAVILION["lower_half_depth"],
                 8.40, 10.70),
                ("upper", PAVILION["upper_half_width"], PAVILION["upper_half_depth"],
                 12.50, 14.50)):
            for face in FACES:
                key = "{}_{}".format(storey, face)
                checks[key] = face_gaps(wall, face, half_w, half_d, z0, z1)
                if checks[key]["open"]:
                    print("WALL OPEN: {} -> {} of {} rays pass through, e.g. {}".format(
                        key, checks[key]["open"], checks[key]["samples"],
                        checks[key]["open_examples"]), flush=True)

    open_total = sum(row["open"] for row in checks.values()
                     if isinstance(row, dict) and "open" in row)
    checks["open_total"] = open_total
    if wall is not None:
        checks["gateway"] = arch_gaps(wall)
        checks["door_fit"] = door_fit(wall)
        for key in ("gateway", "door_fit"):
            if checks[key]["open"]:
                print("{} STILL OPEN: {} of {} rays, e.g. {}".format(
                    key.upper(), checks[key]["open"], checks[key]["samples"],
                    checks[key]["open_examples"]), flush=True)
    print("TOTAL OPEN RAYS:", open_total, "| gateway",
          checks.get("gateway", {}).get("open", "n/a"), "| door_fit",
          checks.get("door_fit", {}).get("open", "n/a"), flush=True)

    report = {"generator": str(GENERATOR), "detail": args.detail,
              "package": str(PACKAGE), "written": written,
              "material_stats": stats, "wall_checks": checks}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("ZHENGXIMEN_REBUILD " + json.dumps({
        "report": str(REPORT), "meshes": len(written),
        "wall_checks": checks}, default=str))


if __name__ == "__main__":
    main()
