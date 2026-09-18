"""Test a SceneCapture2D + persistent render target as a viewport-independent capture path."""
import os, glob, json
from pathlib import Path
import unreal

ROOT = Path("C:/Git/AuraProj/Saved/RawModelImport/V5/GuidemenRebuild")
RT_DIR = "/Game/Assets/Environment/GuangzhouLandmarks/V5/GuidemenRebuild"
print("READ_API", [n for n in dir(unreal.RenderingLibrary) if "render_target" in n.lower()])

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
center = unreal.Vector(100000.0, 100000.0, 0.0)
bp = unreal.EditorAssetLibrary.load_asset("/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K")
building = actors.spawn_actor_from_class(bp.generated_class(), center)
origin, extent = building.get_actor_bounds(False)
print("BOUNDS", [round(v,1) for v in (origin.x, origin.y, origin.z)], [round(v,1) for v in (extent.x, extent.y, extent.z)])

# persistent render target asset so it has a real resource
unreal.EditorAssetLibrary.make_directory(RT_DIR)
rt_path = RT_DIR + "/RT_GuidemenCapture"
if unreal.EditorAssetLibrary.does_asset_exist(rt_path):
    unreal.EditorAssetLibrary.delete_asset(rt_path)
factory = unreal.TextureRenderTargetFactoryNew()
rt = unreal.AssetToolsHelpers.get_asset_tools().create_asset("RT_GuidemenCapture", RT_DIR, unreal.TextureRenderTarget2D, factory)
rt.set_editor_property("size_x", 1600)
rt.set_editor_property("size_y", 1000)
rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
print("RT_CREATED", rt.get_path_name() if rt else None)

target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * 0.5)
radius = max(extent.x, extent.y, extent.z)
camera = target + unreal.Vector(1.7*radius, 1.7*radius, 0.92*radius)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
cap = actors.spawn_actor_from_class(unreal.SceneCapture2D, camera)
cap.set_actor_location_and_rotation(camera, rotation, False, False)
comp = cap.capture_component2d
comp.set_editor_property("texture_target", rt)
comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
comp.set_editor_property("capture_every_frame", True)
comp.set_editor_property("fov_angle", 90.0)
comp.set_editor_property("post_process_blend_weight", 1.0)
comp.capture_scene()
print("CAPTURE_DONE")

before = set(glob.glob("C:/Git/AuraProj/Saved/**/*.png", recursive=True))
unreal.RenderingLibrary.export_render_target(world, rt, str(ROOT), "Guidemen_rt_test")
after = set(glob.glob("C:/Git/AuraProj/Saved/**/*.png", recursive=True))
print("NEW_PNGS", [p for p in sorted(after - before)])
print("EXPECTED_EXISTS", os.path.exists(str(ROOT / "Guidemen_rt_test.png")))
for cand in (ROOT/"Guidemen_rt_test.png", Path("C:/Git/AuraProj/Saved/Guidemen_rt_test.png"),
             Path("C:/Git/AuraProj/Guidemen_rt_test.png"), Path("C:/Git/AuraProj/Saved/Screenshots/Guidemen_rt_test.png")):
    if cand.exists():
        print("FOUND", cand, cand.stat().st_size)

actors.destroy_actor(cap)
actors.destroy_actor(building)
print("CLEANUP_DONE")
