"""Regenerate Zhengximen's aged infill wood and dark structural timber maps."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_zhengximen import load_definitions  # noqa: E402


def main():
    source = load_definitions()
    for name in ('AgedWood', 'DarkTimber'):
        kind, seed, base = source['materials'][name]
        source['material_maps'](name, kind, seed, base)
        print('Rebuilt', name, 'BaseColor / Normal / Roughness / Metallic / AO / Height', flush=True)


if __name__ == '__main__':
    main()
