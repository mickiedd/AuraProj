"""Render repeatable front and three-quarter Wenmingmen GLB previews in Blender.

Run with Blender, not system Python::

    Blender -b --factory-startup -t 8 --python Scripts/PreviewWenmingmen.py -- \
      --glb ContentSource/GuangzhouLandmarks/Wenmingmen/Wenmingmen_HighDetail.glb \
      --output-prefix Saved/Reports/Wenmingmen/before

The two files are ``<prefix>-front.png`` and ``<prefix>-three-quarter.png``.
Trimesh's source is Z-up, but its GLB opens in Blender with source Z mapped to
negative Blender Y. The same -90 degree X correction used by the UE importer
stands the building upright for these previews.
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def arguments() -> argparse.Namespace:
    command_args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glb", type=Path, required=True)
    parser.add_argument("--output-prefix", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--height", type=int, default=1000)
    parser.add_argument("--details", action="store_true", help="Also render close views of the gate and tiled roof")
    parser.add_argument("--view", choices=("front", "three-quarter", "gate-detail", "roof-detail"), help="Render just one selected view")
    parser.add_argument("--save-blend", type=Path, help="Save a packed Blender review scene")
    return parser.parse_args(command_args)


def add_sun(name: str, rotation: tuple[float, float, float], energy: float) -> None:
    lamp = bpy.data.lights.new(name, type="SUN")
    lamp.energy = energy
    lamp.angle = math.radians(8)
    obj = bpy.data.objects.new(name, lamp)
    bpy.context.scene.collection.objects.link(obj)
    obj.rotation_euler = tuple(math.radians(angle) for angle in rotation)


def aim(camera: bpy.types.Object, location: tuple[float, float, float], target: tuple[float, float, float], scale: float) -> None:
    camera.location = location
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.ortho_scale = scale


def main() -> None:
    args = arguments()
    source = args.glb.expanduser().resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    if args.width <= 0 or args.height <= 0:
        raise ValueError("Preview dimensions must be positive")
    output_prefix = args.output_prefix.expanduser().resolve()
    output_prefix.parent.mkdir(parents=True, exist_ok=True)

    # --factory-startup is recommended, but clear the scene even if a user
    # runs the script with their regular Blender startup file.
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(source))
    imported = set(bpy.data.objects) - before
    meshes = [obj for obj in imported if obj.type == "MESH"]
    # The editable base GLB has seven meshes; the high detail GLB adds the
    # separate inscription plane as an eighth node.
    if len(meshes) not in (7, 8):
        raise RuntimeError(f"Expected seven or eight Wenmingmen mesh nodes; found {len(meshes)}")
    upright = Matrix.Rotation(-math.pi / 2, 4, "X")
    for obj in imported:
        if obj.parent not in imported:
            obj.matrix_world = upright @ obj.matrix_world
    bpy.context.view_layer.update()

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = args.width
    scene.render.resolution_y = args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.72, 0.77, 0.82, 1)
    background.inputs["Strength"].default_value = 0.38
    add_sun("Warm key", (35, -25, -35), 2.1)
    add_sun("Soft fill", (-15, 25, 120), 0.28)
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"

    camera_data = bpy.data.cameras.new("Preview camera")
    camera_data.type = "ORTHO"
    camera = bpy.data.objects.new("Preview camera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera

    views = {
        "front": ((0.0, -66.0, 18.0), (0.0, -8.5, 8.0), 72.0),
        "three-quarter": ((39.0, -55.0, 25.0), (0.0, -8.5, 8.0), 84.0),
    }
    if args.details:
        views.update({
            "gate-detail": ((9.0, -22.0, 10.0), (0.0, -1.5, 5.0), 21.0),
            "roof-detail": ((17.0, -22.0, 25.0), (0.0, 0.0, 15.3), 24.0),
        })
    if args.view:
        if args.view not in views:
            raise ValueError("A detail view also needs --details")
        views = {args.view: views[args.view]}
    for name, (location, target, scale) in views.items():
        aim(camera, location, target, scale)
        scene.render.filepath = str(output_prefix) + f"-{name}.png"
        bpy.ops.render.render(write_still=True)
        print("WENMINGMEN_PREVIEW", name, scene.render.filepath, flush=True)
    if args.save_blend:
        blend = args.save_blend.expanduser().resolve()
        blend.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.file.pack_all()
        bpy.ops.wm.save_as_mainfile(filepath=str(blend))
        print("WENMINGMEN_REVIEW_SCENE", blend, flush=True)


if __name__ == "__main__":
    main()
