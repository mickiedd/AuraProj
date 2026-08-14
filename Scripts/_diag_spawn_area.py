"""Inspect the BungeeMan spawn area and role presentation assets in UE5.5."""

import unreal


SPAWN = unreal.Vector(-143402.0, 127141.0, 200.0)
RADIUS = 20000.0


def all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()


def mesh_path(actor):
    try:
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        components = []
    for component in components:
        try:
            mesh = component.get_editor_property("static_mesh")
            if mesh:
                return mesh.get_path_name()
        except Exception:
            pass
    return ""


def asset_sockets(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    print("asset {} loaded={}".format(path, bool(asset)))
    if not asset:
        return
    methods = [name for name in dir(asset) if "socket" in name.lower()]
    print("  socket API: {}".format(methods))
    skeleton = None
    try:
        skeleton = asset.get_editor_property("skeleton")
        print("  skeleton={}".format(skeleton))
    except Exception as exc:
        print("  skeleton read failed: {}".format(exc))
    if skeleton:
        print("  skeleton socket API: {}".format([name for name in dir(skeleton) if "socket" in name.lower()]))
    for name in ("WeaponHandSocket", "Muzzle"):
        try:
            socket = asset.find_socket(unreal.Name(name))
            print("  find_socket {} -> {}".format(name, socket))
            if socket:
                print("    socket API: {}".format([item for item in dir(socket) if "socket" in item.lower() or "location" in item.lower() or "rotation" in item.lower()]))
                for prop in ("socket_name", "bone_name", "relative_location", "relative_rotation", "relative_scale"):
                    try:
                        print("    {}={}".format(prop, socket.get_editor_property(prop)))
                    except Exception:
                        pass
        except Exception as exc:
            print("  find_socket {} failed: {}".format(name, exc))


def main():
    print("==== _diag_spawn_area start ====")
    print("editor world={}".format(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()))
    nearby = []
    for actor in all_actors():
        try:
            loc = actor.get_actor_location()
            distance = ((loc.x - SPAWN.x) ** 2 + (loc.y - SPAWN.y) ** 2) ** 0.5
            if distance <= RADIUS:
                nearby.append((distance, actor))
        except Exception:
            pass
    nearby.sort(key=lambda item: item[0])
    print("nearby actors radius={} count={}".format(RADIUS, len(nearby)))
    for distance, actor in nearby[:80]:
        cls = actor.get_class().get_name()
        path = mesh_path(actor)
        if cls in ("PlayerStart", "StaticMeshActor") or "Landscape" in cls:
            print("  d={:7.0f} cls={} name={} loc={} mesh={}".format(
                distance, cls, actor.get_name(), actor.get_actor_location(), path))

    asset_sockets("/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan")
    asset_sockets("/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B.Assault_Rifle_B")
    print("==== _diag_spawn_area done ====")


main()
