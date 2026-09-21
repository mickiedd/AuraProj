"""Inspect the candidate ground materials for the showcase level.

The showcase ground renders as blown-out white, which drives auto-exposure and
turns every landmark into a black silhouette. This checks what
M_V5PreviewGround actually is (shading model, emissive, unlit?) and which
neutral lit materials are available to use instead.

Read-only.
"""

import json

import unreal

CANDIDATES = [
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround",
    "/Game/Assets/Environment/GuangzhouLandmarks/V3/M_V3PreviewGround",
    "/Game/Assets/Environment/GuangzhouLandmarks/UltraAAA/M_UltraAAA_CaptureGround",
    "/Engine/EngineMaterials/WorldGridMaterial",
    "/Engine/EngineMaterials/DefaultMaterial",
    "/Engine/EngineMaterials/DefaultDeferredDecalMaterial",
]


def describe(path):
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        return {"path": path, "exists": False}
    entry = {"path": path, "exists": True, "class": material.get_class().get_name()}
    for attribute in ("shading_model", "blend_mode", "two_sided",
                      "used_with_static_lighting", "material_domain"):
        try:
            entry[attribute] = str(material.get_editor_property(attribute))
        except Exception:
            pass
    try:
        inputs = material.get_editor_property("material_inputs")
        connected = []
        for index, expression_input in enumerate(inputs):
            try:
                name = str(expression_input.get_editor_property("input_name"))
                text = str(expression_input.get_editor_property("input"))
                if text and text != "None":
                    connected.append(name)
            except Exception:
                pass
        entry["connected_inputs"] = connected
    except Exception as exc:
        entry["connected_inputs"] = "unreadable: {}".format(exc)
    return entry


report = [describe(path) for path in CANDIDATES]
unreal.log("GROUND_MATERIAL_PROBE " + json.dumps(report))
print("GROUND_MATERIAL_PROBE", json.dumps(report, indent=2))
