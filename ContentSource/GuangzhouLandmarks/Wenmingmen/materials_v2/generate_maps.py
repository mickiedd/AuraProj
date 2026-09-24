"""Build repeatable Wenmingmen PBR maps from reference-guided image sources.

Run with: uv run --with pillow --with numpy python generate_maps.py --install

The source images are photo-style albedo studies. The derived normals approximate
surface relief from local contrast; they are not scans or measured height data.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter, ImageOps


HERE = Path(__file__).resolve().parent
SIZE = 2048
EDGE_BLEND = 112
MATERIALS = {
    "stone": {"roughness": 0.86, "normal_strength": 2.6},
    "limestone": {"roughness": 0.82, "normal_strength": 2.7},
    "roof": {"roughness": 0.75, "normal_strength": 2.8},
    "wood": {"roughness": 0.73, "normal_strength": 3.3},
}


def seam_blend(rgb: np.ndarray, width: int) -> np.ndarray:
    """Make opposing edge pixels agree without shifting the image's interior."""
    rgb = rgb.copy()
    weight = (0.5 + 0.5 * np.cos(np.linspace(0, np.pi, width)))
    x_weight = weight[None, :, None]
    left = rgb[:, :width, :].copy()
    right = rgb[:, -width:, :][:, ::-1, :].copy()
    middle = (left + right) * 0.5
    rgb[:, :width, :] = left * (1 - x_weight) + middle * x_weight
    rgb[:, -width:, :] = (right * (1 - x_weight) + middle * x_weight)[:, ::-1, :]

    y_weight = weight[:, None, None]
    top = rgb[:width, :, :].copy()
    bottom = rgb[-width:, :, :][::-1, :, :].copy()
    middle = (top + bottom) * 0.5
    rgb[:width, :, :] = top * (1 - y_weight) + middle * y_weight
    rgb[-width:, :, :] = (bottom * (1 - y_weight) + middle * y_weight)[::-1, :, :]
    return rgb


def blurred(gray: np.ndarray, radius: float) -> np.ndarray:
    image = Image.fromarray(np.uint8(np.clip(gray, 0, 1) * 255), "L")
    return np.asarray(image.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32) / 255.0


def save_maps(name: str, settings: dict[str, float], destination: Path,
              basecolor_jpeg: bool) -> Image.Image:
    source = Image.open(HERE / "sources" / f"{name}_generated.png").convert("RGB")
    source = ImageOps.fit(source, (SIZE, SIZE), method=Image.Resampling.LANCZOS)
    rgb = np.asarray(source, dtype=np.float32) / 255.0
    rgb = seam_blend(rgb, EDGE_BLEND)
    base = Image.fromarray(np.uint8(np.clip(rgb * 255 + 0.5, 0, 255)), "RGB")
    if basecolor_jpeg:
        base.save(destination / f"{name}_BaseColor.jpg", quality=95, subsampling=0)
    else:
        base.save(destination / f"{name}_BaseColor.png", optimize=True)

    # Local contrast gives shallow pores and wood fibres without treating the
    # broad albedo color differences as a large displacement field.
    gray = np.sum(rgb * np.array([0.2126, 0.7152, 0.0722], np.float32), axis=2)
    fine = gray - blurred(gray, 3)
    medium = gray - blurred(gray, 34)
    height = blurred(np.clip(0.5 + fine * 0.75 + medium * 0.45, 0, 1), 0.65)
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    strength = settings["normal_strength"]
    nx, ny = -gx * strength, -gy * strength
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack(((nx / length + 1) * 127.5,
                       (ny / length + 1) * 127.5,
                       (nz / length + 1) * 127.5), axis=2)
    Image.fromarray(np.uint8(np.clip(normal + 0.5, 0, 255)), "RGB").save(
        destination / f"{name}_Normal.png", optimize=True
    )

    # glTF / Unreal pack: R ambient occlusion, G perceptual roughness, B metal.
    # AO is deliberately shallow because the modeled joints receive real shadow.
    local = gray - blurred(gray, 14)
    cavity = np.clip(-local * 2.1, 0, 1)
    ao = np.clip(0.96 - cavity * 0.13, 0, 1)
    roughness = np.clip(settings["roughness"] - local * 0.19, 0.55, 0.96)
    orm = np.stack((ao * 255, roughness * 255, np.zeros_like(ao)), axis=2)
    Image.fromarray(np.uint8(orm + 0.5), "RGB").save(
        destination / f"{name}_ORM.png", optimize=True
    )

    edge_x = float(np.abs(rgb[:, 0] - rgb[:, -1]).mean())
    edge_y = float(np.abs(rgb[0] - rgb[-1]).mean())
    print(f"{name:9s} {SIZE}x{SIZE}  edge_delta_x={edge_x:.6f}  edge_delta_y={edge_y:.6f}")
    return base


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group()
    target.add_argument(
        "--install", action="store_true",
        help="write maps directly into the existing Wenmingmen textures directory",
    )
    target.add_argument(
        "--output-dir", type=Path,
        help="write install-format maps to another directory for verification",
    )
    args = parser.parse_args()
    destination = (HERE.parent / "textures" if args.install else
                   args.output_dir if args.output_dir else HERE)
    destination.mkdir(parents=True, exist_ok=True)
    basecolor_jpeg = args.install or args.output_dir is not None
    previews = [save_maps(name, settings, destination, basecolor_jpeg)
                for name, settings in MATERIALS.items()]
    sheet = Image.new("RGB", (SIZE, SIZE))
    for image, position in zip(previews, ((0, 0), (1, 0), (0, 1), (1, 1))):
        sheet.paste(image.resize((SIZE // 2, SIZE // 2), Image.Resampling.LANCZOS),
                    (position[0] * SIZE // 2, position[1] * SIZE // 2))
    preview_destination = destination if args.output_dir else HERE
    sheet.save(preview_destination / "material_preview.jpg", quality=90, subsampling=0)
    if args.install:
        print(f"Installed {len(MATERIALS)} material sets in {destination}")


if __name__ == "__main__":
    main()
