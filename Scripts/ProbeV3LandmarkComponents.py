"""Inspect the components of the V3 landmark Blueprints.

The V3 Blueprints carry components with non-identity relative transforms, named
ReferenceInfill_*, built from /Engine/BasicShapes/Cube. If those are guide
geometry rather than art they must be excluded from grounding and spacing
measurements, otherwise they would define the landmark's footprint and floor.

This lists every component of the two V3 gate Blueprints with its mesh, relative
transform and visibility flags so the decision is made on evidence.

Read-only.
"""

import json

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"
BLUEPRINTS = [
    ROOT + "/V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
    ROOT + "/V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
    ROOT + "/V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3",
]


def relative_transform(component):
    for attribute in ("relative_transform",):
        try:
            return component.get_editor_property(attribute)
        except Exception:
            pass
    for method in ("get_relative_transform", "k2_get_relative_transform"):
        if hasattr(component, method):
            try:
                return getattr(component, method)()
            except Exception:
                pass
    return None


def read(component, attribute):
    try:
        value = component.get_editor_property(attribute)
    except Exception:
        return None
    if isinstance(value, bool):
        return value
    try:
        return str(value)
    except Exception:
        return repr(value)


def main():
    report = []
    for path in BLUEPRINTS:
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if blueprint is None:
            report.append({"path": path, "error": "load failed"})
            continue
        actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
            blueprint.generated_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=True)
        assert actor, path
        rows = []
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = component.static_mesh
            transform = relative_transform(component)
            translation = transform.translation if transform else None
            rows.append({
                "name": component.get_name(),
                "class": component.get_class().get_name(),
                "mesh": mesh.get_path_name() if mesh else None,
                "rel_loc": [round(float(translation.x), 2), round(float(translation.y), 2),
                            round(float(translation.z), 2)] if translation else None,
                "visible": read(component, "visible"),
                "hidden_in_game": read(component, "hidden_in_game"),
                "is_editor_only": read(component, "is_editor_only"),
                "mobility": read(component, "mobility"),
            })
        entry = {"path": path, "component_count": len(rows), "components": rows}
        fill = [row for row in rows if "ReferenceInfill" in row["name"]]
        entry["reference_infill_count"] = len(fill)
        entry["reference_infill_sample"] = fill[:4]
        entry["non_identity_count"] = sum(
            1 for row in rows
            if row["rel_loc"] and any(abs(value) > 0.001 for value in row["rel_loc"]))
        report.append(entry)
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)

    unreal.log("V3_COMPONENT_PROBE " + json.dumps(report))
    print("V3_COMPONENT_PROBE", json.dumps(report, indent=2)[:12000])


if __name__ == "__main__":
    main()
