"""Diagnose the landmark wrapper Blueprint that just failed its compile assert.

Reports what compile_blueprint actually returns, the Blueprint's status, and the
component tree, so the assert can be replaced with a real check.
"""

import json

import unreal

PATHS = [
    "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate/BP_GreatNorthGate",
    "/Game/Assets/Environment/GuangzhouLandmarks/ZhenhaiTower/BP_ZhenhaiTower",
]


def main():
    out = []
    for path in PATHS:
        entry = {"path": path}
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        entry["exists"] = blueprint is not None
        if blueprint is None:
            out.append(entry)
            continue
        entry["class"] = blueprint.get_class().get_name()
        try:
            entry["parent_class"] = blueprint.get_editor_property("parent_class").get_name()
        except Exception as exc:
            entry["parent_class"] = "unreadable: {}".format(exc)

        result = unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        entry["compile_return"] = repr(result)
        entry["compile_return_type"] = type(result).__name__

        try:
            entry["generated_class"] = blueprint.generated_class().get_name()
        except Exception as exc:
            entry["generated_class"] = "unreadable: {}".format(exc)

        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        rows = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
            data = subsystem.k2_find_subobject_data_from_handle(handle)
            obj = library.get_object(data)
            rows.append({
                "name": str(library.get_display_name(data)),
                "class": obj.get_class().get_name() if obj else "?",
                "is_root": library.is_root_component(data),
            })
        entry["subobjects"] = rows
        entry["subobject_count"] = len(rows)

        # Spawn the blueprint into the current world to confirm it is placeable.
        try:
            actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
                blueprint.generated_class(), unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0),
                transient=True)
            entry["spawn_ok"] = actor is not None
            if actor:
                origin, extent = actor.get_actor_bounds(False)
                entry["spawned_bounds_size_cm"] = [
                    round(float(extent.x) * 2, 2), round(float(extent.y) * 2, 2),
                    round(float(extent.z) * 2, 2)]
                entry["spawned_components"] = len(
                    actor.get_components_by_class(unreal.ActorComponent))
                unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
        except Exception as exc:
            entry["spawn_ok"] = "failed: {}".format(exc)

        out.append(entry)

    unreal.log("WRAPPER_DIAGNOSTIC " + json.dumps(out))
    print("WRAPPER_DIAGNOSTIC", json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
