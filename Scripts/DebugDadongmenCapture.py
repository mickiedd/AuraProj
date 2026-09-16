import unreal

path = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/L_Dadongmen_V4_Preview"
le = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
print("LOAD", le.load_level(path))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if a.get_actor_label().startswith("Preview_Dadongmen"):
        origin, extent = a.get_actor_bounds(False)
        radius = max(extent.x, extent.y, extent.z)
        target = unreal.Vector(origin.x, origin.y, origin.z * .9)
        cam = target + unreal.Vector(radius * 1.2, radius * 2.15, radius * .85)
        print("BUILDING", a.get_actor_label(), a.get_actor_location(), (origin, extent), "RADIUS", radius, "TARGET", target, "CAPTURE_CAM", cam)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
print("WORLD", editor.get_editor_world().get_name())
print("SET_CAMERA", editor.set_level_viewport_camera_info(unreal.Vector(5000, 5000, 3000), unreal.Rotator(pitch=-20, yaw=-135, roll=0)))
try:
    print("CAMERA_INFO", unreal.EditorLevelLibrary.get_level_viewport_camera_info())
except Exception as exc:
    print("CAMERA_INFO_ERROR", repr(exc))
