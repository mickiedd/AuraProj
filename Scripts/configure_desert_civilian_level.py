"""Place the persistent Civilian test topology in the Sci-Fi Desert level.

Run from an Unreal Editor commandlet with PythonScriptPlugin enabled:

    UnrealEditor-Cmd.exe Aura.uproject -run=pythonscript \
        -script=".../Scripts/configure_desert_civilian_level.py"

The level already contains one large central city and 32 generated outer
villages.  The central cluster is left alone; each outer village receives one
authority-only Civilian spawn volume plus a small set of replicated AI markers.
"""

import math

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
VOLUME_PREFIX = "DesertVillageCiviliansVolume_"
ZONE_ID = "DesertVillages"
BUILDING_HINTS = ("Houses", "Round_buildings", "Base_modules", "House_detail")
CLUSTER_RADIUS = 8000.0
EXPECTED_VILLAGES = 32


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def load_target_level():
    world = editor_world()
    if world and world.get_name() == LEVEL_PATH.rsplit("/", 1)[-1]:
        return world

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH), False, False)
    except Exception:
        world = None
    if world is None:
        try:
            unreal.EditorLevelLibrary.load_level(LEVEL_PATH)
            world = editor_world()
        except Exception as exc:
            raise RuntimeError("Unable to load {}: {}".format(LEVEL_PATH, exc))
    if world is None:
        raise RuntimeError("Unable to load {}".format(LEVEL_PATH))
    return world


def static_mesh_paths(actor):
    paths = []
    try:
        components = actor.get_components_by_class(unreal.ActorComponent)
    except Exception:
        components = []
    for component in components:
        try:
            class_name = component.get_class().get_name()
        except Exception:
            class_name = ""
        if "StaticMesh" not in class_name and "Mesh" not in class_name:
            continue
        mesh = None
        try:
            mesh = component.get_static_mesh()
        except Exception:
            pass
        if mesh is None:
            try:
                mesh = component.get_editor_property("static_mesh")
            except Exception:
                pass
        if mesh is not None:
            try:
                paths.append(mesh.get_path_name())
            except Exception:
                pass
    return paths


def building_candidates(actors):
    buildings = []
    for actor in actors:
        try:
            if not actor.get_class().get_name().startswith("StaticMeshActor"):
                continue
            paths = static_mesh_paths(actor)
            if not any(hint in path for hint in BUILDING_HINTS for path in paths):
                continue
            origin, extent = actor.get_actor_bounds(only_colliding_components=False)
            footprint = max(extent.x * 2.0, extent.y * 2.0)
            if footprint < 80.0:
                continue
            location = actor.get_actor_location()
            buildings.append((location.x, location.y, location.z, footprint))
        except Exception:
            continue
    return buildings


def village_clusters(buildings):
    clusters = []
    used = [False] * len(buildings)
    for index, building in enumerate(buildings):
        if used[index]:
            continue
        used[index] = True
        cluster = [building]
        center_x = building[0]
        center_y = building[1]
        changed = True
        while changed:
            changed = False
            for other_index, other in enumerate(buildings):
                if used[other_index]:
                    continue
                if math.hypot(other[0] - center_x, other[1] - center_y) <= CLUSTER_RADIUS:
                    used[other_index] = True
                    cluster.append(other)
                    count = float(len(cluster))
                    center_x = (center_x * (count - 1.0) + other[0]) / count
                    center_y = (center_y * (count - 1.0) + other[1]) / count
                    changed = True
        clusters.append(cluster)
    return clusters


def cluster_center(cluster):
    count = float(len(cluster))
    return (
        sum(item[0] for item in cluster) / count,
        sum(item[1] for item in cluster) / count,
        sum(item[2] for item in cluster) / count,
    )


def destroy_old_civilian_topology(actors):
    prefixes = (
        "AuraCivilianSpawnVolume",
        "AuraCivilianWorkMarker",
        "AuraCivilianObservationMarker",
        "AuraCivilianShelterMarker",
    )
    removed = 0
    for actor in list(actors):
        try:
            if actor.get_class().get_name().startswith(prefixes):
                if unreal.EditorLevelLibrary.destroy_actor(actor):
                    removed += 1
        except Exception:
            pass
    return removed


def spawn_actor(actor_class, location, label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(location[0], location[1], location[2]), unreal.Rotator(0.0, 0.0, 0.0), transient=False
    )
    if actor is None:
        raise RuntimeError("Failed to spawn {}".format(label))
    actor.set_actor_label(label)
    return actor


def set_property(actor, name, value):
    try:
        actor.set_editor_property(name, value)
    except Exception as exc:
        raise RuntimeError("{}: failed to set {}: {}".format(actor.get_name(), name, exc))


def main():
    world = load_target_level()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    removed = destroy_old_civilian_topology(actors)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    buildings = building_candidates(actors)
    clusters = village_clusters(buildings)
    if len(clusters) < EXPECTED_VILLAGES + 1:
        raise RuntimeError("Expected central city plus {} villages; detected {} clusters".format(EXPECTED_VILLAGES, len(clusters)))

    # The central city is the only cluster with a much larger building count.
    central_index = max(range(len(clusters)), key=lambda index: len(clusters[index]))
    villages = [cluster for index, cluster in enumerate(clusters) if index != central_index]
    villages.sort(key=lambda cluster: (cluster_center(cluster)[0], cluster_center(cluster)[1]))
    if len(villages) != EXPECTED_VILLAGES:
        raise RuntimeError("Expected {} outer villages, detected {}".format(EXPECTED_VILLAGES, len(villages)))

    volume_class = unreal.load_class(None, "/Script/Aura.AuraCivilianSpawnVolume")
    work_class = unreal.load_class(None, "/Script/Aura.AuraCivilianWorkMarker")
    observation_class = unreal.load_class(None, "/Script/Aura.AuraCivilianObservationMarker")
    shelter_class = unreal.load_class(None, "/Script/Aura.AuraCivilianShelterMarker")
    if not all((volume_class, work_class, observation_class, shelter_class)):
        raise RuntimeError("Civilian C++ topology classes could not be loaded")

    spawned_volumes = 0
    spawned_markers = 0
    for index, cluster in enumerate(villages):
        x, y, z = cluster_center(cluster)
        volume = spawn_actor(volume_class, (x, y, z), "DesertVillageCiviliansVolume_{:02d}".format(index))
        set_property(volume, "spawn_volume_id", unreal.Name("{}{:02d}".format(VOLUME_PREFIX, index)))
        set_property(volume, "candidate_extents", unreal.Vector(4000.0, 4000.0, 750.0))
        spawned_volumes += 1

        # Keep a few work/observe destinations available so the same level can
        # exercise Work, Observe, Shelter, and the bounded Wander fallback.
        if index % 4 == 0:
            work = spawn_actor(work_class, (x, y, z + 10.0), "DesertVillageWork_{:02d}".format(index))
            set_property(work, "marker_id", unreal.Name("DesertVillageWork_{:02d}".format(index)))
            set_property(work, "zone_id", unreal.Name(ZONE_ID))
            set_property(work, "capacity", 1)
            set_property(work, "acceptance_radius", 140.0)
            set_property(work, "allowed_work_profile_ids", [unreal.Name("Observer")])
            spawned_markers += 1
        elif index % 4 == 1:
            observation = spawn_actor(observation_class, (x, y, z + 10.0), "DesertVillageObserve_{:02d}".format(index))
            set_property(observation, "marker_id", unreal.Name("DesertVillageObserve_{:02d}".format(index)))
            set_property(observation, "zone_id", unreal.Name(ZONE_ID))
            set_property(observation, "capacity", 1)
            set_property(observation, "acceptance_radius", 140.0)
            spawned_markers += 1

        shelter = spawn_actor(shelter_class, (x + 1000.0, y + 500.0, z + 10.0), "DesertVillageShelter_{:02d}".format(index))
        set_property(shelter, "marker_id", unreal.Name("DesertVillageShelter_{:02d}".format(index)))
        set_property(shelter, "zone_id", unreal.Name(ZONE_ID))
        set_property(shelter, "capacity", 4)
        set_property(shelter, "acceptance_radius", 180.0)
        set_property(shelter, "protection_radius", 600.0)
        spawned_markers += 1

    saved = unreal.EditorLevelLibrary.save_current_level()
    unreal.SystemLibrary.execute_console_command(world, "BuildPaths")
    unreal.log("[DesertCivilianTopology] removed={} villages={} volumes={} markers={} saved={} centralClusterBuildings={}".format(
        removed, len(villages), spawned_volumes, spawned_markers, saved, len(clusters[central_index])))


main()
