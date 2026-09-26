"""M01 prototype export audit, including mutation tests of rejection paths."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('heightmap_validator', ROOT / 'Tools/Validate_Heightmap.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class HeightmapContract(unittest.TestCase):
    def test_export(self):
        self.assertTrue(module.validate()['encoding_passed'])

    def test_invalid_metadata_rejected(self):
        original = json.loads((ROOT / 'Export/Provisional/Heightmap_Metadata.json').read_text())
        for key, value in [('historically_accepted', True), ('row_zero', 'south'), ('ue_z_scale', 100), ('origin_easting_m', 0)]:
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'Export/Provisional').mkdir(parents=True)
                for name in ('Data', 'Sources', 'GIS', 'QA'):
                    (root / name).symlink_to(ROOT / name, target_is_directory=True)
                (root / 'Export/Provisional/Canton_Modern_Context_2017_R16.png').symlink_to(ROOT / 'Export/Provisional/Canton_Modern_Context_2017_R16.png')
                changed = dict(original, **{key: value})
                (root / 'Export/Provisional/Heightmap_Metadata.json').write_text(json.dumps(changed))
                with self.assertRaises(AssertionError):
                    module.validate(root)


if __name__ == '__main__':
    unittest.main()
