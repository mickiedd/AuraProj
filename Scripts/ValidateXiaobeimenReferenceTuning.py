"""Focused fresh-world validator for the Xiaobeimen reference tuning."""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "Saved/RawModelImport/V3"
REPORT = json.loads((RAW / "Xiaobeimen_AAA_V3-reference-tuning.json").read_text(encoding="utf-8"))
IMPORT = json.loads((RAW / "Xiaobeimen_AAA_V3-import.json").read_text(encoding="utf-8"))
BP_PATH = REPORT["blueprint"]
DESTINATION = BP_PATH.rsplit("/BP_", 1)[0]
PREFIX = "ReferenceInfill_"
PANEL_NAMES = set(REPORT["panel_names"])


def path(obj):
    return obj.get_path_name() if obj else ""


def vector(v):
    return [v.x, v.y, v.z]


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    assert bp and bp.generated_class(), BP_PATH
    expected = REPORT["panel_dimensions_cm"]
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor = actor_system.spawn_actor_from_class(bp.generated_class(), unreal.Vector(12345, -6789, 321))
    assert actor, BP_PATH
    components = actor.get_components_by_class(unreal.StaticMeshComponent)
    visible = [c for c in components if c.get_editor_property("visible")]
    hidden = [c for c in components if not c.get_editor_property("visible")]
    source_count = next(e["source"]["instance_count"] for e in IMPORT["imports"] if e["source"]["primary"])
    assert len(visible) == source_count + len(PANEL_NAMES), (len(visible), source_count)
    assert len(hidden) == REPORT["collision_components"] == 5, len(hidden)

    panels = [c for c in visible if c.get_name().startswith(PREFIX)]
    assert {c.get_name() for c in panels} == PANEL_NAMES
    for component in panels:
        name = component.get_name()
        assert path(component.static_mesh) == "/Engine/BasicShapes/Cube.Cube", name
        assert path(component.get_material(0)).startswith(REPORT["material"]), (name, path(component.get_material(0)))
        assert str(component.get_collision_profile_name()) == "NoCollision"
        assert component.get_editor_property("hidden_in_game") is False
        location = vector(component.get_editor_property("relative_location"))
        scale = vector(component.get_editor_property("relative_scale3d"))
        dimensions = expected[name]
        assert max(abs(a - b) for a, b in zip(location, {
            "ReferenceInfill_L0_F": [0, 300, 1190], "ReferenceInfill_L0_B": [0, -300, 1190],
            "ReferenceInfill_L0_L": [-700, 0, 1190], "ReferenceInfill_L0_R": [700, 0, 1190],
            "ReferenceInfill_L1_F": [0, 300, 1545], "ReferenceInfill_L1_B": [0, -300, 1545],
            "ReferenceInfill_L1_L": [-700, 0, 1545], "ReferenceInfill_L1_R": [700, 0, 1545],
        }[name])) < 0.01
        assert max(abs(a * 100 - b) for a, b in zip(scale, dimensions)) < 0.01

    for component in hidden:
        assert component.get_editor_property("hidden_in_game")
        assert str(component.get_collision_profile_name()) == "BlockAll"

    # The package dependency closure may use the engine cube primitive, but
    # every runtime material remains inside this Xiaobeimen package.
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
        include_hard_package_references=True, include_searchable_names=False,
        include_soft_management_references=False, include_hard_management_references=False)
    pending, deps = [BP_PATH], set()
    while pending:
        item = pending.pop()
        for dependency in registry.get_dependencies(item, options):
            dep = str(dependency)
            if dep in deps:
                continue
            deps.add(dep)
            if dep.startswith("/Game/"):
                assert dep.startswith(DESTINATION + "/"), dep
                assert unreal.EditorAssetLibrary.does_asset_exist(dep), dep
                pending.append(dep)

    preview_level = REPORT["preview_level"]
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level(preview_level), preview_level
    preview = next(a for a in actor_system.get_all_level_actors()
                   if a.get_actor_label() == "Preview_Xiaobeimen_AAA_V3")
    preview_components = preview.get_components_by_class(unreal.StaticMeshComponent)
    assert sum(1 for c in preview_components if c.get_editor_property("visible")) == len(visible)
    assert sum(1 for c in preview_components if not c.get_editor_property("visible")) == len(hidden)
    preview_visible_count = sum(1 for c in preview_components if c.get_editor_property("visible"))
    assert levels.load_level(REPORT["showcase_level"]), REPORT["showcase_level"]
    showcase = next(a for a in actor_system.get_all_level_actors()
                    if a.get_actor_label() == "GuangzhouLandmark_Xiaobeimen_AAA_V3")
    showcase_components = showcase.get_components_by_class(unreal.StaticMeshComponent)
    assert sum(1 for c in showcase_components if c.get_editor_property("visible")) == len(visible)
    assert sum(1 for c in showcase_components if not c.get_editor_property("visible")) == len(hidden)
    showcase_visible_count = sum(1 for c in showcase_components if c.get_editor_property("visible"))
    print("XIAOBEIMEN_REFERENCE_TUNING_VALIDATED", json.dumps({
        "passed": True, "visible_components": len(visible), "collision_components": len(hidden),
        "panel_count": len(panels), "dependency_count": len(deps),
        "preview_visible": preview_visible_count,
        "showcase_visible": showcase_visible_count,
    }))


if __name__ == "__main__":
    main()
