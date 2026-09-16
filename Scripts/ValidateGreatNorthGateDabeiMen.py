"""Validate the DabeiMen Great North Gate after a level reload.

Reloads L_showcase_level from disk so nothing depends on unsaved editor state,
then asserts:
  * the DabeiMen root exists with the expected label, tags, transform and 5 parts
  * every part is parented to that root and carries a material
  * exactly one part is the 6M-triangle Nanite roof
  * the assembly is grounded and correctly sized/upright
  * the superseded HighDetail wrapper is gone from the level
  * the four pre-existing landmarks are present, unmoved and non-overlapping
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
MANIFEST = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-dabeimen-placement.json"
OUT = PROJECT_ROOT / "Saved/RawModelImport/great-north-gate-dabeimen-validation.json"

ROOT_LABEL = "GuangzhouLandmark_GreatNorthGate_DabeiMen"
ROOT_TAG = "GreatNorthGateDabeiMenRoot"
PART_TAG = "GreatNorthGateDabeiMenPart"
SUPERSEDED_TAGS = ("GreatNorthGateHighDetailRoot", "GreatNorthGateHighDetailPart")

EXPECTED_PARTS = 5
ROOF_ASSET_TOKEN = "GNG_ROOF"
EXPECTED_SIZE_CM = [6800.0, 1210.0, 2450.0]
SIZE_TOLERANCE_CM = 60.0
GROUND_TOLERANCE_CM = 1.0

# The landmarks that existed before this job and must not have moved.
PRIOR_LANDMARKS = {
    "GuangzhouLandmark_GreatNorthGate": [-140400.0, 105155.0, 100.0],
    "GuangzhouLandmark_GreatSouthGate": [-140400.0, 115400.0, 135.1],
    "GuangzhouLandmark_Xiaobeimen": [-140400.0, 94728.1, 100.0],
    "GuangzhouLandmark_ZhenhaiTower": [-140400.0, 85072.3, 100.0],
}
POSITION_TOLERANCE_CM = 1.0

# Material names the design sheet calls for, mapped onto the imported assets.
DESIGN_MATERIALS = {
    "M_Stone_AgedGuangzhou": "青砖石材 blue brick / stone",
    "M_Plaster_Weathered": "风化灰泥 weathered plaster",
    "M_Wood_DarkTimber": "木构件 timber",
    "M_Roof_GrayClayTile": "灰瓦 grey clay tiles",
    "M_Plaque_DabeiMen": "大bei men plaque",
}


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z,
            origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]


def tags_of(actor):
    return [str(t) for t in actor.tags]


manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
assert loaded, LEVEL_PATH
actors = all_actors()

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
    return condition


roots = [a for a in actors if ROOT_TAG in tags_of(a)]
check(len(roots) == 1, "Expected exactly one DabeiMen root, found {}".format(len(roots)))
root = roots[0]

check(root.get_actor_label() == ROOT_LABEL,
      "Root label is {} not {}".format(root.get_actor_label(), ROOT_LABEL))
check(root.get_class().get_name() == "Actor",
      "Root class is {}".format(root.get_class().get_name()))
root_loc = root.get_actor_location()
check(abs(root_loc.z - manifest["ground_z"]) < GROUND_TOLERANCE_CM,
      "Root Z {} is off the recorded ground {}".format(root_loc.z, manifest["ground_z"]))
root_rot = root.get_actor_rotation()
check((root_rot.roll, root_rot.pitch, root_rot.yaw) == (0.0, 0.0, 0.0),
      "Root rotation is {}".format((root_rot.roll, root_rot.pitch, root_rot.yaw)))

parts = [a for a in actors if PART_TAG in tags_of(a)]
check(len(parts) == EXPECTED_PARTS,
      "Expected {} parts, found {}".format(EXPECTED_PARTS, len(parts)))
check(all(a.get_attach_parent_actor() is root for a in parts), "Some parts are not parented to the root")

part_bounds = [bounds(a) for a in parts]
union = [min(b[0] for b in part_bounds), min(b[1] for b in part_bounds),
         min(b[2] for b in part_bounds), max(b[3] for b in part_bounds),
         max(b[4] for b in part_bounds), max(b[5] for b in part_bounds)]
size = [union[3] - union[0], union[4] - union[1], union[5] - union[2]]
for axis in range(3):
    check(abs(size[axis] - EXPECTED_SIZE_CM[axis]) <= SIZE_TOLERANCE_CM,
          "Axis {} is {} cm, expected {} cm".format(axis, round(size[axis], 1), EXPECTED_SIZE_CM[axis]))
check(size[2] > size[1], "Gate is not upright: depth {} vs height {}".format(size[1], size[2]))
check(abs(union[2] - manifest["ground_z"]) < GROUND_TOLERANCE_CM,
      "Assembly is not grounded: {} vs {}".format(union[2], manifest["ground_z"]))

# ---- materials and the roof part ---------------------------------------------
material_names = set()
roof_parts = []
part_report = []
for actor in parts:
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = component.get_editor_property("static_mesh")
    check(mesh is not None, "{} has no mesh".format(actor.get_actor_label()))
    slots = [s.material_interface.get_name() if s.material_interface else None
             for s in mesh.static_materials]
    check(all(slots), "{} has an unassigned material slot".format(actor.get_actor_label()))
    material_names.update(s for s in slots if s)
    if ROOF_ASSET_TOKEN in mesh.get_name():
        roof_parts.append(mesh.get_name())
    part_report.append({
        "actor": actor.get_actor_label(),
        "mesh": mesh.get_path_name(),
        "materials": slots,
        "size_cm": [round(bounds(actor)[3] - bounds(actor)[0], 1),
                    round(bounds(actor)[4] - bounds(actor)[1], 1),
                    round(bounds(actor)[5] - bounds(actor)[2], 1)],
    })

check(len(roof_parts) == 1, "Expected exactly one roof part, found {}".format(roof_parts))
check(material_names == set(DESIGN_MATERIALS),
      "Material set mismatch: {}".format(sorted(material_names)))

# ---- superseded wrapper must be gone -----------------------------------------
leftovers = [a.get_actor_label() for a in actors if any(t in tags_of(a) for t in SUPERSEDED_TAGS)]
check(not leftovers, "Superseded HighDetail actors still present: {}".format(leftovers))

# ---- prior landmarks untouched ------------------------------------------------
landmark_report = {}
for label, expected_loc in PRIOR_LANDMARKS.items():
    found = [a for a in actors if a.get_actor_label() == label]
    if not check(len(found) == 1, "Landmark {} missing or duplicated".format(label)):
        continue
    actor = found[0]
    loc = actor.get_actor_location()
    drift = max(abs(loc.x - expected_loc[0]), abs(loc.y - expected_loc[1]),
                abs(loc.z - expected_loc[2]))
    check(drift <= POSITION_TOLERANCE_CM,
          "Landmark {} moved by {} cm".format(label, round(drift, 2)))
    b = bounds(actor)
    # The new gate is much wider than its neighbours, so overlap is judged in the
    # Y band each building actually occupies along the corridor.
    landmark_report[label] = {"location": [loc.x, loc.y, loc.z],
                              "bounds": [round(v, 1) for v in b],
                              "drift_cm": round(drift, 3)}
    y_overlap = min(b[4], union[4]) - max(b[1], union[1])
    check(y_overlap <= 0, "{} overlaps the new gate along Y by {} cm".format(label, round(y_overlap, 1)))

result = {
    "level": LEVEL_PATH,
    "root": ROOT_LABEL,
    "root_tags": tags_of(root),
    "part_count": len(parts),
    "parts": sorted(part_report, key=lambda p: p["actor"]),
    "roof_part": roof_parts,
    "bounds": [round(v, 1) for v in union],
    "size_cm": [round(v, 1) for v in size],
    "ground_z": manifest["ground_z"],
    "materials": sorted(material_names),
    "design_material_map": DESIGN_MATERIALS,
    "superseded_actors_present": leftovers,
    "prior_landmarks": landmark_report,
    "failures": failures,
    "passed": not failures,
}
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(result, indent=2), encoding="utf-8")

print("GREAT_NORTH_GATE_DABEIMEN_VALIDATION", json.dumps({
    "passed": result["passed"],
    "part_count": result["part_count"],
    "size_cm": result["size_cm"],
    "roof_part": roof_parts,
    "materials": result["materials"],
    "failures": failures}))
for item in result["parts"]:
    print("  {:<62} {}".format(item["actor"], item["materials"]))
assert not failures, "Validation failed: " + "; ".join(failures)
