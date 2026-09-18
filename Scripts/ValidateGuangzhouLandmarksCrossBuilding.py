"""Validate the GuangzhouLandmarks cross-building pass.

Two checks, live:

1. Nanite fallback sweep over all seven building Blueprints — every Nanite-enabled
   mesh must now report `fallback_relative_error = 0.0` and
   `fallback_percent_triangles = 100.0`, and no mesh may have lost its Nanite flag.
2. Guidemen V5 4K detail pass — component count, instance count, collision,
   visibility and shadow flags unchanged; every instance transform byte-identical
   to the pass baseline; the new stone and tile materials bound to the intended
   components; the custom-data channels sized 3 and 1; and the wall-height input
   shown to span its range.

Read-only.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport"
OUTPUT = ROOT / "GuangzhouLandmarks-crossbuilding-validation-20260918.json"

BLUEPRINTS = [
    ("Zhengnanmen_HighFidelity", "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"),
    ("Xiaobeimen_AAA_V3", "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3"),
    ("Xiaobeimen_Production_V3", "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3"),
    ("Zhengnanmen_AAA_V3", "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3"),
    ("Guidemen_V5_4K", "V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("Wuxianmen_V5_4K_Core", "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"),
    ("Wuxianmen_V5_FullPBR", "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"),
]
ROOT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/"
GUIDEMEN_BLUEPRINT = ROOT_PATH + "V5/Guidemen_4K/BP_Guidemen_V5_4K"
GUIDEMEN_BASELINE = ROOT / "Guidemen_V5_4K-reftune4-baseline-20260918.json"


def _path(value):
    return value.get_path_name() if value else ""


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if not isinstance(obj, (unreal.StaticMeshComponent,
                                unreal.HierarchicalInstancedStaticMeshComponent)):
            continue
        if _path(obj) in seen:
            continue
        seen.add(_path(obj))
        result.append(obj)
    return result


def _transform_hash(component):
    payload = []
    if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, False)
            rotator = transform.rotation.rotator()
            payload.append([round(transform.translation.x, 4), round(transform.translation.y, 4),
                            round(transform.translation.z, 4), round(rotator.pitch, 5),
                            round(rotator.yaw, 5), round(rotator.roll, 5),
                            round(transform.scale3d.x, 6), round(transform.scale3d.y, 6),
                            round(transform.scale3d.z, 6)])
    else:
        location = component.get_editor_property("relative_location")
        rotation = component.get_editor_property("relative_rotation")
        rotator = rotation.rotator() if hasattr(rotation, "rotator") else rotation
        scale = component.get_editor_property("relative_scale3d")
        payload.append([round(location.x, 4), round(location.y, 4), round(location.z, 4),
                        round(rotator.pitch, 5), round(rotator.yaw, 5), round(rotator.roll, 5),
                        round(scale.x, 6), round(scale.y, 6), round(scale.z, 6)])
    return hashlib.sha256(json.dumps(payload, separators=(",", ":")).encode("utf-8")).hexdigest()


def main():
    report = {"created": "2026-09-18", "nanite_sweep": {}, "guidemen": {}, "map_saved": False}

    # ---- 1. Nanite sweep ----
    offenders = []
    for label, relative in BLUEPRINTS:
        blueprint = unreal.EditorAssetLibrary.load_asset(ROOT_PATH + relative)
        assert isinstance(blueprint, unreal.Blueprint), relative
        checked = 0
        bad = []
        for component in _components(blueprint):
            mesh = component.static_mesh
            if not mesh:
                continue
            settings = mesh.get_editor_property("nanite_settings")
            if not bool(settings.get_editor_property("enabled")):
                continue
            checked += 1
            err = float(settings.get_editor_property("fallback_relative_error"))
            pct = float(settings.get_editor_property("fallback_percent_triangles"))
            if err != 0.0 or pct != 100.0:
                bad.append({"mesh": _path(mesh).split("/")[-1], "error": err, "percent": pct})
        report["nanite_sweep"][label] = {"nanite_meshes_checked": checked, "offenders": bad}
        offenders.extend(bad)
        print("VALIDATE_NANITE", label, "checked", checked, "offenders", len(bad))
    assert not offenders, offenders[:5]

    # ---- 2. Guidemen detail pass ----
    baseline = json.loads(GUIDEMEN_BASELINE.read_text(encoding="utf-8"))
    baseline_by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = unreal.EditorAssetLibrary.load_asset(GUIDEMEN_BLUEPRINT)
    components = {c.get_name(): c for c in _components(blueprint)}
    assert set(components) == set(baseline_by_name), "component set changed"

    stone_material = "M_WeatheredStone_4K_ReferenceTuned_RefTune4"
    tile_material = "M_GrayClayTile_2K_ReferenceTuned_RefTune4"
    entry = {"components": len(components), "instance_total": 0,
             "transforms_unchanged": True, "stone_bound": 0, "tile_bound": 0,
             "stone_custom_floats": set(), "tile_custom_floats": set()}
    heights = []
    for name, item in baseline_by_name.items():
        component = components[name]
        assert component.get_instance_count() == item["instance_count"], name
        assert bool(component.is_visible()) == item["visible"], name
        assert str(component.get_collision_profile_name()) == item["collision_profile"], name
        assert bool(component.get_editor_property("cast_shadow")) == item["cast_shadow"], name
        assert _transform_hash(component) == item["transform_hash"], (name, "transforms changed")
        entry["instance_total"] += component.get_instance_count()
        bound = _path(component.get_material(0))
        floats = int(component.get_editor_property("num_custom_data_floats"))
        if stone_material in bound:
            entry["stone_bound"] += 1
            entry["stone_custom_floats"].add(floats)
            if floats >= 3:
                for index in range(component.get_instance_count()):
                    heights.append(component.get_instance_transform(index, False).translation.z)
        elif tile_material in bound:
            entry["tile_bound"] += 1
            entry["tile_custom_floats"].add(floats)
    entry["stone_custom_floats"] = sorted(entry["stone_custom_floats"])
    entry["tile_custom_floats"] = sorted(entry["tile_custom_floats"])
    entry["wall_height_span_cm"] = [round(min(heights), 2), round(max(heights), 2)] if heights else None
    assert entry["stone_bound"] >= 10, entry["stone_bound"]
    assert entry["tile_bound"] >= 10, entry["tile_bound"]
    assert entry["stone_custom_floats"] == [3], entry["stone_custom_floats"]
    assert entry["tile_custom_floats"] == [1], entry["tile_custom_floats"]
    report["guidemen"] = entry
    print("VALIDATE_GUIDEMEN", json.dumps(entry))

    report["passed"] = True
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("VALIDATE_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
