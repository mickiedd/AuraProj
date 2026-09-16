"""Capture the exact V2 actor without loading/saving the user's dirty map.

Temporary actors are isolated 1km above the current scene and destroyed on a
Slate tick after GPU export. No existing actor/light or map is saved/changed.
Set ZNM_CAPTURE_STAGE / ZNM_CAPTURE_VIEW via environment before remote_run.
"""
import json
import os
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[1]/"Saved/RawModelImport/ZhengnanmenManual4K"
BP="/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"
STAGE=globals().get("CAPTURE_STAGE","before")
VIEW=globals().get("CAPTURE_VIEW","hero")
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_editor_world()
spawned=[]
resident_textures={}
try:
    building=actors.spawn_actor_from_object(unreal.EditorAssetLibrary.load_asset(BP),unreal.Vector(0,0,100000))
    assert building, BP
    spawned.append(building);building.set_actor_label("ZNM_Manual4K_TransientValidation")
    # SceneCapture cameras do not reliably contribute a texture-streaming view.
    # Temporarily request the actor's complete mip chains instead of capturing
    # a blurred low-mip plaque/surface or disabling streaming for the whole map.
    for mc in building.get_components_by_class(unreal.StaticMeshComponent):
        for i in range(mc.get_num_materials()):
            mat=mc.get_material(i)
            if mat:
                for tex in unreal.MaterialEditingLibrary.get_used_textures(mat):
                    if tex.get_path_name() not in resident_textures:
                        tex.set_force_mip_levels_to_be_resident(30.)
                        resident_textures[tex.get_path_name()]=tex
    origin,extent=building.get_actor_bounds(False)
    w=max(extent.x*2,extent.y*2);h=extent.z*2
    target=origin+unreal.Vector(0,0,h*.02)
    offset=unreal.Vector(w*.94,w*1.8,h*.50)
    if VIEW=="front":offset=unreal.Vector(0,w*1.7,h*.1)
    elif VIEW=="rear":offset=unreal.Vector(w*.35,-w*1.65,h*.2)
    elif VIEW=="roof":
        target=origin+unreal.Vector(0,0,h*.24);offset=unreal.Vector(w*.2,w*.85,h*.35)
    elif VIEW=="stone":
        target=origin+unreal.Vector(0,w*.1,-h*.27);offset=unreal.Vector(w*.18,w*.72,h*.03)
    capture=actors.spawn_actor_from_class(unreal.SceneCapture2D,target+offset)
    spawned.append(capture)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(target+offset,target),False)
    c=capture.capture_component2d
    c.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property("capture_every_frame",False);c.set_editor_property("capture_on_movement",False)
    c.set_editor_property("always_persist_rendering_state",True)
    c.set_editor_property("fov_angle",48.)
    c.set_editor_property("primitive_render_mode",unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    c.show_only_actor_components(building)
    rt=unreal.RenderingLibrary.create_render_target2d(world,2560,1600,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property("texture_target",rt)
    pp=c.get_editor_property("post_process_settings")
    pp.set_editor_property("override_auto_exposure_min_brightness",True)
    pp.set_editor_property("override_auto_exposure_max_brightness",True)
    pp.set_editor_property("auto_exposure_min_brightness",8.)
    pp.set_editor_property("auto_exposure_max_brightness",8.)
    pp.set_editor_property("override_auto_exposure_bias",True)
    pp.set_editor_property("auto_exposure_bias",0.)
    pp.set_editor_property("override_auto_exposure_method",True)
    pp.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
    pp.set_editor_property("auto_exposure_apply_physical_camera_exposure",True)
    pp.set_editor_property("override_camera_iso",True);pp.set_editor_property("camera_iso",100.)
    pp.set_editor_property("override_camera_shutter_speed",True);pp.set_editor_property("camera_shutter_speed",1.)
    pp.set_editor_property("override_depth_of_field_fstop",True);pp.set_editor_property("depth_of_field_fstop",2.)
    pp.set_editor_property("override_bloom_intensity",True);pp.set_editor_property("bloom_intensity",0.)
    pp.set_editor_property("override_lens_flare_intensity",True);pp.set_editor_property("lens_flare_intensity",0.)
    c.set_editor_property("post_process_blend_weight",1.)
    c.set_editor_property("post_process_settings",pp)
    # Local lights have finite radius and cannot reach the user's ground scene.
    for offset,power in ((unreal.Vector(w*.5,w,h*.8),1500000.),(unreal.Vector(-w*.8,w*.4,h*.5),500000.)):
        light=actors.spawn_actor_from_class(unreal.PointLight,origin+offset);spawned.append(light)
        lc=light.get_component_by_class(unreal.PointLightComponent)
        lc.set_editor_property("intensity_units",unreal.LightUnits.LUMENS)
        lc.set_intensity(power);lc.set_editor_property("attenuation_radius",w*6)
        lc.set_editor_property("source_radius",w*.22)
    ROOT.mkdir(parents=True,exist_ok=True)
    state={"ticks":0,"handle":None,"exported":False}
    def tick(delta):
        state["ticks"]+=1
        try:
            # Real editor frames, not a main-thread sleep; allow HISM/material updates.
            if state["ticks"]==80:c.capture_scene()
            if state["ticks"]==100:
                unreal.RenderingLibrary.export_render_target(world,rt,str(ROOT),STAGE+"-"+VIEW+".png")
                state["exported"]=True
            if state["ticks"]>=110:
                for tex in resident_textures.values():tex.set_force_mip_levels_to_be_resident(0.)
                for a in reversed(spawned):actors.destroy_actor(a)
                unreal.unregister_slate_post_tick_callback(state["handle"])
                (ROOT/(STAGE+"-"+VIEW+"-capture.json")).write_text(json.dumps({"stage":STAGE,"view":VIEW,"blueprint":BP,"resolution":[2560,1600],"bounds_extent":[extent.x,extent.y,extent.z],"exported":state["exported"],"temporary_actors_destroyed":True,"full_mip_requests_released":True,"texture_count_requested":len(resident_textures),"map_saved":False},indent=2))
        except Exception as exc:
            for tex in resident_textures.values():tex.set_force_mip_levels_to_be_resident(0.)
            for a in reversed(spawned):
                try:actors.destroy_actor(a)
                except Exception:pass
            unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log_error("ZNM_CAPTURE_FAILED "+str(exc))
    state["handle"]=unreal.register_slate_post_tick_callback(tick)
    print("ZNM_CAPTURE_QUEUED",STAGE,VIEW,"bounds",origin,extent)
except Exception:
    for tex in resident_textures.values():tex.set_force_mip_levels_to_be_resident(0.)
    for a in reversed(spawned):actors.destroy_actor(a)
    raise
