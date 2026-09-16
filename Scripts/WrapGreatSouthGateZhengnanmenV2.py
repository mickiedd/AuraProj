"""Wrap the Great South Gate V2 preview scene under one root Actor.

The imported scene intentionally remains a collection of StaticMeshActors so
its repeated meshes stay shared. This script adds a selectable hierarchy root
around those actors and the intact-façade infill, while leaving preview lights
outside the wrapper. All child actors are attached with KEEP_WORLD rules.
"""
import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity"
WRAPPER_LABEL = "GuangzhouLandmark_GreatSouthGate_Zhengnanmen_V2"
ROOT_TAG = "GreatSouthGateZhengnanmenV2Root"
PARK_TAG = "GuangzhouLandmarkPark"
IMPORT_TAG = "ImportedGreatSouthGateZhengnanmenV2"
FILL_PREFIX = "Zhengnanmen_IntactFill_"
SCENE_ROOT_PREFIX = "GreatSouthGate_Zhengnanmen_UE5_HighFidelity_scene"
REPORT_PATH = PROJECT_ROOT / "Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-wrapper.json"


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


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
print("GREAT_SOUTH_GATE_V2_WRAP_STEP_WORLD", world.get_path_name())

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = all_actors()
existing_roots = [
    actor for actor in actors
    if ROOT_TAG in tags(actor) or actor.get_actor_label() == WRAPPER_LABEL
]
assert len(existing_roots) <= 1, "Multiple Great South Gate V2 wrappers exist"

model_actors = sorted(
    [actor for actor in actors if actor not in existing_roots and is_model_actor(actor)],
    key=lambda actor: (actor.get_actor_label(), actor.get_name()),
)
assert model_actors, "No imported Great South Gate V2 actors found"
transform_baseline = [transform_record(actor) for actor in model_actors]
before_digest = transform_digest(model_actors)

if existing_roots:
    root = existing_roots[0]
    print("GREAT_SOUTH_GATE_V2_WRAP_STEP_ROOT_RESUME", root.get_name())
else:
    root = actor_subsystem.spawn_actor_from_class(
        unreal.Actor.static_class(),
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        transient=False,
    )
    assert root, WRAPPER_LABEL
    root.set_actor_label(WRAPPER_LABEL)
    root.tags = [IMPORT_TAG, PARK_TAG, ROOT_TAG]
    print("GREAT_SOUTH_GATE_V2_WRAP_STEP_ROOT", root.get_name())

try:
    root.set_editor_property("is_editor_only_actor", False)
except Exception:
    pass
root.set_actor_hidden_in_game(False)
root.root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

attached_count = 0
for actor in model_actors:
    if actor.get_attach_parent_actor() is not root:
        attached = actor.root_component.attach_to_component(
            root.root_component,
            unreal.Name(""),
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            False,
        )
        assert attached, actor.get_actor_label()
        attached_count += 1
    actor.modify()

root.modify()
after_digest = transform_digest(model_actors)
assert before_digest == after_digest, "Child world transforms changed while wrapping"
direct_children = [actor for actor in all_actors() if actor.get_attach_parent_actor() is root]
assert all(actor in direct_children for actor in model_actors), "One or more model actors were not attached"

lights = [
    actor for actor in all_actors()
    if any(token in actor.get_class().get_name() for token in ("DirectionalLight", "SkyLight"))
]
lights_under_root = [actor.get_actor_label() for actor in lights if actor.get_attach_parent_actor() is root]
assert not lights_under_root, "Preview lights must remain outside the model wrapper"

assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Level save failed"

static_mesh_actors = [actor for actor in model_actors if static_mesh_path(actor).startswith(DEST)]
fill_actors = [actor for actor in model_actors if actor.get_actor_label().startswith(FILL_PREFIX)]
scene_roots = [actor for actor in model_actors if actor.get_actor_label().startswith(SCENE_ROOT_PREFIX)]
manifest = {
    "level": LEVEL_PATH,
    "wrapper_label": WRAPPER_LABEL,
    "wrapper_actor": root.get_name(),
    "wrapper_tags": tags(root),
    "wrapper_transform": transform_record(root),
    "model_child_count": len(model_actors),
    "direct_child_count": len([actor for actor in direct_children if actor in model_actors]),
    "imported_static_mesh_actor_count": len(static_mesh_actors),
    "intact_fill_actor_count": len(fill_actors),
    "imported_scene_root_actor_count": len(scene_roots),
    "attached_this_run": attached_count,
    "lights_outside_wrapper": sorted(actor.get_actor_label() for actor in lights),
    "transform_baseline": transform_baseline,
    "transform_baseline_precision": 4,
    "persistence_tolerance_cm": 0.1,
    "persistence_tolerance_degrees": 0.01,
    "persistence_tolerance_scale": 0.0001,
    "transform_digest_before": before_digest,
    "transform_digest_after": after_digest,
    "bounds_cm": union_bounds(model_actors),
    "shared_meshes_preserved": True,
    "passed": True,
}
REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
REPORT_PATH.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print("GREAT_SOUTH_GATE_ZHENGNANMEN_V2_WRAPPED", json.dumps(manifest))
