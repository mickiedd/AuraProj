"""Import the supplied Great West Gate GLB as a new building and place it.

The source contains 20 high-poly mesh parts (about 5.7M triangles).  They are
imported as separate static meshes to avoid the editor's unstable combined
Nanite build path; the parts share one world transform and are tagged as one
building group in the level.
"""
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path("C:/Git/AuraProj")
SOURCE = Path("C:/Works/Raw3DModels/SM_Zhengximen_GreatWestGate_5M_PBR.glb")
LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
DEST = "/Game/Assets/Environment/GuangzhouLandmarks/GreatWestGate"
ACTOR_PREFIX = "GuangzhouLandmark_GreatWestGate__"
ACTOR_TAG = "ImportedGreatWestGate"
PARK_TAG = "GuangzhouLandmarkPark"
ROOT_TAG = "GreatWestGateRoot"
PART_TAG = "GreatWestGatePart"
PLACEMENT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/great-west-gate-placement.json"
IMPORT_REPORT = PROJECT_ROOT / "Saved/RawModelImport/GreatWestGate.json"
EDGE_MARGIN = 12000.0
GRID_STEP = 5000.0
CLEARANCE = 2500.0

assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT_ROOT.resolve(), "Wrong Unreal project"
assert SOURCE.exists(), SOURCE


def actor_bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y,
            origin.x + extent.x, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z)


def overlaps(left, right, padding=CLEARANCE):
    return (left[0] - padding < right[2] and left[2] + padding > right[0] and
            left[1] - padding < right[3] and left[3] + padding > right[1])


def editor_world():
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    except Exception:
        loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
        world = (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
                 if loaded else None)
    assert world, LEVEL_PATH
    return world


def all_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def great_west_parts(actors):
    wrapped = [actor for actor in actors if PART_TAG in [str(tag) for tag in actor.tags]]
    if wrapped:
        return wrapped
    return [actor for actor in actors
            if ACTOR_TAG in [str(tag) for tag in actor.tags]
            and actor.get_actor_label().startswith(ACTOR_PREFIX)]


def terrain_rect(actors):
    landscapes = [a for a in actors if a.get_class().get_name().startswith("Landscape")]
    assert landscapes, "No Landscape actors found"
    boxes = [actor_bounds(a) for a in landscapes]
    return (min(b[0] for b in boxes) + EDGE_MARGIN,
            min(b[1] for b in boxes) + EDGE_MARGIN,
            max(b[2] for b in boxes) - EDGE_MARGIN,
            max(b[3] for b in boxes) - EDGE_MARGIN)


def trace_ground_z(world, x, y, ignore):
    object_type = getattr(unreal.ObjectTypeQuery, "OBJECT_TYPE_QUERY1", None)
    draw = getattr(unreal.DrawDebugTrace, "NONE", None)
    if object_type is None or draw is None:
        return 0.0
    types = unreal.Array(unreal.ObjectTypeQuery)
    types.append(object_type)
    hit = unreal.SystemLibrary.line_trace_single_for_objects(
        world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -5000.0),
        types, True, ignore, draw, False)
    data = hit.to_tuple()
    return float(data[4].z) if data and data[0] is True else 0.0


def discover_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([DEST], True)
    meshes = []
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            meshes.append(asset)
    return sorted(meshes, key=lambda mesh: mesh.get_name())


def import_meshes():
    existing = discover_meshes()
    if existing:
        assert len(existing) >= 10, "Partial Great West import found; refusing to mix assets"
        return existing, False, [mesh.get_path_name() for mesh in existing]

    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.asset_name = "SM_GreatWestGate"
    pipeline.import_offset_rotation = unreal.Rotator(roll=-90.0)
    pipeline.import_offset_uniform_scale = 1.0
    pipeline.common_meshes_properties.bake_meshes = True
    pipeline.mesh_pipeline.combine_static_meshes = False
    # The supplied source is roughly 5.7M triangles. Importing its 20 parts as
    # regular static meshes avoids the editor crash seen in the combined Nanite
    # build while preserving the source geometry and embedded PBR materials.
    pipeline.mesh_pipeline.build_nanite = False
    pipeline.mesh_pipeline.set_editor_property("collision", False)
    pipeline.mesh_pipeline.generate_lightmap_u_vs = False

    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = False
    params.override_pipelines = [unreal.SoftObjectPath(pipeline.get_path_name())]
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    objects = manager.import_asset(DEST, manager.create_source_data(str(SOURCE)), params)
    assert objects, "Import returned no Great West objects"
    assert unreal.EditorAssetLibrary.save_directory(DEST), "Great West asset save failed"
    meshes = discover_meshes()
    assert len(meshes) >= 10, "Expected imported Great West mesh parts"
    assert all(slot.material_interface for mesh in meshes for slot in mesh.static_materials), "Unassigned Great West material"
    return meshes, True, [obj.get_path_name() for obj in objects]


def union_mesh_bounds(meshes):
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3
    for mesh in meshes:
        bounds = mesh.get_bounds()
        values_min = [bounds.origin.x - bounds.box_extent.x,
                      bounds.origin.y - bounds.box_extent.y,
                      bounds.origin.z - bounds.box_extent.z]
        values_max = [bounds.origin.x + bounds.box_extent.x,
                      bounds.origin.y + bounds.box_extent.y,
                      bounds.origin.z + bounds.box_extent.z]
        mins = [min(a, b) for a, b in zip(mins, values_min)]
        maxs = [max(a, b) for a, b in zip(maxs, values_max)]
    return mins, maxs


world = editor_world()
meshes, imported, imported_objects = import_meshes()
local_min, local_max = union_mesh_bounds(meshes)
local_center = [(a + b) / 2.0 for a, b in zip(local_min, local_max)]
mesh_size = [b - a for a, b in zip(local_min, local_max)]
import_report = {
    "source": str(SOURCE),
    "destination": DEST,
    "imported_this_run": imported,
    "mesh_count": len(meshes),
    "mesh_paths": [mesh.get_path_name() for mesh in meshes],
    "union_bounds_cm": {"min": local_min, "max": local_max},
    "size_cm": mesh_size,
    "nanite": False,
    "material_slots": sum(len(mesh.static_materials) for mesh in meshes),
    "materials": sorted({slot.material_interface.get_path_name()
                          for mesh in meshes for slot in mesh.static_materials}),
    "imported_objects": imported_objects,
}
IMPORT_REPORT.parent.mkdir(parents=True, exist_ok=True)
IMPORT_REPORT.write_text(json.dumps(import_report, indent=2), encoding="utf-8")

actors = all_actors()
root_group = [a for a in actors if ROOT_TAG in [str(t) for t in a.tags]]
assert len(root_group) <= 1, "Multiple Great West Gate roots found"
group = great_west_parts(actors)
assert len(group) in (0, len(meshes)), "Partial Great West actor group found"

if group:
    group.sort(key=lambda actor: actor.get_actor_label())
    # Keep the existing group transform on a resumable run.
    shared_location = group[0].get_actor_location()
    existing_by_label = {actor.get_actor_label(): actor for actor in group}
else:
    existing_boxes = [actor_bounds(a) for a in actors
                      if a.get_class().get_name().startswith("StaticMeshActor")]
    rect = terrain_rect(actors)
    candidates = []
    x = rect[0]
    while x <= rect[2]:
        y = rect[3]
        while y >= rect[1]:
            candidates.append((x, y))
            y -= GRID_STEP
        x += GRID_STEP
    selected = None
    for center_x, center_y in candidates:
        candidate = (center_x - (mesh_size[0] / 2.0), center_y - (mesh_size[1] / 2.0),
                     center_x + (mesh_size[0] / 2.0), center_y + (mesh_size[1] / 2.0), 0.0, 0.0)
        if not any(overlaps(candidate, old) for old in existing_boxes):
            selected = (center_x, center_y)
            break
    assert selected, "No clear Great West Gate footprint found"
    ignore = unreal.Array(unreal.Actor)
    for candidate_actor in actors:
        if not candidate_actor.get_class().get_name().startswith("Landscape"):
            ignore.append(candidate_actor)
    ground = trace_ground_z(world, selected[0], selected[1], ignore)
    shared_location = unreal.Vector(selected[0] - local_center[0],
                                    selected[1] - local_center[1],
                                    ground - local_min[2])
    existing_by_label = {}

for mesh in meshes:
    label = ACTOR_PREFIX + mesh.get_name()
    actor = existing_by_label.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor.static_class(), shared_location,
            unreal.Rotator(0.0, 0.0, 0.0), transient=False)
        assert actor, label
        actor.set_actor_label(label)
        actor.tags = [ACTOR_TAG, PARK_TAG]
    actor.set_actor_location(shared_location, False, True)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    assert component, label
    component.set_static_mesh(mesh)
    actor.modify()

group = great_west_parts(all_actors())
group_bounds = [actor_bounds(a) for a in group]
actual_bounds = [min(b[0] for b in group_bounds), min(b[1] for b in group_bounds),
                 max(b[2] for b in group_bounds), max(b[3] for b in group_bounds),
                 min(b[4] for b in group_bounds), max(b[5] for b in group_bounds)]
assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "Failed to save showcase level"
placement_report = {
    "level": LEVEL_PATH,
    "source": str(SOURCE),
    "asset_folder": DEST,
    "tag": ACTOR_TAG,
    "actors": [a.get_actor_label() for a in sorted(group, key=lambda actor: actor.get_actor_label())],
    "location": [shared_location.x, shared_location.y, shared_location.z],
    "center_xy": [(actual_bounds[0] + actual_bounds[2]) / 2.0,
                   (actual_bounds[1] + actual_bounds[3]) / 2.0],
    "ground_z": actual_bounds[4],
    "bounds": actual_bounds,
    "size_cm": mesh_size,
    "imported": imported,
    "passed": True,
}
PLACEMENT_REPORT.write_text(json.dumps(placement_report, indent=2), encoding="utf-8")
print("GREAT_WEST_GATE_IMPORT", json.dumps(import_report))
print("GREAT_WEST_GATE_PLACEMENT", json.dumps(placement_report))
