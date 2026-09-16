"""Validate the persisted Great South Gate V2 root Actor after reload."""
import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
WRAPPER_LABEL = "GuangzhouLandmark_GreatSouthGate_Zhengnanmen_V2"
ROOT_TAG = "GreatSouthGateZhengnanmenV2Root"
FILL_PREFIX = "Zhengnanmen_IntactFill_"
SCENE_ROOT_PREFIX = "GreatSouthGate_Zhengnanmen_UE5_HighFidelity_scene"
REPORT_PATH = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-wrapper.json"
VALIDATION_PATH = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-wrapper-validation.json"


def tags(actor):
    return [str(tag) for tag in actor.tags]


def static_mesh_path(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or not component.static_mesh:
        return ""
    return component.static_mesh.get_path_name()


def is_model_actor(actor):
    label = actor.get_actor_label()
    return (
        static_mesh_path(actor).startswith(DEST)
        or label.startswith(FILL_PREFIX)
        or label.startswith(SCENE_ROOT_PREFIX)
    )


def transform_record(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return {
        "label": actor.get_actor_label(),
        "name": actor.get_name(),
        "location": [round(location.x, 4), round(location.y, 4), round(location.z, 4)],
        "rotation": [round(rotation.roll, 4), round(rotation.pitch, 4), round(rotation.yaw, 4)],
        "scale": [round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)],
    }


def transform_digest(actors):
    payload = [transform_record(actor) for actor in sorted(actors, key=lambda item: (item.get_actor_label(), item.get_name()))]
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def actor_bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [
        origin.x - extent.x,
        origin.y - extent.y,
        origin.z - extent.z,
        origin.x + extent.x,
        origin.y + extent.y,
        origin.z + extent.z,
    ]


def union_bounds(actors):
    values = [actor_bounds(actor) for actor in actors]
    return [
        min(item[0] for item in values),
        min(item[1] for item in values),
        min(item[2] for item in values),
        max(item[3] for item in values),
        max(item[4] for item in values),
        max(item[5] for item in values),
    ]


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level_editor.load_level(LEVEL_PATH), LEVEL_PATH
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith(LEVEL_PATH), world
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

roots = [
    actor for actor in actors
    if ROOT_TAG in tags(actor) or actor.get_actor_label() == WRAPPER_LABEL
]
assert len(roots) == 1, "Expected one Great South Gate V2 wrapper, found {}".format(len(roots))
root = roots[0]
assert root.get_actor_label() == WRAPPER_LABEL
assert ROOT_TAG in tags(root)

location = root.get_actor_location()
rotation = root.get_actor_rotation()
scale = root.get_actor_scale3d()
assert max(abs(location.x), abs(location.y), abs(location.z)) < 0.01, location
assert max(abs(rotation.roll), abs(rotation.pitch), abs(rotation.yaw)) < 0.01, rotation
assert max(abs(scale.x - 1.0), abs(scale.y - 1.0), abs(scale.z - 1.0)) < 0.01, scale

model_actors = sorted(
    [actor for actor in actors if actor is not root and is_model_actor(actor)],
    key=lambda actor: (actor.get_actor_label(), actor.get_name()),
)
imported_static_mesh_actors = [actor for actor in model_actors if static_mesh_path(actor).startswith(DEST)]
fill_actors = [actor for actor in model_actors if actor.get_actor_label().startswith(FILL_PREFIX)]
scene_roots = [actor for actor in model_actors if actor.get_actor_label().startswith(SCENE_ROOT_PREFIX)]
unparented = [actor.get_actor_label() for actor in model_actors if actor.get_attach_parent_actor() is not root]
assert not unparented, "Unparented model actors: " + ", ".join(unparented[:12])

lights = [
    actor for actor in actors
    if any(token in actor.get_class().get_name() for token in ("DirectionalLight", "SkyLight"))
]
lights_under_root = [actor.get_actor_label() for actor in lights if actor.get_attach_parent_actor() is root]
assert not lights_under_root, "Lights were attached to wrapper: " + ", ".join(lights_under_root)

manifest = json.loads(REPORT_PATH.read_text(encoding="utf-8"))
assert manifest["wrapper_label"] == WRAPPER_LABEL
assert manifest["model_child_count"] == len(model_actors)
assert manifest["imported_static_mesh_actor_count"] == len(imported_static_mesh_actors)
assert manifest["intact_fill_actor_count"] == len(fill_actors)
assert manifest["imported_scene_root_actor_count"] == len(scene_roots)
assert manifest["transform_digest_before"] == manifest["transform_digest_after"]
current_digest = transform_digest(model_actors)
baseline = {(item["label"], item["name"]): item for item in manifest["transform_baseline"]}
current_keys = {(actor.get_actor_label(), actor.get_name()) for actor in model_actors}
assert set(baseline) == current_keys, "Transform baseline actor set changed"
max_location_delta = 0.0
max_rotation_delta = 0.0
max_scale_delta = 0.0
for actor in model_actors:
    expected = baseline[(actor.get_actor_label(), actor.get_name())]
    actual = transform_record(actor)
    max_location_delta = max(
        max_location_delta,
        max(abs(actual["location"][i] - expected["location"][i]) for i in range(3)),
    )
    max_rotation_delta = max(
        max_rotation_delta,
        max(abs(actual["rotation"][i] - expected["rotation"][i]) for i in range(3)),
    )
    max_scale_delta = max(
        max_scale_delta,
        max(abs(actual["scale"][i] - expected["scale"][i]) for i in range(3)),
    )
assert max_location_delta <= manifest["persistence_tolerance_cm"], max_location_delta
assert max_rotation_delta <= manifest["persistence_tolerance_degrees"], max_rotation_delta
assert max_scale_delta <= manifest["persistence_tolerance_scale"], max_scale_delta

direct_children = [actor for actor in actors if actor.get_attach_parent_actor() is root]
assert len([actor for actor in direct_children if actor in model_actors]) == len(model_actors)
actual_bounds = union_bounds(model_actors)
expected_bounds = manifest["bounds_cm"]
assert all(abs(actual_bounds[index] - expected_bounds[index]) < 0.1 for index in range(6)), (
    actual_bounds,
    expected_bounds,
)

result = {
    "passed": True,
    "level": LEVEL_PATH,
    "wrapper": root.get_actor_label(),
    "root_actor": root.get_name(),
    "model_child_count": len(model_actors),
    "direct_model_child_count": len([actor for actor in direct_children if actor in model_actors]),
    "imported_static_mesh_actor_count": len(imported_static_mesh_actors),
    "intact_fill_actor_count": len(fill_actors),
    "imported_scene_root_actor_count": len(scene_roots),
    "light_count_outside_wrapper": len(lights),
    "lights_under_wrapper": lights_under_root,
    "transform_digest": current_digest,
    "max_location_delta_cm": max_location_delta,
    "max_rotation_delta_degrees": max_rotation_delta,
    "max_scale_delta": max_scale_delta,
    "bounds_cm": actual_bounds,
    "errors": [],
}
VALIDATION_PATH.parent.mkdir(parents=True, exist_ok=True)
VALIDATION_PATH.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("GREAT_SOUTH_GATE_ZHENGNANMEN_V2_WRAPPER_VALIDATION", json.dumps(result))
