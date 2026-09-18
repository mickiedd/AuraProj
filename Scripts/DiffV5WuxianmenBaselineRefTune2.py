"""Diagnose why the baseline comparison fails for untouched components."""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
BASELINE = ROOT / "Wuxianmen_V5_4K_Core-reftune2-baseline-20260917.json"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-reftune2-baseline-diff-20260917.json"


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


def main():
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    components = {component.get_name(): component for component in _components(blueprint)}
    report = {}
    for name, component in sorted(components.items()):
        expected = by_name[name]["instances"]
        issues = []
        for index, item in enumerate(expected):
            transform = component.get_instance_transform(index, False)
            live_loc = [transform.translation.x, transform.translation.y, transform.translation.z]
            live_scale = [transform.scale3d.x, transform.scale3d.y, transform.scale3d.z]
            loc_delta = [round(a - b, 6) for a, b in zip(live_loc, item["location"])]
            scale_delta = [round(a - b, 6) for a, b in zip(live_scale, item["scale"])]
            rotator = transform.rotation.rotator()
            if any(abs(value) > 1e-3 for value in loc_delta) or any(abs(value) > 1e-3 for value in scale_delta):
                issues.append({
                    "index": index,
                    "kind": "location_or_scale",
                    "live_location": [round(v, 4) for v in live_loc],
                    "base_location": item["location"],
                    "loc_delta": loc_delta,
                    "live_scale": [round(v, 4) for v in live_scale],
                    "base_scale": item["scale"],
                    "scale_delta": scale_delta,
                })
                if len(issues) >= 3:
                    break
                continue
            live_rot = [rotator.pitch, rotator.yaw, rotator.roll]
            base_rot = item["rotation"]
            if any(abs(a - b) > 1e-3 for a, b in zip(live_rot, base_rot)):
                issues.append({
                    "index": index,
                    "kind": "rotation",
                    "live_rotator": [round(v, 4) for v in live_rot],
                    "base_rotator": base_rot,
                    "live_quat": [round(v, 6) for v in (transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w)],
                })
                if len(issues) >= 3:
                    break
        report[name] = {
            "instance_count": component.get_instance_count(),
            "expected_count": len(expected),
            "issues": issues,
        }
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE2_BASELINE_DIFF", OUTPUT)
    for name, item in report.items():
        print("REFTUNE2_DIFF", name, item["instance_count"], item["expected_count"], json.dumps(item["issues"]))


if __name__ == "__main__":
    main()
