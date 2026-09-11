"""Wrap the aligned Great West Gate parts under one level actor."""
import json
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
ACTOR_TAG = "ImportedGreatWestGate"
PARK_TAG = "GuangzhouLandmarkPark"
ROOT_TAG = "GreatWestGateRoot"
PART_TAG = "GreatWestGatePart"
PARENT_LABEL = "GuangzhouLandmark_GreatWestGate"
PART_PREFIX = "GuangzhouLandmark_GreatWestGate__"
REPORT = Path("C:/Git/AuraProj/Saved/RawModelImport/great-west-gate-wrapper.json")


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith(LEVEL_PATH), world
print("GREAT_WEST_WRAP_STEP_WORLD", world.get_path_name())
actors = all_actors()
existing_root = [actor for actor in actors if ROOT_TAG in [str(tag) for tag in actor.tags]]
assert len(existing_root) <= 1, "Multiple Great West Gate wrappers exist"
parts = [
    actor for actor in actors
    if ACTOR_TAG in [str(tag) for tag in actor.tags]
    or PART_TAG in [str(tag) for tag in actor.tags]
]
assert len(parts) == 20, len(parts)
assert all(actor.get_actor_label().startswith(PART_PREFIX) for actor in parts)
assert all(abs(actor.get_actor_rotation().yaw) < 0.001 for actor in parts), "Parts are not aligned to level yaw"

if existing_root:
    root = existing_root[0]
    print("GREAT_WEST_WRAP_STEP_ROOT_RESUME", root.get_name())
else:
    root_location = parts[0].get_actor_location()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    root = actor_subsystem.spawn_actor_from_class(
        unreal.Actor.static_class(), root_location, unreal.Rotator(0.0, 0.0, 0.0), transient=False)
    assert root, PARENT_LABEL
    print("GREAT_WEST_WRAP_STEP_ROOT", root.get_name())
    root.set_actor_label(PARENT_LABEL)
    root.tags = [ACTOR_TAG, ROOT_TAG, PARK_TAG]

# A static root is required before attaching StaticMeshActor roots.
root.root_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

for part in sorted(parts, key=lambda actor: actor.get_actor_label()):
    if part.get_attach_parent_actor() is not root:
        attached = part.root_component.attach_to_component(
            root.root_component,
            unreal.Name(""),
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            False,
        )
        assert attached, part.get_actor_label()
        print("GREAT_WEST_WRAP_STEP_ATTACHED", part.get_actor_label())
    part.tags = [PART_TAG, PARK_TAG]
    part.modify()

assert root.get_actor_rotation().yaw == 0.0
assert len([actor for actor in all_actors() if actor.get_attach_parent_actor() is root]) == 20
assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Level save failed"

manifest = {
    "level": LEVEL_PATH,
    "root": root.get_actor_label(),
    "root_actor": root.get_name(),
    "root_tags": [str(tag) for tag in root.tags],
    "rotation": [root.get_actor_rotation().roll, root.get_actor_rotation().pitch, root.get_actor_rotation().yaw],
    "location": [root.get_actor_location().x, root.get_actor_location().y, root.get_actor_location().z],
    "part_count": 20,
    "parts": [part.get_actor_label() for part in sorted(parts, key=lambda actor: actor.get_actor_label())],
    "rotation_basis": "matches adjacent Guidemen root and existing landmark row yaw 0.0",
}
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print("GREAT_WEST_GATE_WRAPPED", json.dumps(manifest))
