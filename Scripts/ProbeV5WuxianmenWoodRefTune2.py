"""Reload the Wuxianmen Blueprint from disk and list its large wood instances.

A partially completed repair run mutated the tile component in memory without
saving. This reloads the asset from disk so the in-memory state matches the
baseline again, then reports every wood instance whose X span is large enough to
be a roof deck, so the deck selector can be written against real data.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-reftune2-wood-probe-20260917.json"
EAL = unreal.EditorAssetLibrary


def _path(value):
    return value.get_path_name() if value else ""


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set()
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if _path(component) in seen:
            continue
        seen.add(_path(component))
        result.append(component)
    return result


def _aabb(transform, local_min, local_max):
    quat = transform.rotation
    scale = transform.scale3d
    corners = []
    for x in (local_min.x, local_max.x):
        for y in (local_min.y, local_max.y):
            for z in (local_min.z, local_max.z):
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    quat, unreal.Vector(x * scale.x, y * scale.y, z * scale.z)
                )
                corners.append(rotated + transform.translation)
    low = [min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)]
    high = [max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)]
    return low, high


def main():
    candidates = [name for name in dir(unreal.EditorLoadingAndSavingUtils) if "reload" in name.lower()]
    print("REFTUNE2_RELOAD_API", json.dumps(candidates))
    reloaded = None
    if candidates:
        method = getattr(unreal.EditorLoadingAndSavingUtils, candidates[0])
        try:
            package = unreal.load_package(BLUEPRINT.split(".")[0])
            reloaded = method([package])
        except Exception as error:
            print("REFTUNE2_RELOAD_ERROR", repr(error))
    print("REFTUNE2_RELOAD", reloaded)
    blueprint = EAL.load_asset(BLUEPRINT)
    components = {component.get_name(): component for component in _components(blueprint)}
    wood = components["HISM_006_wood_000451_GEN_VARIABLE"]
    bounds = wood.static_mesh.get_bounds()
    local_min = bounds.origin - bounds.box_extent
    local_max = bounds.origin + bounds.box_extent
    print("REFTUNE2_WOOD_MESH", _path(wood.static_mesh), [round(v, 3) for v in (local_min.x, local_min.y, local_min.z)], [round(v, 3) for v in (local_max.x, local_max.y, local_max.z)])

    rows = []
    for index in range(wood.get_instance_count()):
        transform = wood.get_instance_transform(index, False)
        low, high = _aabb(transform, local_min, local_max)
        rows.append({
            "index": index,
            "location": [round(transform.translation.x, 2), round(transform.translation.y, 2), round(transform.translation.z, 2)],
            "scale": [round(transform.scale3d.x, 4), round(transform.scale3d.y, 4), round(transform.scale3d.z, 4)],
            "span": [round(high[0] - low[0], 2), round(high[1] - low[1], 2), round(high[2] - low[2], 2)],
        })
    big = [row for row in rows if row["span"][0] > 1500.0]
    OUTPUT.write_text(json.dumps({"instances": rows, "large": big}, indent=2), encoding="utf-8")
    print("REFTUNE2_WOOD_LARGE", len(big))
    for row in big:
        print("REFTUNE2_WOOD_ROW", row["index"], row["location"], row["scale"], row["span"])


if __name__ == "__main__":
    main()
