"""Deterministic one-frame-per-run UltraAAA capture harness.

The request is read from Saved/RawModelImport/V4/UltraAAA/capture_request.json.
The harness adds transient sky, fog, warm ground/quay and water actors so the
gate is judged against a real backdrop rather than the old black horizon.
"""
from __future__ import annotations

import json
import time
from pathlib import Path

import unreal


PROJECT = Path("C:/Git/AuraProj")
ROOT = PROJECT / "Saved/RawModelImport/V4/UltraAAA"
REQUEST = ROOT / "capture_request.json"

LEVELS = {
    "Wuxianmen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/L_Wuxianmen_V4_Preview",
    "Zhengximen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/L_Zhengximen_V4_Preview",
    "Dadongmen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/L_Dadongmen_V4_Preview",
    "Guidemen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/L_Guidemen_V4_Preview",
    "Zhengnanmen": "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/GreatSouthGate_Zhengnanmen_HighFidelity_Preview",
}
LABELS = {
    "Wuxianmen": "Preview_Wuxianmen_V4",
    "Zhengximen": "Preview_Zhengximen_V4",
    "Dadongmen": "Preview_Dadongmen_V4",
    "Guidemen": "Preview_Guidemen_V4",
    "Zhengnanmen": "GuangzhouLandmark_GreatSouthGate_Zhengnanmen_V2",
}
FRAMES = {
    "Wuxianmen": (8200.0, 1500.0, 2800.0),
    "Zhengximen": (5600.0, 1600.0, 2700.0),
    "Dadongmen": (4200.0, 2500.0, 3500.0),
    "Guidemen": (4600.0, 1900.0, 3500.0),
    "Zhengnanmen": (3100.0, 2300.0, 2850.0),
}


def find_or_spawn_building(gate, actors):
    label = LABELS[gate]
    building = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
    if building:
        return building
    bp_paths = {
        "Wuxianmen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/BP_Wuxianmen_V4",
        "Zhengximen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/BP_Zhengximen_V4",
        "Dadongmen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4",
        "Guidemen": "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/BP_Guidemen_V4",
        "Zhengnanmen": "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
    }
    bp = unreal.EditorAssetLibrary.load_asset(bp_paths[gate])
    assert bp and bp.generated_class(), bp_paths[gate]
    building = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0.0, 0.0, 0.0))
    assert building, gate
    building.set_actor_label(label)
    return building


def transient_actor(actors, cls, label, location, rotation=None):
    old = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
    if old:
        actors.destroy_actor(old)
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(*(rotation or (0.0, 0.0, 0.0))))
    assert actor, label
    actor.set_actor_label(label)
    return actor


def simple_material(path, base, roughness=0.92, metallic=0.0):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    destination, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    c = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, -100)
    c.set_editor_property("constant", unreal.LinearColor(*base, 1.0))
    lib.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 120)
    r.set_editor_property("r", roughness)
    lib.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 340)
    m.set_editor_property("r", metallic)
    lib.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def set_component_material(actor, material):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        component.set_material(0, material)


def setup_backdrop(gate, actors):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    ground_mat = simple_material("/Game/Assets/Environment/GuangzhouLandmarks/UltraAAA/M_UltraAAA_CaptureGround", (0.19, 0.145, 0.10), 0.96)
    water_mat = simple_material("/Game/Assets/Environment/GuangzhouLandmarks/UltraAAA/M_UltraAAA_CaptureWater", (0.035, 0.12, 0.14), 0.26)
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    assert plane
    ground = transient_actor(actors, unreal.StaticMeshActor, "UltraAAA_Capture_Ground", (0.0, 0.0, -12.0))
    ground_component = ground.get_component_by_class(unreal.StaticMeshComponent)
    ground_component.set_static_mesh(plane)
    ground_component.set_editor_property("relative_scale3d", unreal.Vector(160.0, 160.0, 1.0))
    ground_component.set_material(0, ground_mat)
    if gate == "Wuxianmen" or gate == "Dadongmen":
        water = transient_actor(actors, unreal.StaticMeshActor, "UltraAAA_Capture_Water", (0.0, -2600.0, -8.0))
        water_component = water.get_component_by_class(unreal.StaticMeshComponent)
        water_component.set_static_mesh(plane)
        water_component.set_editor_property("relative_scale3d", unreal.Vector(95.0, 26.0, 1.0))
        water_component.set_material(0, water_mat)
    light = transient_actor(actors, unreal.DirectionalLight, "UltraAAA_Capture_Key", (0.0, 0.0, 4200.0), (-38.0, -38.0, 0.0))
    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    light_component.set_intensity(7.0)
    light_component.set_light_color(unreal.LinearColor(1.0, 0.89, 0.73, 1.0))
    sky = transient_actor(actors, unreal.SkyLight, "UltraAAA_Capture_Sky", (0.0, 0.0, 2200.0))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_intensity(1.25)
    sky_component.recapture_sky()
    try:
        atmosphere = transient_actor(actors, unreal.SkyAtmosphere, "UltraAAA_Capture_Atmosphere", (0.0, 0.0, 0.0))
        atmosphere.get_component_by_class(unreal.SkyAtmosphereComponent)
    except Exception:
        pass
    try:
        fog = transient_actor(actors, unreal.ExponentialHeightFog, "UltraAAA_Capture_Fog", (0.0, 0.0, 0.0))
        fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        fog_component.set_editor_property("fog_density", 0.004)
        fog_component.set_editor_property("fog_height_falloff", 0.18)
    except Exception:
        pass
    return light


def add_tiling_panel(gate, actors):
    plane = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    assert plane
    if gate == "Wuxianmen":
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/Materials/M_WeatheredStone_UltraAAA"
    elif gate == "Zhengximen":
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/Materials/M_GrayBrick_UltraAAA"
    elif gate == "Dadongmen":
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/Materials/M_Dadongmen_Stone_UltraAAA"
    elif gate == "Guidemen":
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/Materials/M_Stone_BlueGrey_UltraAAA"
    else:
        material_path = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Materials/M_Stone_Aged_UltraAAA"
    material = unreal.EditorAssetLibrary.load_asset(material_path)
    assert material, material_path
    for row in range(3):
        for col in range(3):
            panel = transient_actor(actors, unreal.StaticMeshActor, "UltraAAA_TilePanel_%d_%d" % (row, col), ((col - 1) * 1550.0, 800.0, 1450.0))
            comp = panel.get_component_by_class(unreal.StaticMeshComponent)
            comp.set_static_mesh(plane)
            comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
            comp.set_editor_property("relative_scale3d", unreal.Vector(15.0, 10.0, 1.0))
            comp.set_material(0, material)


def capture(req):
    gate, view, iteration = req["gate"], req["view"], int(req["iteration"])
    assert gate in LEVELS and view in ("front", "door_close", "roof_close", "rear", "raking", "tiling", "wireframe", "normals"), req
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level_editor.load_level(LEVELS[gate]), LEVELS[gate]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    building = find_or_spawn_building(gate, actors)
    key = setup_backdrop(gate, actors)
    if view == "tiling":
        add_tiling_panel(gate, actors)
    if view == "raking":
        key.set_actor_rotation(unreal.Rotator(pitch=-14.0, yaw=-55.0, roll=0.0), False)
    width, depth, height = FRAMES[gate]
    origin = unreal.Vector(0.0, 0.0, 0.0)
    target = unreal.Vector(0.0, 0.0, height * (0.42 if view not in ("door_close", "roof_close") else (0.30 if view == "door_close" else 0.72)))
    sign = 1.0 if view == "rear" else -1.0
    if view in ("front", "rear", "raking", "wireframe", "normals"):
        camera = target + unreal.Vector(width * (0.16 if view == "raking" else 0.10), sign * depth * 1.65, height * 0.10)
    elif view == "door_close":
        camera = target + unreal.Vector(width * 0.08, sign * depth * 0.55, height * 0.02)
    elif view == "roof_close":
        camera = target + unreal.Vector(width * 0.11, sign * depth * 0.62, height * 0.20)
    else:
        camera = unreal.Vector(0.0, -5200.0, 1550.0)
        target = unreal.Vector(0.0, 800.0, 1450.0)
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10", "r.Nanite 1", "r.Lumen.Reflections 1", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), command)
    mode = "lit"
    if view == "wireframe": mode = "wireframe"
    if view == "normals": mode = "WorldNormal"
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "viewmode " + mode)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output_dir = ROOT / "Captures" / gate / ("Iteration_%02d" % iteration)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / (gate + "_UltraAAA_Iteration%02d_%s.png" % (iteration, view))
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, "UltraAAA %s iteration %d %s" % (gate, iteration, view), 1.0, True)
    time.sleep(5.0)
    print("GUANGZHOU_ULTRA_AAA_CAPTURE", json.dumps({"gate": gate, "view": view, "iteration": iteration, "output": str(output), "camera": [camera.x, camera.y, camera.z], "target": [target.x, target.y, target.z], "backdrop": "transient sky + fog + warm ground/quay"}))


capture(json.loads(REQUEST.read_text(encoding="utf-8")))
