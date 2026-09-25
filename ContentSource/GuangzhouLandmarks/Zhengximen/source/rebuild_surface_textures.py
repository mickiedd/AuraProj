"""Regenerate only the weathered masonry and traditional-character plaque maps.

The main package generator writes its full texture/mesh/FBX bundle on import;
load its definitions through the existing driver instead. Usage:
  python rebuild_surface_textures.py
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_zhengximen import load_definitions  # noqa: E402


def main():
    source = load_definitions()
    maps = source['material_maps']
    for name in ('GrayBrick', 'GatePlaque'):
        kind, seed, base = source['materials'][name]
        maps(name, kind, seed, base)
        print('Rebuilt', name, 'BaseColor / Normal / Roughness / Metallic / AO / Height', flush=True)


if __name__ == '__main__':
    main()
