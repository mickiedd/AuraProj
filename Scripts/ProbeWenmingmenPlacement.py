"""Probe where the placed Landmark_Wenmingmen sits and which way it faces.

Read-only. Loads L_GuangzhouLandmarkShowcase, finds the existing
``Landmark_Wenmingmen`` actor and reports two things in the actor's OWN local
frame, which is the frame the builder's yaw turns:

1. the local-space extent of every rendered component (X width, Y depth, Z up);
2. the local position of ``Light_Wenmingmen``, which is built on the plaza side
   by ``CreateGuangzhouLandmarkShowcase.py`` and therefore marks where the plaza
   is in that same frame.

Why this exists: on 2026-09-25 Wenmingmen was found placed back to front. Its
facade - the closed timber door, the couplets and the climbing vines - sits on
local **+Y**, together with its canal and bridge, while the plaza is on local
**-Y**, so ``facing_offset 0`` pointed the gate away from the viewer. Four
readings back that up and this script prints all of them:

  * ``SMC_water``, the canal the bridge crosses, spans local Y 710..1810. The
    Blender front render (``Saved/Reports/Wenmingmen/final-front.png``) shows the
    door on the canal side of the model, and no rotation can change that
    relationship, so the door is on +Y.
  * ``SMC_iron``, the door's ironwork, is a thin plate at local Y ~200.
  * ``SMC_foliage``, the vines, is a band at local Y ~200 on that same face.
  * ``SMC_stone`` / ``SMC_limestone``, the paved platform, START at local Y ~-230
    (the -Y facade plane) and run out to +1867 / +2351, so the apron the gate is
    approached over lies in +Y only.

MEASUREMENT TRAP, and the reason this script does not simply add
``relative_location`` to the mesh bounds: these six components are plain
``StaticMeshComponent``s carrying a **-90 degree component rotation**, so the
naive reading is wrong by a whole axis - it reports a local Z floor of about
-2351 cm instead of -332.5. The correct reading transforms the mesh bounds
corners by each component's **world transform** (``get_world_transform``), which
already folds in the component rotation, its scale and its translation. That
route reproduces the Blueprint bounds exactly (6500 x 2806.90 x 2199.01 cm), which
is how it was confirmed. ``unreal.PrimitiveComponent.bounds`` is not exposed to
Python, and ``get_editor_property("relative_transform")`` raises on these
components, so the world transform is also the only route that works.

Run headless, from the project root::

    /Volumes/M2/Engine/UE_5.5/Engine/Binaries/Mac/UnrealEditor-Cmd \\
      /Volumes/M2/Works/AuraProj/Aura.uproject -run=pythonscript \\
      -script=/Volumes/M2/Works/AuraProj/Scripts/ProbeWenmingmenPlacement.py \\
      -unattended -nopause -nosplash -nullrhi -stdout

Writes ``Saved/RawModelImport/wenmingmen-placement-probe.json`` and prints a
summary. The level is never saved.
"""

import json
import math
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
LANDMARK_LABEL = "Landmark_Wenmingmen"
LIGHT_LABEL = "Light_Wenmingmen"
OUT = (Path(unreal.Paths.project_dir())
       / "Saved/RawModelImport/wenmingmen-placement-probe.json")


def corners_of(mesh):
    """The eight corners of a mesh's own local bounds box."""
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    xs = (float(origin.x) - float(extent.x), float(origin.x) + float(extent.x))
    ys = (float(origin.y) - float(extent.y), float(origin.y) + float(extent.y))
    zs = (float(origin.z) - float(extent.z), float(origin.z) + float(extent.z))
    return [(x, y, z) for x in xs for y in ys for z in zs]


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, "could not load " + LEVEL_PATH

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    landmark = None
    light = None
    for actor in actors:
        label = actor.get_actor_label()
        if label == LANDMARK_LABEL:
            landmark = actor
        elif label == LIGHT_LABEL:
            light = actor
    assert landmark is not None, LANDMARK_LABEL + " is not in " + LEVEL_PATH

    origin = landmark.get_actor_location()
    yaw = float(landmark.get_actor_rotation().yaw)
    cos_yaw = math.cos(math.radians(-yaw))
    sin_yaw = math.sin(math.radians(-yaw))

    def to_local(point):
        """World point -> the actor's local frame (yaw only; Z is unchanged)."""
        dx = float(point.x) - float(origin.x)
        dy = float(point.y) - float(origin.y)
        return (dx * cos_yaw - dy * sin_yaw,
                dx * sin_yaw + dy * cos_yaw,
                float(point.z) - float(origin.z))

    rows = []
    union = None
    for component in landmark.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if mesh is None:
            continue
        try:
            visible = bool(component.get_editor_property("visible"))
        except Exception:
            visible = True
        if not visible:
            continue

        world = component.get_world_transform()
        xs, ys, zs = [], [], []
        for corner in corners_of(mesh):
            local = to_local(world.transform_location(unreal.Vector(*corner)))
            xs.append(local[0])
            ys.append(local[1])
            zs.append(local[2])

        row = {
            "component": component.get_name(),
            "mesh": mesh.get_name(),
            "local_x": [round(min(xs), 2), round(max(xs), 2)],
            "local_y": [round(min(ys), 2), round(max(ys), 2)],
            "local_z": [round(min(zs), 2), round(max(zs), 2)],
        }
        rows.append(row)

        box = [row["local_x"], row["local_y"], row["local_z"]]
        if union is None:
            union = [list(axis) for axis in box]
        else:
            for index, axis in enumerate(box):
                union[index][0] = min(union[index][0], axis[0])
                union[index][1] = max(union[index][1], axis[1])

    rows.sort(key=lambda row: row["local_y"][1])

    report = {
        "level": LEVEL_PATH,
        "landmark": LANDMARK_LABEL,
        "actor_location": [round(float(origin.x), 2), round(float(origin.y), 2),
                           round(float(origin.z), 2)],
        "actor_yaw": round(yaw, 3),
        "ring_angle_deg": round(math.degrees(math.atan2(
            float(origin.y), float(origin.x))) % 360.0, 3),
        "local_union": union,
        "local_size_cm": ([round(axis[1] - axis[0], 2) for axis in union]
                          if union else None),
        "components": rows,
    }

    # The light is built on the plaza side, so its local Y says which local Y
    # sign is "toward the plaza".
    if light is not None:
        light_local = to_local(light.get_actor_location())
        report["light"] = {
            "label": LIGHT_LABEL,
            "local_x": round(light_local[0], 2),
            "local_y": round(light_local[1], 2),
            "plaza_side_is_local_y": "negative" if light_local[1] < 0 else "positive",
        }
    else:
        report["light"] = None

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("WROTE " + str(OUT))
    print("actor_location {}".format(report["actor_location"]))
    print("local_size_cm (X, Y, Z) {}".format(report["local_size_cm"]))
    if report["light"]:
        print("plaza side is local Y {}".format(report["light"]["plaza_side_is_local_y"]))
    print("{:<26} {:>21} {:>21} {:>21}".format(
        "component", "local X", "local Y", "local Z"))
    for row in rows:
        print("{:<26} {:>21} {:>21} {:>21}".format(
            row["component"], str(row["local_x"]), str(row["local_y"]),
            str(row["local_z"])))


if __name__ == "__main__":
    main()
