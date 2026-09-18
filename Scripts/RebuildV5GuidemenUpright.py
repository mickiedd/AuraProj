"""Rebuild Guidemen HISM children below an explicit upright orientation node."""
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportV5Buildings import _safe_component_name, _transform


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V5"
EAL = unreal.EditorAssetLibrary


def main():
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    cfg = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))[0]
    report_path = ROOT / (cfg["name"] + "-import.json")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    blueprint = EAL.load_asset(report["blueprint"])
    assert isinstance(blueprint, unreal.Blueprint)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    root = next(handle for handle in handles if library.is_root_component(subsystem.k2_find_subobject_data_from_handle(handle)))
    hisms = []
    orientations = []
    for handle in handles:
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            hisms.append(handle)
        elif isinstance(component, unreal.SceneComponent) and component.get_name().startswith("BuildingOrientation"):
            orientations.append(handle)
    for handle in hisms + orientations:
        assert subsystem.delete_subobject(root, handle, blueprint) == 1
    params = unreal.AddNewSubobjectParams(
        parent_handle=root,
        new_class=unreal.SceneComponent,
        blueprint_context=blueprint,
        conform_transform_to_parent=False,
    )
    orientation_handle, reason = subsystem.add_new_subobject(params)
    assert library.is_handle_valid(orientation_handle), str(reason)
    subsystem.rename_subobject(orientation_handle, unreal.Text("BuildingOrientation"))
    orientation = library.get_object(subsystem.k2_find_subobject_data_from_handle(orientation_handle))
    orientation.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    desired = cfg["blueprint_root_rotation"]
    orientation.set_editor_property(
        "relative_rotation",
        unreal.Rotator(pitch=desired[0], yaw=desired[1], roll=desired[2]),
    )
    manifest = []
    for index, group in enumerate(report["scene"]["groups"]):
        params = unreal.AddNewSubobjectParams(
            parent_handle=orientation_handle,
            new_class=unreal.HierarchicalInstancedStaticMeshComponent,
            blueprint_context=blueprint,
            conform_transform_to_parent=False,
        )
        handle, reason = subsystem.add_new_subobject(params)
        assert library.is_handle_valid(handle), str(reason)
        subsystem.rename_subobject(handle, unreal.Text(_safe_component_name(index, group["mesh"])))
        component = library.get_object(subsystem.k2_find_subobject_data_from_handle(handle))
        mesh = EAL.load_asset(group["mesh"])
        component.set_static_mesh(mesh)
        component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("BlockAll")
        component.set_editor_property("cast_shadow", True)
        for material_index, slot in enumerate(mesh.get_editor_property("static_materials")):
            component.set_material(material_index, slot.material_interface)
        transforms = [_transform(record) for record in group["instances"]]
        component.add_instances(transforms, False, True, False)
        assert component.get_instance_count() == len(transforms)
        manifest.append({"component": component.get_name(), "mesh": group["mesh"], "instances": len(transforms)})
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    correction = {
        "passed": True,
        "name": cfg["name"],
        "blueprint": report["blueprint"],
        "orientation_component": "BuildingOrientation",
        "rotation": desired,
        "hism_component_count": len(manifest),
        "instance_count": sum(item["instances"] for item in manifest),
        "geometry_or_instance_transforms_changed": False,
        "manifest": manifest,
    }
    (ROOT / "Guidemen-upright-correction.json").write_text(json.dumps(correction, indent=2), encoding="utf-8")
    print("V5_GUIDEMEN_UPRIGHT_REBUILT", json.dumps({key: value for key, value in correction.items() if key != "manifest"}))


if __name__ == "__main__":
    main()
