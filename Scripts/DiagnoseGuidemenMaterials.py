"""Diagnose the Guidemen materials: why does most of the building render blue?

The capture shows the stone wall rendering correctly as light grey textured masonry, while
the timber, doors, roof and other parts render as saturated blue with visible relief. Blue
with relief is the signature of a normal map being read as base colour — a tangent-space
normal map is predominantly (128, 128, 255).

This dumps, for every material bound on the Blueprint's components: the material path, its
blend/shading flags, the textures it samples, whether each texture loads, and whether the
base-colour sampler looks like a normal map by name. Also reports which components use which
material, so the blast radius is clear.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/material-diagnosis-20260918.json"
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

    usage = {}
    for component in _components(blueprint):
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            if not material:
                continue
            usage.setdefault(_path(material), []).append(component.get_name())

    report = {"blueprint": BLUEPRINT, "materials": [], "map_saved": False}
    for path, components in sorted(usage.items()):
        material = unreal.EditorAssetLibrary.load_asset(path)
        entry = {"material": path, "components": len(components),
                 "component_sample": components[:3]}
        if not material:
            entry["error"] = "material did not load"
            report["materials"].append(entry)
            print("MAT_DIAG", path.split("/")[-1], "DID NOT LOAD")
            continue
        try:
            textures = unreal.MaterialEditingLibrary.get_used_textures(material)
        except Exception as error:
            textures = []
            entry["texture_query_error"] = repr(error)[:100]
        rows = []
        for texture in textures:
            texture_path = _path(texture)
            rows.append({
                "texture": texture_path.split("/")[-1].split(".")[0],
                "loaded": texture is not None,
                "looks_like_normal": "normal" in texture_path.lower(),
            })
        entry["textures"] = rows
        entry["two_sided"] = bool(material.get_editor_property("two_sided"))
        entry["used_with_nanite"] = bool(material.get_editor_property("used_with_nanite"))
        entry["shading_model"] = str(material.get_editor_property("shading_model"))
        report["materials"].append(entry)
        flags = []
        if any(row["looks_like_normal"] for row in rows):
            flags.append("has normal map")
        if not rows:
            flags.append("NO TEXTURES")
        print(f"MAT_DIAG {path.split('/')[-1][:44]:46s} comps={len(components):2d} "
              f"tex={[row['texture'][:22] for row in rows]} {' | '.join(flags)}")
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("MAT_DIAG_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
