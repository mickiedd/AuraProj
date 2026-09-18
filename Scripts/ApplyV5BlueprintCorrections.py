"""Apply the reference-verified root orientation to existing V5 Blueprints."""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V5"


def main():
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    results = []
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for cfg in json.loads((ROOT / "packages.json").read_text(encoding="utf-8")):
        report = json.loads((ROOT / (cfg["name"] + "-import.json")).read_text(encoding="utf-8"))
        blueprint = unreal.EditorAssetLibrary.load_asset(report["blueprint"])
        assert isinstance(blueprint, unreal.Blueprint)
        root_component = None
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
            data = subsystem.k2_find_subobject_data_from_handle(handle)
            if library.is_root_component(data):
                root_component = library.get_object(data)
                break
        assert root_component, report["blueprint"]
        before = root_component.get_editor_property("relative_rotation")
        desired = cfg.get("blueprint_root_rotation", [0.0, 0.0, 0.0])
        root_component.set_editor_property(
            "relative_rotation",
            unreal.Rotator(pitch=desired[0], yaw=desired[1], roll=desired[2]),
        )
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
        results.append({
            "name": cfg["name"],
            "blueprint": report["blueprint"],
            "before": [before.pitch, before.yaw, before.roll],
            "after": desired,
            "reason": "Align the authored building height with Unreal Z; Guidemen alone requires the archive-scene X-axis correction.",
        })
    (ROOT / "blueprint-corrections.json").write_text(json.dumps({"passed": True, "packages": results}, indent=2), encoding="utf-8")
    print("V5_BLUEPRINT_CORRECTIONS_APPLIED", json.dumps(results))


if __name__ == "__main__":
    main()
