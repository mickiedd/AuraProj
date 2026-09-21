"""Wrap the two unwrapped Guangzhou landmarks into independent Blueprints.

GreatNorthGate (Dabeimen) and ZhenhaiTower exist in the landmark library only as
bare StaticMeshes with loose materials. Every other landmark in the folder is an
independent Blueprint Actor Asset, so these two cannot be placed the same way.

This follows the convention established by CreateGreatSouthGateActorAsset.py:
the wrapper is a PackedLevelActor Blueprint holding one
HierarchicalInstancedStaticMeshComponent per source mesh, with the source
transform recorded as an instance transform. These two landmarks are single
meshes sitting at identity, so each gets one HISM component with one instance.

NOTE: unreal.BlueprintEditorLibrary.compile_blueprint returns None in this
build (void, not bool) - asserting on its return value fails even on success.
The wrapper is verified by spawning it and comparing bounds to the source mesh.

Idempotent: an existing wrapper is reused and re-verified, never overwritten.
"""

import json
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/landmark-wrapper-blueprints.json")

# (blueprint asset path, source static mesh path, human label)
WRAPPERS = [
    (ROOT + "/GreatNorthGate/BP_GreatNorthGate",
     ROOT + "/GreatNorthGate/SM_GreatNorthGate",
     "GreatNorthGate"),
    (ROOT + "/ZhenhaiTower/BP_ZhenhaiTower",
     ROOT + "/ZhenhaiTower/SM_ZhenhaiTower",
     "ZhenhaiTower"),
]

# The HISM bound is slightly looser than the mesh bound, so the Z check is a
# sanity band rather than an exact match.
FOOTPRINT_TOLERANCE_CM = 1.0
HEIGHT_TOLERANCE_RATIO = 0.10


def text(value):
    return value.to_string() if hasattr(value, "to_string") else str(value)


def path_of(obj):
    return obj.get_path_name() if obj else ""


def material_slots(mesh):
    slots = []
    for static_material in mesh.get_editor_property("static_materials"):
        material = static_material.get_editor_property("material_interface")
        slots.append(path_of(material) if material else "")
    return slots


def mesh_size(mesh):
    bounds = mesh.get_bounds()
    return (float(bounds.box_extent.x) * 2.0,
            float(bounds.box_extent.y) * 2.0,
            float(bounds.box_extent.z) * 2.0)


def find_root_handle(blueprint, subsystem, library):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if library.is_root_component(data):
            return handle
    return None


def subobject_summary(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    rows = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        rows.append({
            "name": str(library.get_display_name(data)),
            "class": obj.get_class().get_name() if obj else "?",
            "is_root": bool(library.is_root_component(data)),
        })
    return rows


def verify(blueprint_path, expected_size, label):
    """Spawn the wrapper and confirm it reproduces the source mesh footprint."""
    blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
    assert blueprint, blueprint_path
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actor_subsystem.spawn_actor_from_class(
        blueprint.generated_class(), unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0),
        transient=True)
    assert actor, "wrapper did not spawn: " + label
    try:
        origin, extent = actor.get_actor_bounds(False)
        actual = (float(extent.x) * 2.0, float(extent.y) * 2.0, float(extent.z) * 2.0)
        for axis, (got, want) in enumerate(zip(actual[:2], expected_size[:2])):
            assert abs(got - want) <= FOOTPRINT_TOLERANCE_CM, (
                "{} footprint axis {} mismatch: spawned {:.2f} vs mesh {:.2f}"
                .format(label, axis, got, want))
        assert actual[2] >= expected_size[2] * (1.0 - HEIGHT_TOLERANCE_RATIO), (
            "{} height too small: spawned {:.2f} vs mesh {:.2f}"
            .format(label, actual[2], expected_size[2]))
        return {
            "spawned_size_cm": [round(value, 2) for value in actual],
            "spawned_origin_cm": [round(float(origin.x), 2), round(float(origin.y), 2),
                                  round(float(origin.z), 2)],
            "component_count": len(actor.get_components_by_class(unreal.ActorComponent)),
        }
    finally:
        actor_subsystem.destroy_actor(actor)


def wrap(blueprint_path, mesh_path, label):
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    assert isinstance(mesh, unreal.StaticMesh), mesh_path
    expected_size = mesh_size(mesh)

    existing = unreal.EditorAssetLibrary.load_asset(blueprint_path)
    if existing:
        # A wrapper can exist in memory without ever having been written to disk:
        # an earlier run created it, then aborted before save_asset. The asset
        # registry and the level both resolve it, so it looks fine until the
        # editor restarts. Always save on the reuse path too.
        assert unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False), \
            "could not save existing wrapper " + blueprint_path
        entry = {"label": label, "blueprint": blueprint_path, "status": "existing_reused",
                 "source_mesh": mesh_path}
    else:
        blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
            blueprint_path, unreal.PackedLevelActor)
        assert blueprint, blueprint_path

        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary

        root_handle = find_root_handle(blueprint, subsystem, library)
        assert root_handle, "no root component on " + blueprint_path

        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", root_handle)
        params.set_editor_property("new_class", unreal.HierarchicalInstancedStaticMeshComponent)
        params.set_editor_property("blueprint_context", blueprint)
        params.set_editor_property("conform_transform_to_parent", False)
        handle, fail_reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), text(fail_reason)

        data = subsystem.k2_find_subobject_data_from_handle(handle)
        template = library.get_object(data)
        assert template, mesh_path
        template.set_static_mesh(mesh)
        # The mesh already carries its material assignment, but the component's
        # slots are set explicitly so the wrapper does not depend on the mesh's
        # default materials staying put.
        for index, material_path in enumerate(material_slots(mesh)):
            if material_path:
                material = unreal.EditorAssetLibrary.load_asset(material_path)
                if material:
                    template.set_material(index, material)
        # Single mesh at identity: one instance, no transform.
        template.add_instances([unreal.Transform()], False, True, False)

        # compile_blueprint is void in this build; verify by spawning instead.
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False), \
            blueprint_path
        entry = {"label": label, "blueprint": blueprint_path, "status": "created",
                 "parent_class": "/Script/Engine.PackedLevelActor",
                 "source_mesh": mesh_path}

    entry["source_mesh_size_cm"] = [round(value, 2) for value in expected_size]
    entry["material_slots"] = material_slots(mesh)
    entry["subobjects"] = subobject_summary(unreal.EditorAssetLibrary.load_asset(blueprint_path))
    entry["hism_component_count"] = sum(
        1 for row in entry["subobjects"] if "InstancedStaticMesh" in row["class"])
    entry.update(verify(blueprint_path, expected_size, label))
    assert entry["hism_component_count"] == 1, entry["subobjects"]
    # Confirm the asset is genuinely on disk, not just in the editor's memory.
    on_disk = Path(
        "C:/Git/AuraProj/Content" + blueprint_path.split("/Game", 1)[1] + ".uasset")
    assert on_disk.exists(), "wrapper was not written to disk: " + str(on_disk)
    entry["on_disk"] = str(on_disk)
    entry["on_disk_bytes"] = on_disk.stat().st_size
    return entry


def main():
    results = []
    for blueprint_path, mesh_path, label in WRAPPERS:
        entry = wrap(blueprint_path, mesh_path, label)
        results.append(entry)
        print("WRAPPED", json.dumps({k: v for k, v in entry.items()
                                     if k not in ("material_slots", "subobjects")}))

    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps({"wrappers": results, "passed": True}, indent=2),
                        encoding="utf-8")
    unreal.log("LANDMARK_WRAPPER_BLUEPRINTS " + json.dumps(results))
    print("WRAPPER_MANIFEST", str(MANIFEST))


if __name__ == "__main__":
    main()
