"""Persist the BungeeMan rifle attachment alignment in the body socket asset."""

import unreal


BODY_MESH_PATH = "/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan"
SOCKET_NAME = "WeaponHandSocket"
ATTACH_OFFSET = unreal.Vector(0.0, 0.0, -35.0)
# Unreal Python's positional Rotator constructor is (roll, pitch, yaw). This
# produces the previewed socket rotation Pitch=90, Yaw=0, Roll=15.
ATTACH_ROTATION = unreal.Rotator(15.0, 90.0, 0.0)


def prop(socket, name):
    return socket.get_editor_property(name)


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_editor.is_in_play_in_editor():
        unreal.EditorLevelLibrary.editor_end_play()

    mesh = unreal.EditorAssetLibrary.load_asset(BODY_MESH_PATH)
    if not mesh:
        raise RuntimeError("Could not load {}".format(BODY_MESH_PATH))
    socket = mesh.find_socket(unreal.Name(SOCKET_NAME))
    if not socket:
        raise RuntimeError("Could not find {} on {}".format(SOCKET_NAME, BODY_MESH_PATH))

    old_location = prop(socket, "relative_location")
    old_rotation = prop(socket, "relative_rotation")
    old_scale = prop(socket, "relative_scale")
    old_location_values = (old_location.x, old_location.y, old_location.z)
    old_rotation_values = (old_rotation.pitch, old_rotation.yaw, old_rotation.roll)
    old_scale_values = (old_scale.x, old_scale.y, old_scale.z)
    socket.set_editor_property("relative_location", ATTACH_OFFSET)
    socket.set_editor_property("relative_rotation", ATTACH_ROTATION)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton:
        unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False)

    print("asset={}".format(BODY_MESH_PATH))
    print("socket={} bone={}".format(prop(socket, "socket_name"), prop(socket, "bone_name")))
    print("old location={} rotation={} scale={}".format(old_location_values, old_rotation_values, old_scale_values))
    print("new location={} rotation={} scale={}".format(
        prop(socket, "relative_location"), prop(socket, "relative_rotation"), prop(socket, "relative_scale")))
    print("saved mesh={} skeleton={}".format(mesh, skeleton))


main()
