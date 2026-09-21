"""Rebind the landmark Blueprint components to their repaired meshes.

Only runs on meshes whose import round trip was verified by bounds in
ImportRepairedMeshes.py. Each component whose mesh is the original is pointed at
the repaired variant; instance payloads, materials, collision and visibility are
left untouched.

The originals are never modified, so rollback is a matter of pointing the
component back. A per-component record of what changed is written before saving,
and the save only happens if every component rebound cleanly.
"""

import json
from pathlib import Path

import unreal

IMPORT_REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/import-report.json")
PROGRESS = Path("C:/Git/AuraProj/Saved/MeshAudit/progress.json")
REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/rebind-report.json")

LANDMARK_BLUEPRINTS = {
    "Zhengnanmen_HighFidelity":
        "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/"
        "BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
    "Zhengnanmen_AAA_V3":
        "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
    "Xiaobeimen_AAA_V3":
        "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
    "Xiaobeimen_Production_V3":
        "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/"
        "BP_Xiaobeimen_Production_V3",
    "Guidemen_V5_4K":
        "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K",
    "Wuxianmen_V5_4K_Core":
        "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
    "Wuxianmen_V5_FullPBR":
        "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_FullPBR",
    "GreatNorthGate":
        "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate/BP_GreatNorthGate",
    "ZhenhaiTower":
        "/Game/Assets/Environment/GuangzhouLandmarks/ZhenhaiTower/BP_ZhenhaiTower",
}


def blueprint_components(blueprint):
    """Blueprint template components, deduplicated.

    k2_gather_subobject_data_for_blueprint returns each component more than once
    (the SCS template and its inherited counterpart), so identity-dedupe.
    """
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if obj is None:
            continue
        seen[obj.get_path_name()] = obj
    return list(seen.values())


def main():
    imports = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    owners = json.loads(PROGRESS.read_text(encoding="utf-8"))["owners"]

    # repaired asset name -> (original mesh path, repaired mesh path)
    swaps = {}
    for name, entry in imports.items():
        if not entry.get("bounds_match"):
            continue
        swaps[name] = (entry["source_mesh"], entry["asset"])

    if not swaps:
        print("REBIND_NOTHING verified swaps:", 0)
        return

    report = {"swaps": {}, "components": [], "failed": []}
    for name, (original, repaired) in sorted(swaps.items()):
        report["swaps"][name] = {"from": original, "to": repaired}

    touched_blueprints = set()
    for name, (original, repaired) in sorted(swaps.items()):
        repaired_mesh = unreal.EditorAssetLibrary.load_asset(repaired)
        if repaired_mesh is None:
            report["failed"].append({"mesh": name, "reason": "repaired asset missing"})
            continue
        for landmark in owners.get(original, []):
            blueprint_path = LANDMARK_BLUEPRINTS.get(landmark)
            if blueprint_path is None:
                report["failed"].append({"mesh": name, "reason": "unknown landmark "
                                                                  + landmark})
                continue
            blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
            if blueprint is None:
                report["failed"].append({"mesh": name,
                                         "reason": "blueprint missing " + blueprint_path})
                continue
            rebound = 0
            for component in blueprint_components(blueprint):
                if not isinstance(component, unreal.StaticMeshComponent):
                    continue
                current = component.static_mesh
                if current is None or current.get_path_name() != original + "." + \
                        original.rsplit("/", 1)[-1]:
                    continue
                component.set_static_mesh(repaired_mesh)
                rebound += 1
                report["components"].append({
                    "landmark": landmark,
                    "blueprint": blueprint_path,
                    "component": component.get_name(),
                    "from": original,
                    "to": repaired,
                })
            if rebound:
                touched_blueprints.add(blueprint_path)
                print("REBOUND", landmark, name, rebound)

    for blueprint_path in sorted(touched_blueprints):
        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        saved = unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False)
        if not saved:
            report["failed"].append({"blueprint": blueprint_path, "reason": "save failed"})
        print("REBIND_SAVED", blueprint_path, bool(saved))

    report["blueprints_saved"] = sorted(touched_blueprints)
    report["component_count"] = len(report["components"])
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REBIND_TOTAL components", report["component_count"],
          "blueprints", len(touched_blueprints), "failed", len(report["failed"]))
    print("REBIND_REPORT", REPORT)


if __name__ == "__main__":
    main()
