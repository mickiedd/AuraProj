"""Check what material the showcase ground plane is really using.

The ground renders pure white even with a mid-grey material assigned, and even
with exposure pinned, so the assignment is probably not taking effect. This
reads the ground actor's material slot back from the saved level, tries each
candidate material and reports what sticks, and also reports the plane mesh's
own slot count.
"""

import json

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
GROUND_LABEL = "Showcase_Ground"

CANDIDATES = [
    "/Engine/EngineMaterials/DefaultMaterial",
    "/Engine/EngineMaterials/WorldGridMaterial",
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround",
]


def slots(component):
    out = []
    try:
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            out.append(material.get_path_name() if material else None)
    except Exception as exc:
        out.append("unreadable: {}".format(exc))
    return out


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    ground = next((a for a in actors if a.get_actor_label() == GROUND_LABEL), None)
    assert ground, GROUND_LABEL

    component = ground.get_component_by_class(unreal.StaticMeshComponent)
    mesh = component.static_mesh
    report = {
        "ground_actor": ground.get_name(),
        "ground_actor_class": ground.get_class().get_name(),
        "mesh": mesh.get_path_name() if mesh else None,
        "component_num_materials": component.get_num_materials(),
        "component_materials_now": slots(component),
        "mobility": str(component.get_editor_property("mobility")),
        "actor_scale": [float(ground.get_actor_scale3d().x), float(ground.get_actor_scale3d().y),
                        float(ground.get_actor_scale3d().z)],
    }
    if mesh:
        mesh_slots = []
        for static_material in mesh.get_editor_property("static_materials"):
            material = static_material.get_editor_property("material_interface")
            mesh_slots.append(material.get_path_name() if material else None)
        report["mesh_materials"] = mesh_slots
        try:
            bounds = mesh.get_bounds()
            report["mesh_size_cm"] = [round(float(bounds.box_extent.x) * 2, 2),
                                      round(float(bounds.box_extent.y) * 2, 2),
                                      round(float(bounds.box_extent.z) * 2, 2)]
        except Exception:
            pass

    # Try each candidate and record what the slot reports afterwards.
    attempts = []
    for path in CANDIDATES:
        material = unreal.EditorAssetLibrary.load_asset(path)
        if material is None:
            attempts.append({"material": path, "loaded": False})
            continue
        try:
            component.set_material(0, material)
        except Exception as exc:
            attempts.append({"material": path, "set_failed": str(exc)})
            continue
        attempts.append({"material": path, "loaded": True, "slot_after": slots(component)})
    report["attempts"] = attempts

    unreal.log("SHOWCASE_GROUND_MATERIAL_CHECK " + json.dumps(report))
    print("SHOWCASE_GROUND_MATERIAL_CHECK", json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
