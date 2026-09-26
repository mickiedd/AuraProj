"""Check the actual native-import bytes, coordinate conversion and frozen inputs."""
import hashlib
import json
from pathlib import Path
import unittest
import numpy as np
from PIL import Image
from pyproj import Transformer

ROOT=Path(__file__).resolve().parents[1]

class NativeImportContract(unittest.TestCase):
    def test_raw_orientation_and_contract(self):
        c=json.loads((ROOT/'Data/Canton_Prototype_Contract.json').read_text())
        raw=ROOT/c['raw_height_path']
        self.assertEqual(hashlib.sha256(raw.read_bytes()).hexdigest(),c['raw_sha256'])
        self.assertEqual(hashlib.sha256((ROOT/'Export/Provisional/Heightmap_Metadata.json').read_bytes()).hexdigest(),c['source_metadata_sha256'])
        png=ROOT/'Export/Provisional/Canton_Modern_Context_2017_R16.png'
        self.assertEqual(hashlib.sha256(png.read_bytes()).hexdigest(),c['png_sha256'])
        expected=np.asarray(Image.open(png))
        actual=np.frombuffer(raw.read_bytes(),dtype='<u2').reshape(2017,2017)
        np.testing.assert_array_equal(actual,np.flipud(expected))
        self.assertFalse(np.array_equal(actual,expected),'Fixture must distinguish a vertical flip')
        self.assertFalse(np.array_equal(actual,expected.T),'Fixture must distinguish transpose')
        self.assertEqual(c['actor_scale'],[200,200,50])
        self.assertEqual(c['actor_location_cm'],[0,0,0])
        self.assertFalse(c['historically_accepted'])
        self.assertIsNone(c['historical_terrain_zero_m'])

    def test_overlay_uses_metres(self):
        overlay=json.loads((ROOT/'Export/Provisional/UE_Overlay_Metres.json').read_text())
        inverse=Transformer.from_crs('EPSG:32649','EPSG:4326',always_xy=True)
        for name in ('Wall','Gates'):
            path=ROOT/f'GIS/Provisional/{name}.geojson'
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),overlay['source_sha256'][str(path.relative_to(ROOT))])
            originals=json.loads(path.read_text())['features']
            for converted,original in zip(overlay['layers'][name]['features'],originals):
                a,b=converted['geometry'],original['geometry']
                points=[a['coordinates']] if a['type']=='Point' else a['coordinates']
                sources=[b['coordinates']] if b['type']=='Point' else b['coordinates']
                for p,q in zip(points,sources):
                    self.assertTrue(729400<=p[0]<=733432 and 2557300<=p[1]<=2561332)
                    longitude,latitude=inverse.transform(*p)
                    self.assertAlmostEqual(longitude,q[0],places=7)
                    self.assertAlmostEqual(latitude,q[1],places=7)

if __name__=='__main__':unittest.main()
