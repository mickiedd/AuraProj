"""Render Zhengximen previews in Blender for visual review.

Adapted from the tuning skill's `render_glb_previews.py`. The skill's own VIEWS are
tuned for a roughly 65 x 35 m landmark; Zhengximen is 53.2 x 10.8 m and 16.9 m tall,
so the cameras and ortho scales here are its own. (The skill docstring asks you to
tune VIEWS for a different footprint, which means editing a shared file - a
`--views` override would be the better fix.)

The generator source is Z-up. The GLB opens in Blender with source Z mapped to
negative Blender Y, so a -90 degree X rotation stands the building upright - the same
correction the Unreal importer applies. Without it the model lies on its side.

Run with Blender, NOT system Python:

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -t 8 \
      --python Scripts/RenderZhengximenPreviews.py -- \
      --glb <glb> --output-prefix Saved/Reports/Zhengximen/<version> --details
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

# (location, target, ortho_scale) for a 53.2 x 10.8 x 16.9 m gate centred on the
# origin with its facade toward -Y.
VIEWS = {
    "front": ((0.0, -58.0, 12.0), (0.0, 0.0, 8.4), 60.0),
    "three-quarter": ((33.0, -46.0, 20.0), (0.0, 0.0, 8.4), 62.0),
    "gate-detail": ((5.5, -17.0, 6.5), (0.0, 0.0, 4.6), 17.0),
    # Straight down the gateway. Any band of background between the door and the
    # arch is a real gap here - an angled view cannot tell a gap from the shadowed
    # inside of the barrel vault, which is why this view exists.
    "arch-front": ((0.0, -20.0, 3.1), (0.0, 0.0, 3.1), 9.0),
    "pavilion-detail": ((13.0, -17.0, 19.0), (0.0, 0.0, 13.0), 21.0),
}
DETAIL_VIEWS = ("gate-detail", "arch-front", "pavilion-detail")


def arguments() -> argparse.Namespace:
    command_args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--glb", type=Path, required=True)
    parser.add_argument("--output-prefix", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--height", type=int, default=1000)
    parser.add_argument("--details", action="store_true")
    parser.add_argument("--view", choices=tuple(VIEWS))
    return parser.parse_args(command_args)


def add_sun(name: str, rotation: tuple[float, float, float], energy: float) -> None:
    lamp = bpy.data.lights.new(name, type="SUN")
    lamp.energy = energy
    lamp.angle = math.radians(8)
    obj = bpy.data.objects.new(name, lamp)
    bpy.context.scene.collection.objects.link(obj)
    obj.rotation_euler = tuple(math.radians(angle) for angle in rotation)


def aim(camera: bpy.types.Object, location: tuple, target: tuple, scale: float) -> None:
    camera.location = location
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.ortho_scale = scale


def main() -> None:
    args = arguments()
    source = args.glb.expanduser().resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    output_prefix = args.output_prefix.expanduser().resolve()
    output_prefix.parent.mkdir(parents=True, exist_ok=True)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(source))
    imported = set(bpy.data.objects) - before
    meshes = [obj for obj in imported if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError("No mesh nodes found in the GLB")
    upright = Matrix.Rotation(-math.pi / 2, 4, "X")
    for obj in imported:
        if obj.parent not in imported:
            obj.matrix_world = upright @ obj.matrix_world
    bpy.context.view_layer.update()
    print("PREVIEW_MESHES", [(obj.name, len(obj.data.polygons)) for obj in meshes],
          flush=True)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in \
        {item.identifier for item in
         bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
    scene.render.resolution_x = args.width
    scene.render.resolution_y = args.height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
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

    views = {name: VIEWS[name] for name in ("front", "three-quarter")}
    if args.details:
        views.update({name: VIEWS[name] for name in DETAIL_VIEWS})
    if args.view:
        views = {args.view: VIEWS[args.view]}

    for name, (location, target, scale) in views.items():
        aim(camera, location, target, scale)
        scene.render.filepath = str(output_prefix) + f"-{name}.png"
        bpy.ops.render.render(write_still=True)
        print("PREVIEW", name, scene.render.filepath, flush=True)


if __name__ == "__main__":
    main()
