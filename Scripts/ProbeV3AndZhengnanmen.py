"""Probe the V3 Blueprints and the Zhengnanmen HighFidelity component set.

The generic HISM audit reported zero components for the three V3 Blueprints, so
they must carry their geometry in another component or actor type. This lists every
component of every type for those Blueprints, plus the actor class, so the right
audit path can be chosen. For Zhengnanmen HighFidelity it lists all 114 component
mesh names so the roof and ridge roles can be identified by name.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/"
V3 = [
    "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
    "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3",
    "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
]
ZHENGNAN = "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"


def _path(value):
    return value.get_path_name() if value else ""


def main():
    for relative in V3:
        asset = unreal.EditorAssetLibrary.load_asset(ROOT + relative)
        print("=== V3", relative.split("/")[-1], "class", asset.get_class().get_name() if asset else "MISSING")
        if not asset:
            continue
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        counts = {}
        samples = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(asset):
            data = subsystem.k2_find_subobject_data_from_handle(handle)
            obj = library.get_object(data)
            if obj is None:
                continue
            cls = obj.get_class().get_name()
            counts[cls] = counts.get(cls, 0) + 1
            if len(samples) < 12:
                mesh = ""
                try:
                    mesh = _path(obj.static_mesh)
                except Exception:
                    mesh = ""
                samples.append((obj.get_name(), cls, mesh))
        print("   component classes", json.dumps(counts))
        for name, cls, mesh in samples:
            print("     ", name[:52], "|", cls, "|", mesh.split("/")[-1])
        # actor-level info
        try:
            generated = asset.generated_class()
            default = unreal.get_default_object(generated)
            print("   default object", default.get_class().get_name(),
                  "components", len(default.get_components_by_class(unreal.ActorComponent)))
        except Exception as error:
            print("   default object probe failed", repr(error)[:120])

    asset = unreal.EditorAssetLibrary.load_asset(ROOT + ZHENGNAN)
    print("=== ZHENGNAN components", asset.get_class().get_name() if asset else "MISSING")
    if asset:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        rows = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(asset):
            data = subsystem.k2_find_subobject_data_from_handle(handle)
            obj = library.get_object(data)
            if not isinstance(obj, unreal.HierarchicalInstancedStaticMeshComponent):
                continue
            rows.append((obj.get_name(), _path(obj.static_mesh).split("/")[-1].split(".")[0],
                         obj.get_instance_count()))
        for name, mesh, count in sorted(rows, key=lambda r: r[1]):
            print(f"   {mesh[:58]:60s} n={count:5d}  {name[:44]}")


if __name__ == "__main__":
    main()
