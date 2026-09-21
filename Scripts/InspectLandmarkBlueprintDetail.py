"""Describe the landmark Blueprints in detail: parent class, component tree,
per-component mesh + instance count + mobility, and the class the wrapper is.

The first inventory pass enumerated components off the CDO, which returns
nothing for these Blueprints. This pass uses SubobjectDataSubsystem, which
walks the Blueprint's own SimpleConstructionScript.

Read-only.
"""

import json

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"

BLUEPRINTS = [
    "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
    "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
    "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3",
    "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
    "V5/Guidemen_4K/BP_Guidemen_V5_4K",
    "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
    "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
]


def subobject_rows(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if obj is None:
            rows.append({"class": "?", "name": str(library.get_display_name(data))})
            continue
        row = {
            "class": obj.get_class().get_name(),
            "name": str(library.get_display_name(data)),
            "var": str(library.get_variable_name(data)),
        }
        for attr in ("static_mesh", "mobility"):
            try:
                value = obj.get_editor_property(attr)
            except Exception:
                continue
            if attr == "static_mesh":
                row["mesh"] = value.get_path_name() if value else None
            else:
                row["mobility"] = str(value).split(".")[-1].rstrip(":>")
        for getter in ("get_instance_count", "get_num_instances"):
            if hasattr(obj, getter):
                try:
                    row["instances"] = int(getattr(obj, getter)())
                    break
                except Exception:
                    pass
        rows.append(row)
    return rows


def main():
    report = []
    for relative in BLUEPRINTS:
        path = ROOT + "/" + relative
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        entry = {"path": path}
        if blueprint is None:
            entry["error"] = "load failed"
            report.append(entry)
            continue
        generated = blueprint.generated_class()
        entry["asset_class"] = blueprint.get_class().get_name()
        entry["generated_class"] = generated.get_name() if generated else None
        for attr, key in (("parent_class", "super_class"), ("native_parent_class", "native_parent")):
            try:
                value = blueprint.get_editor_property(attr)
            except Exception:
                continue
            if value is not None:
                entry[key] = value.get_name()
        try:
            entry["is_level_instance"] = bool(
                unreal.EditorAssetLibrary.find_asset_data(path).asset_class_path.asset_name)
        except Exception:
            pass
        entry["subobjects"] = subobject_rows(blueprint)
        entry["subobject_count"] = len(entry["subobjects"])
        report.append(entry)

    unreal.log("LANDMARK_BLUEPRINT_DETAIL " + json.dumps(report))
    for entry in report:
        print("BP", entry.get("path", "?").rsplit("/", 1)[-1],
              "| class:", entry.get("asset_class"),
              "| super:", entry.get("super_class"),
              "| subobjects:", entry.get("subobject_count"),
              "|", entry.get("error", ""))
        for row in entry.get("subobjects", [])[:60]:
            print("     ", row.get("class"), "|", row.get("name"),
                  "| inst:", row.get("instances"),
                  "| mob:", row.get("mobility"),
                  "|", (row.get("mesh") or "").rsplit("/", 1)[-1])


if __name__ == "__main__":
    main()
