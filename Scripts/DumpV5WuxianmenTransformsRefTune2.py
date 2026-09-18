"""Dump every live instance transform of BP_Wuxianmen_V5_4K_Core for offline verification."""
import json
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-reftune2-live-transforms-20260917.json"


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
    report = {"blueprint": BLUEPRINT, "components": {}}
    for component in _components(blueprint):
        rows = []
        for index in range(component.get_instance_count()):
            t = component.get_instance_transform(index, False)
            rows.append({
                "i": index,
                "l": [t.translation.x, t.translation.y, t.translation.z],
                "q": [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w],
                "s": [t.scale3d.x, t.scale3d.y, t.scale3d.z],
            })
        report["components"][component.get_name()] = {
            "mesh": _path(component.static_mesh),
            "instances": rows,
        }
    OUTPUT.write_text(json.dumps(report), encoding="utf-8")
    print("REFTUNE2_LIVE_DUMP", OUTPUT)
    for name, item in report["components"].items():
        print("REFTUNE2_LIVE", name, len(item["instances"]))


if __name__ == "__main__":
    main()
