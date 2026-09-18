"""Dump the Guidemen Blueprint's component and instance payload for comparison with the source.

The rebuild question is whether the asset's assembly is a faithful copy of the source GLB's
node transforms. That is answerable without changing anything: dump every component's mesh,
instance count and per-instance transform, then compare offline against the transforms read
straight from the GLB.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/assembly-dump-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"


def _path(value):
    return value.get_path_name() if value else ""


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
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


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    report = {"blueprint": BLUEPRINT, "components": [], "map_saved": False}
    total = 0
    for component in _components(blueprint):
        rows = []
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, False)
            rotator = transform.rotation.rotator()
            rows.append({
                "l": [round(transform.translation.x, 3), round(transform.translation.y, 3),
                      round(transform.translation.z, 3)],
                "r": [round(rotator.pitch, 3), round(rotator.yaw, 3), round(rotator.roll, 3)],
                "s": [round(transform.scale3d.x, 5), round(transform.scale3d.y, 5),
                      round(transform.scale3d.z, 5)],
            })
        total += len(rows)
        report["components"].append({
            "name": component.get_name(),
            "mesh": _path(component.static_mesh),
            "mesh_name": _path(component.static_mesh).split("/")[-1].split(".")[0],
            "instances": rows,
        })
    report["instance_total"] = total
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("ASSEMBLY_DUMP components", len(report["components"]), "instances", total)
    for item in report["components"]:
        print(f"    {item['mesh_name'][:36]:38s} n={len(item['instances']):6d}  {item['name'][:40]}")
    print("ASSEMBLY_DUMP_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
