"""Wrap the imported Zhengdongmen meshes into a landmark Blueprint.

Follows the convention established by CreateGreatSouthGateActorAsset.py,
WrapLandmarkMeshBlueprints.py and CreateZhengximenLandmarkBlueprint.py: the
wrapper is a PackedLevelActor Blueprint holding one HierarchicalInstancedStaticMeshComponent
per source mesh, with the source transform recorded as an instance transform.

The imported gate is seven meshes - one per material group - each authored in the
building's own space at identity, so each gets one HISM carrying one instance at
identity. Seven components rather than one is what lets the stone base, the
timber gallery, the roof tiles, the plaster panels, the iron doorwork, the timber
door leaves and the 正东门 plaque keep their own materials.

NOTE: unreal.BlueprintEditorLibrary.compile_blueprint returns None in this build
(void, not bool) - asserting on its return value fails even on success. The
wrapper is verified by spawning it and comparing bounds to the union of the
source meshes.

Idempotent: an existing wrapper is reused, saved and re-verified, never rebuilt -
unless it carries the wrong number of components, which is what a crashed run
leaves behind (the asset is created and saved before its components are added).
"""

import json
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen"
MESH_ROOT = ROOT + "/Meshes"
BLUEPRINT_PATH = ROOT + "/BP_Zhengdongmen"
LABEL = "Zhengdongmen"

MESH_GROUPS = ["Stone", "Wood", "RoofTile", "Plaster", "Iron", "DoorWood", "Sign"]

PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
MANIFEST = PROJECT / "Saved" / "RawModelImport" / "zhengdongmen-blueprint.json"

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


def union_size(meshes):
    """Union footprint of the parts, from their absolute bounds.

    Deliberately NOT the largest per-axis extent. Each part has its own origin, so
    the roof tiles (714.7 cm tall, sitting about 12 m up) and the stone base
    (1063 cm tall, on the ground) both under-report the building: the largest
    per-axis extent gives 1063 cm of height where the gate is really 1933 cm. The
    parts share one coordinate space, so the union of their absolute bounds is the
    footprint.
    """
    low = [float("inf")] * 3
    high = [float("-inf")] * 3
    for mesh in meshes:
        bounds = mesh.get_bounds()
        origin, extent = bounds.origin, bounds.box_extent
        for axis, (centre, half) in enumerate(((origin.x, extent.x),
                                               (origin.y, extent.y),
                                               (origin.z, extent.z))):
            low[axis] = min(low[axis], float(centre) - float(half))
            high[axis] = max(high[axis], float(centre) + float(half))
    return tuple(high[axis] - low[axis] for axis in range(3))


def find_root_handle(blueprint, subsystem, library):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        if library.is_root_component(data):
            return handle
    return None


def subobjects(blueprint):
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


def hism_count(blueprint):
    return sum(1 for row in subobjects(blueprint)
               if "InstancedStaticMesh" in row["class"])


def verify(blueprint_path, expected_size, label):
    """Spawn the wrapper and confirm it reproduces the source footprint."""
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


def wrap():
    meshes = []
    for group in MESH_GROUPS:
        mesh = unreal.EditorAssetLibrary.load_asset(
            "{}/SM_ZDM_{}".format(MESH_ROOT, group))
        assert isinstance(mesh, unreal.StaticMesh), group
        meshes.append((group, mesh))
    expected_size = union_size([mesh for _, mesh in meshes])

    existing = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if existing and hism_count(existing) != len(MESH_GROUPS):
        # A wrapper left behind by a run that failed partway - the asset is created
        # and saved before its components are added - must not be reused: it looks
        # like a wrapper while carrying none of the gate.
        unreal.EditorAssetLibrary.delete_asset(BLUEPRINT_PATH)
        existing = None
    if existing:
        # A wrapper can also exist in memory without ever having been written to
        # disk. Always save on the reuse path too, or it vanishes on restart.
        assert unreal.EditorAssetLibrary.save_asset(BLUEPRINT_PATH,
                                                    only_if_is_dirty=False), \
            "could not save existing wrapper " + BLUEPRINT_PATH
        entry = {"label": LABEL, "blueprint": BLUEPRINT_PATH,
                 "status": "existing_reused"}
    else:
        blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
            BLUEPRINT_PATH, unreal.PackedLevelActor)
        assert blueprint, BLUEPRINT_PATH

        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary

        root_handle = find_root_handle(blueprint, subsystem, library)
        assert root_handle, "no root component on " + BLUEPRINT_PATH

        for group, mesh in meshes:
            params = unreal.AddNewSubobjectParams()
            params.set_editor_property("parent_handle", root_handle)
            params.set_editor_property(
                "new_class", unreal.HierarchicalInstancedStaticMeshComponent)
            params.set_editor_property("blueprint_context", blueprint)
            params.set_editor_property("conform_transform_to_parent", False)
            handle, fail_reason = subsystem.add_new_subobject(params)
            assert library.is_handle_valid(handle), text(fail_reason)
            subsystem.rename_subobject(handle, unreal.Text("HISM_" + group))

            data = subsystem.k2_find_subobject_data_from_handle(handle)
            template = library.get_object(data)
            assert template, group
            template.set_static_mesh(mesh)
            # The mesh already carries its material assignment, but the component's
            # slots are set explicitly so the wrapper does not depend on the mesh's
            # default materials staying put.
            for index, material_path in enumerate(material_slots(mesh)):
                if material_path:
                    material = unreal.EditorAssetLibrary.load_asset(material_path)
                    if material:
                        template.set_material(index, material)
            # Each part is authored in the building's own space: one instance at
            # identity, no transform.
            template.add_instances([unreal.Transform()], False, True, False)

        # compile_blueprint is void in this build; verify by spawning instead.
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_asset(BLUEPRINT_PATH,
                                                    only_if_is_dirty=False), \
            BLUEPRINT_PATH
        entry = {"label": LABEL, "blueprint": BLUEPRINT_PATH, "status": "created",
                 "parent_class": "/Script/Engine.PackedLevelActor"}

    entry["meshes"] = {group: mesh.get_path_name() for group, mesh in meshes}
    entry["source_union_size_cm"] = [round(value, 2) for value in expected_size]
    entry["subobjects"] = subobjects(
        unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH))
    entry["hism_component_count"] = sum(
        1 for row in entry["subobjects"] if "InstancedStaticMesh" in row["class"])
    entry.update(verify(BLUEPRINT_PATH, expected_size, LABEL))
    assert entry["hism_component_count"] == len(MESH_GROUPS), entry["subobjects"]

    # Confirm the asset is genuinely on disk, not just in the editor's memory.
    on_disk = PROJECT / ("Content" + BLUEPRINT_PATH.split("/Game", 1)[1] + ".uasset")
    assert on_disk.exists(), "wrapper was not written to disk: " + str(on_disk)
    entry["on_disk"] = str(on_disk)
    entry["on_disk_bytes"] = on_disk.stat().st_size
    return entry


def main():
    entry = wrap()
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps({"wrappers": [entry], "passed": True}, indent=2),
                        encoding="utf-8")
    unreal.log("ZHENGDONGMEN_BLUEPRINT_COMPLETE " + json.dumps(
        {k: v for k, v in entry.items() if k != "subobjects"}))


if __name__ == "__main__":
    main()
