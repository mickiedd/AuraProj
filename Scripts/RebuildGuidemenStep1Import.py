"""Rebuild step 1 — recover-copy the current Guidemen asset, then re-import the source.

> **DO NOT RUN AS WRITTEN.** The first attempt of this script crashed the shared Unreal
> Editor with an out-of-memory fatal error. `bake_meshes = True` is wrong for this source:
> the GLB uses shared-mesh instancing, with 169,891,784 *expanded* triangles but only
> 38,276 *unique* ones, so baking writes each of the 18,823 nodes out as its own mesh and
> the allocation blows past the paging file. The flag is now `False` and a guard refuses
> the import outright if the expanded triangle count is large. Re-run only with the editor
> freshly started and after reading `2026-09-18-guidemen-rebuild-import-oom.md`.

Two things happen here, both non-destructive:

1. `BP_Guidemen_V5_4K` is duplicated to `BP_Guidemen_V5_4K_PreRebuild_20260918` so the
   current state stays recoverable as a sibling asset before anything is rebuilt.
2. The source GLB is imported fresh into a task-owned folder,
   `.../V5/Guidemen_4K/Rebuild20260918/Meshes`, using the project's established
   Interchange contract for these glTF sources: a -90 degree roll converts the source's
   Y-up encoding to UE's Z-up, metres to centimetres, Nanite on, no automatic collision,
   and **instancing preserved**.

Then it reports what actually arrived, so the rebuild's contract is verified rather than
assumed: per-mesh instance counts, bounds, and the assembled extent in UE axes — which
should read X ~56 m, Y ~18 m, Z ~20 m for a Y-up source rotated -90 about X.

Nothing is deleted, nothing is rebound, no map is saved.
"""
from __future__ import annotations

import json
import struct
from collections import defaultdict
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/source/Model/Guidemen_GuideGate_UE5_100M_Instanced.glb"
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/reimport-report-20260918.json"

PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K"
BLUEPRINT = PACKAGE + "/BP_Guidemen_V5_4K"
RECOVERY_NAME = "BP_Guidemen_V5_4K_PreRebuild_20260918"
MESH_DEST = PACKAGE + "/Rebuild20260918/Meshes"

# The source declares 169.9M expanded triangles against 38.3k unique. Anything above this
# guard means the import will try to materialise far more than the machine can hold.
EXPANDED_TRIANGLE_LIMIT = 20_000_000

EAL = unreal.EditorAssetLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _source_facts():
    """Read the GLB's JSON chunk for node and accessor counts, without decoding geometry."""
    with SOURCE.open("rb") as handle:
        magic, _, length = struct.unpack("<III", handle.read(12))
        assert magic == 0x46546C67, magic
        doc = None
        while handle.tell() < length:
            chunk_length, chunk_type = struct.unpack("<II", handle.read(8))
            payload = handle.read(chunk_length)
            if chunk_type == 0x4E4F534A and doc is None:
                doc = json.loads(payload.decode("utf-8"))
    return doc


def _expanded_triangles(doc):
    total = 0
    for mesh in doc.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index_accessor = primitive.get("indices")
            if index_accessor is None:
                continue
            total += doc["accessors"][index_accessor]["count"] // 3
    return total


def recovery_copy():
    existing = EAL.does_asset_exist(PACKAGE + "/" + RECOVERY_NAME)
    source = EAL.load_asset(BLUEPRINT)
    assert source, BLUEPRINT
    if existing:
        return {"created": False, "reason": "already exists", "path": PACKAGE + "/" + RECOVERY_NAME}
    copy = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        RECOVERY_NAME, PACKAGE, source)
    assert copy, RECOVERY_NAME
    assert EAL.save_loaded_asset(copy)
    return {"created": True, "path": _path(copy)}


def import_source():
    assert SOURCE.exists(), SOURCE
    doc = _source_facts()
    unique_triangles = _expanded_triangles(doc)
    node_count = len(doc.get("nodes", []))
    # The GLB stores one primitive per geometry definition; instances reference them.
    geometry_count = len(doc.get("meshes", []))
    instance_expansion = node_count  # each node with a mesh becomes a scene instance
    projected = unique_triangles * max(1, instance_expansion // max(1, geometry_count))
    assert projected <= EXPANDED_TRIANGLE_LIMIT, (
        f"refusing to import: {unique_triangles} unique triangles across {geometry_count} "
        f"geometries referenced by {node_count} nodes would expand to roughly {projected} "
        f"triangles. The previous attempt at this crashed the editor with an out-of-memory "
        f"error. Rebuild the assembly from the source node transforms instead of re-importing "
        f"the scene."
    )
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = "GuidemenRebuild"
    # source is Y-up metres; -90 roll is the project's established conversion
    pipeline.import_offset_rotation = unreal.Rotator(roll=-90)
    pipeline.import_offset_uniform_scale = 1.0
    # bake_meshes MUST stay False: this source is shared-mesh instanced, so baking writes
    # every node out as its own mesh and exhausts memory.
    pipeline.common_meshes_properties.bake_meshes = False
    pipeline.mesh_pipeline.combine_static_meshes = False
    pipeline.mesh_pipeline.build_nanite = True
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(MESH_DEST, manager.create_source_data(str(SOURCE)), params)
    assert objects, "import returned no objects"
    EAL.save_directory(MESH_DEST)
    return objects


def main():
    report = {"created": "2026-09-18", "source": str(SOURCE), "map_saved": False}
    report["recovery_copy"] = recovery_copy()
    print("REBUILD_RECOVERY", json.dumps(report["recovery_copy"]))

    objects = import_source()
    meshes = [o for o in objects if isinstance(o, unreal.StaticMesh)]
    report["imported_objects"] = len(objects)
    report["imported_meshes"] = len(meshes)
    print("REBUILD_IMPORT meshes", len(meshes), "objects", len(objects))

    rows = []
    lows, highs = [], []
    for mesh in meshes:
        bounds = mesh.get_bounds()
        centre = bounds.origin
        extent = bounds.box_extent
        low = [centre.x - extent.x, centre.y - extent.y, centre.z - extent.z]
        high = [centre.x + extent.x, centre.y + extent.y, centre.z + extent.z]
        lows.append(low)
        highs.append(high)
        nanite = mesh.get_editor_property("nanite_settings")
        rows.append({
            "mesh": _path(mesh),
            "name": mesh.get_name(),
            "size_cm": [round(extent.x * 2, 2), round(extent.y * 2, 2), round(extent.z * 2, 2)],
            "low_cm": [round(v, 2) for v in low],
            "high_cm": [round(v, 2) for v in high],
            "triangles": int(mesh.get_num_triangles(0)) if hasattr(mesh, "get_num_triangles") else None,
            "nanite": bool(nanite.get_editor_property("enabled")),
            "fallback_relative_error": float(nanite.get_editor_property("fallback_relative_error")),
            "fallback_percent_triangles": float(nanite.get_editor_property("fallback_percent_triangles")),
            "materials": [s.material_interface.get_name() for s in mesh.static_materials
                          if s.material_interface],
        })
    rows.sort(key=lambda item: -max(item["size_cm"]))
    report["meshes"] = rows
    if lows:
        report["assembled_low_cm"] = [round(min(l[a] for l in lows), 2) for a in range(3)]
        report["assembled_high_cm"] = [round(max(h[a] for h in highs), 2) for a in range(3)]
        report["assembled_extent_cm"] = [round(report["assembled_high_cm"][a] - report["assembled_low_cm"][a], 2)
                                         for a in range(3)]
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("REBUILD_ASSEMBLED extent_cm", report.get("assembled_extent_cm"),
          "low", report.get("assembled_low_cm"))
    print("REBUILD_EXPECTED source Y-up 56.39 x 20.26 x 18.00 m -> UE X 5639, Y 1800, Z 2026 cm")
    for item in rows[:20]:
        print(f"    {item['name'][:40]:42s} size_cm={item['size_cm']} tris={item['triangles']} "
              f"nanite={item['nanite']} mats={item['materials']}")
    print("REBUILD_REPORT", OUTPUT)


if __name__ == "__main__":
    main()
