"""Create or rebuild the Guangzhou landmark showcase level.

Gathers every placeable landmark from /Game/Assets/Environment/GuangzhouLandmarks
- the seven reference-tuned Blueprints plus BP_GreatNorthGate and
BP_ZhenhaiTower created by WrapLandmarkMeshBlueprints.py - and arranges them in a
ring around a central plaza, each turned to face the plaza centre.

TWO TRAPS THIS SCRIPT EXISTS TO AVOID, both found the hard way:

1. unreal.Rotator's positional argument order is (roll, pitch, yaw), NOT
   (pitch, yaw, roll). `unreal.Rotator(0.0, yaw, 0.0)` therefore sets PITCH and
   tips the landmark over instead of turning it. That is what made an earlier
   run of this script produce a level of landmarks lying on their sides, with
   get_actor_bounds reporting Guidemen as 5730 cm tall. Always construct
   rotators with keyword arguments here.

2. Component transforms cannot be assumed to be identity. The V3 Blueprints
   carry ReferenceInfill_* components (interior infill that blocks sight lines
   through the gate openings) at non-zero relative locations, and hidden UCX
   collision components. Grounding and spacing are therefore computed from the
   actual instanced geometry, with each component's relative transform and each
   instance transform folded in, and with non-rendering components excluded -
   collision volumes must not define a landmark's visible floor or footprint.

Layout radius is derived from the landmarks' own geometry, so swapping in a
larger or smaller variant keeps the ring valid.

Each landmark also gets ONE INDEPENDENT SPOT LIGHT of its own, standing in front
of it on the plaza side and aimed at the middle of the building, so a single
building's illumination can be tuned without touching any other. The shared key,
fill and sky light are deliberately left exactly as validated: the per-building
light is an accent layered on top of them, not a replacement for them. Light
position, cone and attenuation are all derived from the building's measured
geometry, so a landmark swapped for a larger or smaller variant still gets a
correctly sized and correctly aimed light, and the cone is narrow enough that it
does not wash the neighbouring gates.

Re-runnable: an existing showcase level is loaded and its showcase-tagged actors
are removed before the ring is rebuilt.
"""

import json
import math
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"
LEVEL_PATH = ROOT + "/L_GuangzhouLandmarkShowcase"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-showcase.json")

GROUND_MESH = "/Engine/BasicShapes/Plane"
# The project's preview grounds (M_V5PreviewGround, WorldGridMaterial) all render
# near-white, which crushes the landmarks - dark weathered stone and timber -
# into near-black silhouettes. M_ShowcaseGround is a plain dark paving material
# authored for this level by CreateShowcaseGroundMaterial.py.
GROUND_MATERIAL = ROOT + "/M_ShowcaseGround"
# Plane is 100 cm square, so the scale is the extent in metres. Raised from 420 m on
# 2026-09-22: at 420 m the plane's edge sits only 160 m from the plaza, so an
# eye-level camera looks straight over it and the band between the edge and the
# landmarks is the SkyAtmosphere seen BELOW the horizon (found by ray trace in
# Scripts/ProbeShowcaseFrameGeometry.py - row 510 of the Guidemen frame hits no
# geometry at all). The ground and that atmosphere band are both blue, so the edge
# is invisible and the blue merely reads as continuous; the hero view shows it
# plainly as the edge of a floating slab.
#
# The size is set by two measurements, not by taste. The band is
# atan(camera_height / half_extent) tall, and at 900 px over a 36 deg vertical FOV
# that is 25 px per degree, so:
#
#   half extent   band height (eye level)   hero camera (420 m out, 260 m up)
#     210 m            1.09 deg / 27 px       edge in frame, slab reads as a slab
#    1000 m            0.23 deg /  6 px       edge in frame at ~row 640
#    4000 m            0.06 deg /  1 px       edge at 4.2 deg, out of frame
#
# 8000 m (half extent 4000 m) is the first size that removes the band for practical
# purposes AND pushes the edge out of the hero frame, so the ring reads as sitting
# on open ground. The plane is a single quad with a constant material, so the size
# costs nothing.
GROUND_SCALE = 8000.0
GROUND_TOP_Z = 0.0
GROUND_HALF_EXTENT = GROUND_SCALE * 100.0 / 2.0

GAP_CM = 3000.0
MIN_RADIUS_CM = 8000.0
LABEL_HEIGHT_CM = 400.0
LABEL_WORLD_SIZE = 150.0

# Pinned exposure, chosen by bracketing and looking. See spawn_post_process.
EXPOSURE_EV100 = 3.0

SHOWCASE_TAG = "GuangzhouLandmarkShowcase"
PARK_TAG = "GuangzhouLandmarkPark"
IMPORT_TAG = "ImportedGuangzhouLandmark"
LIGHT_TAG = "GuangzhouLandmarkLight"

# ---- per-building lights ----------------------------------------------------
# One spot light per landmark, owned by that landmark. Everything below is
# expressed relative to the building's own measured geometry rather than in
# absolute centimetres, so the recipe survives a landmark being swapped for a
# larger or smaller variant.
#
# The accent is deliberately modest. The level pins exposure (EV100 3.0), and the
# earlier diagnosis on this level established that it was already clipping pale
# stone at that exposure, so the accent is set to add definition to a facade
# without lifting the whole ring into the clip. LIGHT_ACCENT_LUX is the
# illuminance the light contributes at the building, on the same scale as the
# global key (7.0) and fill (2.0) - it sits between the two on purpose.
LIGHT_FACADE_CLEARANCE_CM = 1500.0   # stands this far in front of the facade
LIGHT_HEIGHT_FACTOR = 1.35           # x building height, so it clears the roof
LIGHT_AIM_HEIGHT_FACTOR = 0.45       # cone is centred on this height of the building
LIGHT_CONE_MARGIN_DEG = 10.0         # added to the angle the building subtends
LIGHT_MAX_CONE_DEG = 80.0
LIGHT_INNER_CONE_FRACTION = 0.45
LIGHT_ACCENT_LUX = 3.0               # illuminance contributed at the building
LIGHT_ATTENUATION_MARGIN = 1.15      # x (distance + building radius)
LIGHT_SOURCE_RADIUS_CM = 60.0        # softens the shadow edges
LIGHT_TEMPERATURE_K = 5200.0         # mildly warm, against the neutral global key
LIGHT_FOLDER = "LandmarkLights"

# The imported gate models present their front on local -Y, which is the
# convention the existing Scifi Desert landmark placement relies on. A landmark
# at ring angle theta therefore takes yaw = theta - 90. FACING_OFFSET_DEGREES is
# the per-landmark correction for any model that does not follow it.
LANDMARKS = [
    ("Zhengnanmen_HighFidelity",
     "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
     "Zhengnanmen (Great South Gate) - HighFidelity", 0.0),
    ("Zhengnanmen_AAA_V3", "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
     "Zhengnanmen (Great South Gate) - AAA V3", 0.0),
    ("Xiaobeimen_AAA_V3", "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
     "Xiaobeimen (Small North Gate) - AAA V3", 0.0),
    ("Xiaobeimen_Production_V3", "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3",
     "Xiaobeimen (Small North Gate) - Production V3", 0.0),
    # The Guidemen slot points at BP_Guidemen_V5_4K_PreRebuild_20260918. Despite
    # the name, that is the REPAIRED model: the 2026-09-21 window/roof repair and
    # the 2026-09-22 arch/door/plaque follow-up were both applied to it, while
    # the original BP_Guidemen_V5_4K was left untouched and has since been
    # deleted. "PreRebuild" is a leftover from when it was only a backup taken
    # before the rebuild, so do not read it as "stale".
    ("Guidemen_ReferenceRepaired",
     "V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918",
     "Guidemen (Guide Gate) - V5 4K reference-repaired", 0.0),
    ("Wuxianmen_V5_4K_Core", "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
     "Wuxianmen (Five Immortals Gate) - V5 4K Core", 0.0),
    ("Wuxianmen_V5_FullPBR", "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
     "Wuxianmen (Five Immortals Gate) - V5 FullPBR", 0.0),
    ("GreatNorthGate", "GreatNorthGate/BP_GreatNorthGate",
     "GreatNorthGate (Dabeimen)", 0.0),
    ("ZhenhaiTower", "ZhenhaiTower/BP_ZhenhaiTower",
     "Zhenhai Tower", 0.0),
]


def yaw_rotator(yaw_degrees):
    """Keyword construction - see trap 1 in the module docstring."""
    return unreal.Rotator(pitch=0.0, yaw=yaw_degrees, roll=0.0)


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def set_light_property(component, attribute, value):
    """set_editor_property, reporting rather than swallowing a failure.

    Returns whether the write took. A light that silently kept its default and
    one that was configured correctly are indistinguishable in a screenshot, so
    the caller records the readback and asserts on it.
    """
    try:
        component.set_editor_property(attribute, value)
        return True
    except Exception as exc:
        print("SHOWCASE_LIGHT_PROPERTY_FAILED", attribute, repr(exc))
        return False


def read_light_property(component, attribute):
    for reader in (lambda: component.get_editor_property(attribute),
                   lambda: getattr(component, attribute)):
        try:
            return reader()
        except Exception:
            pass
    return None


def set_light_unit(component):
    for name in ("CANDELAS", "CANDELA"):
        unit = getattr(unreal.LightUnits, name, None)
        if unit is not None and set_light_property(component, "intensity_units", unit):
            return unit
    return None


def mesh_corners(mesh):
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    xs = (float(origin.x) - float(extent.x), float(origin.x) + float(extent.x))
    ys = (float(origin.y) - float(extent.y), float(origin.y) + float(extent.y))
    zs = (float(origin.z) - float(extent.z), float(origin.z) + float(extent.z))
    return [(x, y, z) for x in xs for y in ys for z in zs]


def component_relative_transform(component):
    """The component's transform relative to its parent (the actor root here)."""
    for attribute in ("relative_transform",):
        try:
            return component.get_editor_property(attribute)
        except Exception:
            pass
    for method in ("get_relative_transform", "k2_get_relative_transform"):
        if hasattr(component, method):
            try:
                return getattr(component, method)()
            except Exception:
                pass
    return unreal.Transform()


def local_geometry_bounds(actor):
    """Union of all rendered instanced geometry in actor-local space.

    Returns (min_x, min_y, min_z, max_x, max_y, max_z, radius_xy) where radius_xy
    is the largest XY distance from the actor origin, which is invariant under
    yaw and therefore the correct figure for spacing the ring.

    Components that do not render are skipped: the UCX collision volumes would
    otherwise define the landmark's floor and footprint.
    """
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    radius = 0.0
    component_count = 0
    instance_count = 0
    skipped = []
    corner_cache = {}

    # HierarchicalInstancedStaticMeshComponent derives from StaticMeshComponent,
    # so this single query already returns both kinds. Querying both would
    # double-count every HISM.
    components = list(actor.get_components_by_class(unreal.StaticMeshComponent))

    for component in components:
        mesh = component.static_mesh
        if mesh is None:
            continue
        is_hism = component.get_class().get_name().startswith("Hierarchical")
        if is_hism and int(component.get_instance_count()) == 0:
            continue

        try:
            visible = bool(component.get_editor_property("visible"))
        except Exception:
            visible = True
        if not visible:
            skipped.append(component.get_name())
            continue

        component_count += 1
        component_transform = component_relative_transform(component)

        key = mesh.get_path_name()
        if key not in corner_cache:
            corner_cache[key] = mesh_corners(mesh)
        corners = corner_cache[key]

        if is_hism:
            transforms = [component.get_instance_transform(index)
                          for index in range(int(component.get_instance_count()))]
        else:
            transforms = [unreal.Transform()]
        instance_count += len(transforms)

        for instance_transform in transforms:
            for (cx, cy, cz) in corners:
                local = instance_transform.transform_location(unreal.Vector(cx, cy, cz))
                point = component_transform.transform_location(local)
                lo[0] = min(lo[0], point.x); hi[0] = max(hi[0], point.x)
                lo[1] = min(lo[1], point.y); hi[1] = max(hi[1], point.y)
                lo[2] = min(lo[2], point.z); hi[2] = max(hi[2], point.z)
                radius = max(radius, math.hypot(point.x, point.y))

    assert component_count, "no rendered geometry on " + actor.get_actor_label()
    return {
        "min": lo, "max": hi, "radius_xy": radius,
        "component_count": component_count, "instance_count": instance_count,
        "skipped_components": skipped,
    }


def bounds_of(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [float(origin.x), float(origin.y), float(origin.z),
            float(extent.x), float(extent.y), float(extent.z)]


def ensure_level():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # The showcase level is often already the open level when this is re-run from
    # a live editor, and asking the level editor to load the level that is
    # already current is not guaranteed to report success. Detect that case and
    # leave the level alone; the clear-and-rebuild below is what makes the run
    # idempotent, not the load.
    if world and world.get_path_name().startswith(LEVEL_PATH):
        return world
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        assert level_editor.load_level(LEVEL_PATH), LEVEL_PATH
    else:
        assert level_editor.new_level(LEVEL_PATH), "new_level failed: " + LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL_PATH), world
    return world


def clear_previous():
    """Remove everything this script previously built.

    Each landmark now owns a light, and destroying a parent destroys its
    attached children. The snapshot taken here therefore contains actors that
    may already be gone by the time the loop reaches them, so every per-actor
    step is guarded; the assertion at the end is what proves the level really is
    clear, rather than the count.
    """
    removed = 0
    for actor in list(actor_subsystem().get_all_level_actors()):
        try:
            if SHOWCASE_TAG not in [str(tag) for tag in actor.tags]:
                continue
            if actor_subsystem().destroy_actor(actor):
                removed += 1
        except Exception as exc:
            print("SHOWCASE_CLEAR_SKIPPED", repr(exc))
    left = [actor.get_actor_label()
            for actor in actor_subsystem().get_all_level_actors()
            if SHOWCASE_TAG in [str(tag) for tag in actor.tags]]
    assert not left, "showcase actors survived the clear: " + ", ".join(left)
    return removed


def spawn_ground():
    mesh = unreal.EditorAssetLibrary.load_asset(GROUND_MESH)
    assert mesh, GROUND_MESH
    actor = actor_subsystem().spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(), unreal.Vector(0, 0, GROUND_TOP_Z),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert actor, "ground plane"
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(GROUND_SCALE, GROUND_SCALE, 1.0))
    material = unreal.EditorAssetLibrary.load_asset(GROUND_MATERIAL)
    if material:
        component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.set_actor_label("Showcase_Ground")
    actor.tags = [SHOWCASE_TAG]
    return actor


def spawn_directional(label, intensity, rotation, cast_shadows):
    actor = actor_subsystem().spawn_actor_from_class(
        unreal.DirectionalLight.static_class(), unreal.Vector(0, 0, 20000),
        rotation, transient=False)
    assert actor, label
    component = actor.get_component_by_class(unreal.LightComponent)
    if component:
        try:
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception:
            pass
        component.set_intensity(intensity)
        for attribute, value in (("atmosphere_sun_light", True),
                                 ("cast_shadows", cast_shadows)):
            try:
                component.set_editor_property(attribute, value)
            except Exception:
                pass
    actor.set_actor_label(label)
    actor.tags = [SHOWCASE_TAG]
    return actor


def spawn_post_process():
    """An unbound post-process volume with a pinned exposure.

    Auto-exposure cannot be relied on: a single capture_scene() call does not give
    it time to converge, so the same level renders differently from one capture to
    the next and cannot be reviewed. The project's preview levels solve this by
    overriding both ends of the auto-exposure range, but they pin EV100 = 1.0,
    which is far too bright for this scene - it clips the pale stone to pure white
    and the sky to a flat wash.

    EV100 = 3.0 was chosen by bracketing (2.0 / 3.0 / 4.0 / 5.0) and looking: at
    3.0 the gate is legible but the stone still clips, and at 4.0 the frame goes dark.

    Note that hiding a light actor does NOT disable the light, so a "hide the
    lights" test cannot be used to prove a scene is unlit - it will look identical
    either way.
    """
    try:
        actor = actor_subsystem().spawn_actor_from_class(
            unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    except Exception as exc:
        print("SHOWCASE_POSTPROCESS_SKIPPED", exc)
        return None
    if actor is None:
        return None
    actor.set_editor_property("unbound", True)
    try:
        settings = actor.get_editor_property("settings")
        for attribute, value in (("override_auto_exposure_min_brightness", True),
                                 ("auto_exposure_min_brightness", EXPOSURE_EV100),
                                 ("override_auto_exposure_max_brightness", True),
                                 ("auto_exposure_max_brightness", EXPOSURE_EV100)):
            settings.set_editor_property(attribute, value)
        actor.set_editor_property("settings", settings)
    except Exception as exc:
        print("SHOWCASE_POSTPROCESS_EXPOSURE_SKIPPED", exc)
    actor.set_actor_label("Showcase_PostProcess")
    actor.tags = [SHOWCASE_TAG]
    return actor


def spawn_lighting():
    """Matches the lighting the project's landmark preview levels already use:
    a 7.0 key, a 2.0 unshadowed fill, a 1.4 sky light with a black lower
    hemisphere, sky atmosphere, and pinned exposure."""
    spawn_directional("Showcase_Key", 7.0,
                      unreal.Rotator(pitch=-48.0, yaw=35.0, roll=0.0), True)
    spawn_directional("Showcase_Fill", 2.0,
                      unreal.Rotator(pitch=-20.0, yaw=-140.0, roll=0.0), False)

    sky = actor_subsystem().spawn_actor_from_class(
        unreal.SkyLight.static_class(), unreal.Vector(0, 0, 12000),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    if sky:
        sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
        if sky_component:
            try:
                sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            except Exception:
                pass
            sky_component.set_intensity(1.4)
            for attribute, value in (("real_time_capture", True),
                                     ("lower_hemisphere_is_black", True)):
                try:
                    sky_component.set_editor_property(attribute, value)
                except Exception:
                    pass
            try:
                sky_component.recapture_sky()
            except Exception:
                pass
        sky.set_actor_label("Showcase_SkyLight")
        sky.tags = [SHOWCASE_TAG]

    try:
        atmosphere = actor_subsystem().spawn_actor_from_class(
            unreal.SkyAtmosphere.static_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
        if atmosphere:
            atmosphere.set_actor_label("Showcase_SkyAtmosphere")
            atmosphere.tags = [SHOWCASE_TAG]
    except Exception as exc:
        print("SHOWCASE_ATMOSPHERE_SKIPPED", exc)

    spawn_post_process()


def place_landmarks():
    entries = []
    for key, relative_path, display_name, facing_offset in LANDMARKS:
        blueprint_path = ROOT + "/" + relative_path
        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        assert isinstance(blueprint, unreal.Blueprint), blueprint_path
        actor = actor_subsystem().spawn_actor_from_class(
            blueprint.generated_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
        assert actor, blueprint_path
        actor.set_actor_label("Landmark_" + key)
        geometry = local_geometry_bounds(actor)
        entries.append({
            "key": key, "actor": actor, "blueprint": blueprint_path,
            "display_name": display_name, "facing_offset": facing_offset,
            "geometry": geometry,
        })

    # Ring radius: each landmark contributes its own bounding-circle diameter
    # plus a fixed gap of arc; the radius is that total arc over 2*pi.
    total_arc = sum(2.0 * item["geometry"]["radius_xy"] + GAP_CM for item in entries)
    radius = max(MIN_RADIUS_CM, total_arc / (2.0 * math.pi))

    arc = 0.0
    for item in entries:
        geometry = item["geometry"]
        arc += geometry["radius_xy"]
        theta = arc / radius
        yaw = math.degrees(theta) - 90.0 + item["facing_offset"]
        x = radius * math.cos(theta)
        y = radius * math.sin(theta)

        actor = item["actor"]
        # Grounding: yaw does not change Z, so world min Z is actor Z plus the
        # local minimum Z measured from the instanced geometry.
        z = GROUND_TOP_Z - geometry["min"][2]
        actor.set_actor_location(unreal.Vector(x, y, z), False, True)
        actor.set_actor_rotation(yaw_rotator(yaw), False)

        # Verify the rotation actually took, via the rotator the engine stores.
        applied = actor.get_actor_rotation()
        assert abs(float(applied.yaw) - (yaw % 360.0)) < 0.01 or \
            abs(abs(float(applied.yaw) - (yaw % 360.0)) - 360.0) < 0.01, \
            "{}: yaw did not apply (wanted {}, got {})".format(
                item["key"], yaw, float(applied.yaw))
        assert abs(float(applied.pitch)) < 0.01 and abs(float(applied.roll)) < 0.01, \
            "{}: landmark is tipped (pitch {}, roll {})".format(
                item["key"], float(applied.pitch), float(applied.roll))

        actor.tags = [SHOWCASE_TAG, PARK_TAG, IMPORT_TAG, "Landmark_" + item["key"]]
        try:
            actor.set_folder_path("Landmarks")
        except Exception:
            pass

        arc += geometry["radius_xy"] + GAP_CM
        item.update({
            "label": "Landmark_" + item["key"],
            "angle_deg": round(math.degrees(theta), 3),
            "yaw_deg": round(yaw, 3),
            "location": [round(x, 2), round(y, 2), round(z, 2)],
            "geometry_min_local": [round(value, 2) for value in geometry["min"]],
            "geometry_max_local": [round(value, 2) for value in geometry["max"]],
            "geometry_radius_xy": round(geometry["radius_xy"], 2),
            "height_cm": round(geometry["max"][2] - geometry["min"][2], 2),
            "component_count": geometry["component_count"],
            "instance_count": geometry["instance_count"],
            "skipped_non_rendering_components": len(geometry["skipped_components"]),
            "world_min_z": round(z + geometry["min"][2], 3),
            "actor_bounds_cm": [round(value, 2) for value in bounds_of(actor)],
        })
    return entries, radius


def landmark_light_recipe(actor_location, radius, height):
    """Everything about one building's light, derived from its own geometry.

    Returned as a dict so the spawn step, the isolation check and the manifest
    all use the same numbers instead of recomputing them and drifting apart.
    """
    x, y = float(actor_location.x), float(actor_location.y)
    planar = math.hypot(x, y)
    assert planar > 0.0, "landmark sits on the plaza centre"
    # The landmarks were turned to face the plaza centre, so "in front of the
    # facade" is the inward radial direction.
    front = (-x / planar, -y / planar)
    clearance = radius + LIGHT_FACADE_CLEARANCE_CM
    location = (x + front[0] * clearance, y + front[1] * clearance,
                GROUND_TOP_Z + height * LIGHT_HEIGHT_FACTOR)
    aim = (x, y, GROUND_TOP_Z + height * LIGHT_AIM_HEIGHT_FACTOR)

    dx, dy, dz = aim[0] - location[0], aim[1] - location[1], aim[2] - location[2]
    planar_distance = math.hypot(dx, dy)
    distance = math.sqrt(dx * dx + dy * dy + dz * dz)
    # Keyword construction - see trap 1 in the module docstring.
    rotation = unreal.Rotator(pitch=math.degrees(math.atan2(dz, planar_distance)),
                              yaw=math.degrees(math.atan2(dy, dx)), roll=0.0)

    # The building is treated as a sphere of its own XY radius centred on the aim
    # point. radius_xy is at least half the height on all nine landmarks, so that
    # sphere contains the building; asin gives the half-angle the building
    # subtends from the light, and the margin keeps the eaves and finials off the
    # cone edge.
    half_angle = math.degrees(math.asin(min(1.0, radius / distance)))
    outer_cone = min(LIGHT_MAX_CONE_DEG, half_angle + LIGHT_CONE_MARGIN_DEG)
    return {
        "location": location, "aim": aim, "rotation": rotation,
        "distance_cm": distance, "half_angle_deg": half_angle,
        "outer_cone_deg": outer_cone,
        "inner_cone_deg": outer_cone * LIGHT_INNER_CONE_FRACTION,
        "attenuation_radius_cm": (distance + radius) * LIGHT_ATTENUATION_MARGIN,
        # Intensity is in candelas, so the illuminance it produces at the
        # building is intensity / (distance in metres)^2. Solving that for the
        # target illuminance keeps the accent the same strength on every
        # building whatever its size, which absolute candelas would not.
        "intensity_cd": LIGHT_ACCENT_LUX * (distance / 100.0) ** 2,
    }


def spawn_landmark_lights(entries):
    """One independent, shadow-casting spot light per landmark.

    The light stands in front of its building - on the plaza side, which is the
    side the landmark faces - and above its roof, aimed at the middle of the
    building. Cone and attenuation are sized from the building's own geometry so
    the light reads as belonging to that building rather than washing its
    neighbours, and it is attached to the landmark actor so it moves with it.

    Returns one record per light, including the values read back off the
    component, because a light that silently kept its default is
    indistinguishable from a configured one in a screenshot.
    """
    records = []
    for entry in entries:
        actor = entry["actor"]
        recipe = landmark_light_recipe(actor.get_actor_location(),
                                       entry["geometry_radius_xy"],
                                       entry["height_cm"])
        light = actor_subsystem().spawn_actor_from_class(
            unreal.SpotLight.static_class(), unreal.Vector(*recipe["location"]),
            recipe["rotation"], transient=False)
        assert light, "landmark light for " + entry["key"]
        component = light.get_component_by_class(unreal.SpotLightComponent)
        assert component, "SpotLightComponent for " + entry["key"]

        try:
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception as exc:
            print("SHOWCASE_LIGHT_MOBILITY_FAILED", entry["key"], repr(exc))
        set_light_unit(component)
        for attribute, value in (("intensity", recipe["intensity_cd"]),
                                 ("attenuation_radius", recipe["attenuation_radius_cm"]),
                                 ("outer_cone_angle", recipe["outer_cone_deg"]),
                                 ("inner_cone_angle", recipe["inner_cone_deg"]),
                                 ("cast_shadows", True),
                                 ("source_radius", LIGHT_SOURCE_RADIUS_CM),
                                 ("use_temperature", True),
                                 ("temperature", LIGHT_TEMPERATURE_K),
                                 ("affects_world", True)):
            set_light_property(component, attribute, value)

        light.set_actor_label("Light_" + entry["key"])
        light.tags = [SHOWCASE_TAG, LIGHT_TAG, "Light_" + entry["key"]]
        try:
            light.set_folder_path(LIGHT_FOLDER)
        except Exception as exc:
            print("SHOWCASE_LIGHT_FOLDER_FAILED", entry["key"], repr(exc))

        # Ownership. KEEP_WORLD on all three rules: the light's world placement
        # is already computed, attachment must not move it.
        attached = False
        try:
            attached = bool(light.attach_to_actor(
                actor, "", unreal.AttachmentRule.KEEP_WORLD,
                unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                False))
        except Exception as exc:
            print("SHOWCASE_LIGHT_ATTACH_FAILED", entry["key"], repr(exc))
        assert attached, "{}: light is not owned by its building".format(entry["key"])

        world = light.get_actor_location()
        drift = math.dist((float(world.x), float(world.y), float(world.z)),
                          recipe["location"])
        assert drift < 0.01, \
            "{}: attachment moved the light {:.3f} cm".format(entry["key"], drift)

        records.append({
            "key": entry["key"], "label": light.get_actor_label(),
            "actor": light, "recipe": recipe,
            "attached_to": actor.get_actor_label() if attached else None,
            "location": [round(value, 2) for value in recipe["location"]],
            "aim": [round(value, 2) for value in recipe["aim"]],
            "distance_cm": round(recipe["distance_cm"], 2),
            "building_radius_cm": round(entry["geometry_radius_xy"], 2),
            "outer_cone_deg": round(recipe["outer_cone_deg"], 3),
            "inner_cone_deg": round(recipe["inner_cone_deg"], 3),
            "attenuation_radius_cm": round(recipe["attenuation_radius_cm"], 2),
            "intensity_cd": round(recipe["intensity_cd"], 2),
            "readback": {attribute: (bool(value) if isinstance(value, bool)
                                     else round(float(value), 4))
                         for attribute, value in (
                             ("intensity", read_light_property(component, "intensity")),
                             ("attenuation_radius",
                              read_light_property(component, "attenuation_radius")),
                             ("outer_cone_angle",
                              read_light_property(component, "outer_cone_angle")),
                             ("inner_cone_angle",
                              read_light_property(component, "inner_cone_angle")),
                             ("cast_shadows",
                              read_light_property(component, "cast_shadows")))
                         if value is not None},
        })

    # Isolation. The whole point of a per-building light is that it lights its own
    # building, so for each light record how far the nearest other landmark's
    # BODY - not just its centre - sits outside the cone. Measuring to the centre
    # alone would be too generous: a neighbouring gate 70 deg off the axis still
    # subtends about 20 deg from this light, so it is the centre angle minus that
    # span that decides whether the neighbour is lit. A negative margin means a
    # light is washing a neighbour and the cone needs narrowing.
    for index, record in enumerate(records):
        axis = record["recipe"]["aim"]
        origin = record["recipe"]["location"]
        axis = (axis[0] - origin[0], axis[1] - origin[1], axis[2] - origin[2])
        axis_length = math.sqrt(sum(component * component for component in axis))
        nearest = None
        for other_index, other in enumerate(records):
            if other_index == index:
                continue
            target = other["recipe"]["aim"]
            to_target = (target[0] - origin[0], target[1] - origin[1],
                         target[2] - origin[2])
            target_length = math.sqrt(sum(component * component for component in to_target))
            cosine = sum(a * b for a, b in zip(axis, to_target)) / (axis_length * target_length)
            angle = math.degrees(math.acos(max(-1.0, min(1.0, cosine))))
            span = math.degrees(math.asin(
                min(1.0, other["building_radius_cm"] / target_length)))
            margin = angle - span - record["outer_cone_deg"]
            if nearest is None or margin < nearest["margin_deg"]:
                nearest = {"neighbour": other["label"], "angle_deg": round(angle, 3),
                           "neighbour_span_deg": round(span, 3),
                           "margin_deg": round(margin, 3)}
        record["nearest_neighbour_outside_cone"] = nearest
    return records


def spawn_label(entry, ring_radius):
    try:
        actor = actor_subsystem().spawn_actor_from_class(
            unreal.TextRenderActor.static_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    except Exception as exc:
        print("SHOWCASE_TEXT_SKIPPED", entry["key"], exc)
        return None
    if not actor:
        return None
    theta = math.radians(entry["angle_deg"])
    inner = ring_radius - entry["geometry_radius_xy"] - 500.0
    actor.set_actor_location(
        unreal.Vector(inner * math.cos(theta), inner * math.sin(theta), LABEL_HEIGHT_CM),
        False, True)
    actor.set_actor_rotation(yaw_rotator(math.degrees(theta) - 90.0), False)
    actor.set_actor_label("Label_" + entry["key"])
    actor.tags = [SHOWCASE_TAG]
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    if component:
        try:
            component.set_text(unreal.Text(entry["display_name"]))
        except Exception:
            pass
        try:
            component.set_editor_property("world_size", LABEL_WORLD_SIZE)
        except Exception:
            pass
    return actor


def spawn_player_start():
    try:
        actor = actor_subsystem().spawn_actor_from_class(
            unreal.PlayerStart.static_class(), unreal.Vector(0, 0, 200),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    except Exception as exc:
        print("SHOWCASE_PLAYERSTART_SKIPPED", exc)
        return None
    if actor:
        actor.set_actor_label("Showcase_PlayerStart")
        actor.tags = [SHOWCASE_TAG]
    return actor


def main():
    world = ensure_level()
    removed = clear_previous()
    spawn_ground()
    spawn_lighting()
    entries, ring_radius = place_landmarks()
    lights = spawn_landmark_lights(entries)
    labelled = sum(1 for entry in entries if spawn_label(entry, ring_radius))
    player_start = spawn_player_start()

    assert len(entries) == len(LANDMARKS), len(entries)
    assert len(lights) == len(entries), (len(lights), len(entries))
    for entry in entries:
        assert abs(entry["world_min_z"] - GROUND_TOP_Z) < 0.01, \
            "{} is not grounded: {}".format(entry["label"], entry["world_min_z"])
        reach = ring_radius + entry["geometry_radius_xy"]
        assert reach < GROUND_HALF_EXTENT, \
            "{} reaches {:.0f} cm, past the ground plane".format(entry["label"], reach)

    # Every light must be on its own building's front, aimed at it, reaching
    # across it, shadow-casting, and clear of every other building's cone.
    for record in lights:
        key = record["key"]
        assert record["attached_to"] == "Landmark_" + key, \
            "{}: light parent is {}".format(key, record["attached_to"])
        assert record["outer_cone_deg"] > record["recipe"]["half_angle_deg"], \
            "{}: cone is narrower than the building".format(key)
        assert record["attenuation_radius_cm"] > record["distance_cm"], \
            "{}: attenuation stops short of the building".format(key)
        readback = record["readback"]
        assert readback.get("cast_shadows") is True, \
            "{}: shadows are off".format(key)
        assert abs(readback.get("intensity", -1.0) - record["intensity_cd"]) < 1.0, \
            "{}: intensity readback {}".format(key, readback.get("intensity"))
        assert abs(readback.get("outer_cone_angle", -1.0)
                   - record["outer_cone_deg"]) < 0.01, \
            "{}: outer cone readback {}".format(key, readback.get("outer_cone_angle"))
        assert record["nearest_neighbour_outside_cone"]["margin_deg"] > 0.0, \
            "{}: light reaches {}".format(key, record["nearest_neighbour_outside_cone"])

    assert unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH), "level save failed"

    manifest = {
        "level": LEVEL_PATH,
        "layout": "ring around a central plaza, each landmark facing the centre",
        "ring_radius_cm": round(ring_radius, 2),
        "gap_cm": GAP_CM,
        "facing_basis": "local -Y is the front; yaw = ring angle - 90 degrees",
        "geometry_source": "computed from instance transforms, component relative "
                           "transforms and mesh bounds; non-rendering components excluded",
        "actors_removed_before_rebuild": removed,
        "ground_plane": {"mesh": GROUND_MESH, "material": GROUND_MATERIAL,
                         "scale": GROUND_SCALE, "top_z": GROUND_TOP_Z},
        "text_labels": labelled,
        "player_start": player_start.get_actor_label() if player_start else None,
        "landmark_lights": [{key: value for key, value in record.items()
                             if key not in ("actor", "recipe")} for record in lights],
        "landmark_light_recipe": {
            "type": "SpotLight, one per landmark, attached to its landmark actor",
            "role": "accent layered on top of the unchanged global key/fill/sky",
            "facade_clearance_cm": LIGHT_FACADE_CLEARANCE_CM,
            "height_factor_of_building": LIGHT_HEIGHT_FACTOR,
            "aim_height_factor_of_building": LIGHT_AIM_HEIGHT_FACTOR,
            "cone_margin_deg": LIGHT_CONE_MARGIN_DEG,
            "max_cone_deg": LIGHT_MAX_CONE_DEG,
            "inner_cone_fraction": LIGHT_INNER_CONE_FRACTION,
            "accent_lux_at_building": LIGHT_ACCENT_LUX,
            "attenuation_margin": LIGHT_ATTENUATION_MARGIN,
            "source_radius_cm": LIGHT_SOURCE_RADIUS_CM,
            "temperature_k": LIGHT_TEMPERATURE_K,
            "intensity_units": "candelas",
        },
        "landmarks": [{key: value for key, value in entry.items()
                       if key not in ("actor", "geometry")} for entry in entries],
        "passed": True,
    }
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    unreal.log("GUANGZHOU_LANDMARK_SHOWCASE " + json.dumps(manifest))
    print("SHOWCASE_LEVEL", LEVEL_PATH)
    print("SHOWCASE_RING_RADIUS_CM", round(ring_radius, 1))
    for entry in manifest["landmarks"]:
        print("SHOWCASE_LANDMARK", json.dumps({
            "label": entry["label"], "yaw": entry["yaw_deg"],
            "location": entry["location"], "height": entry["height_cm"],
            "components": entry["component_count"], "instances": entry["instance_count"]}))
    for entry in manifest["landmark_lights"]:
        print("SHOWCASE_LANDMARK_LIGHT", json.dumps({
            "label": entry["label"], "attached_to": entry["attached_to"],
            "distance_cm": entry["distance_cm"], "intensity_cd": entry["intensity_cd"],
            "outer_cone_deg": entry["outer_cone_deg"],
            "attenuation_radius_cm": entry["attenuation_radius_cm"],
            "nearest_neighbour_margin_deg":
                entry["nearest_neighbour_outside_cone"]["margin_deg"]}))
    print("SHOWCASE_MANIFEST", str(MANIFEST))


if __name__ == "__main__":
    main()
