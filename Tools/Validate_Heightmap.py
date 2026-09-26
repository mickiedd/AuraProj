"""Audit the isolated modern-context export. Passing does not accept M01."""
import hashlib
import json
from pathlib import Path
import struct
import numpy as np
from PIL import Image
import rasterio

ROOT = Path(__file__).resolve().parents[1]


def validate(root=ROOT):
    meta = json.loads((root / 'Export/Provisional/Heightmap_Metadata.json').read_text())
    assert meta['status'] == 'PROVISIONAL_MODERN_CONTEXT_ONLY' and meta['historically_accepted'] is False
    assert meta['historical_terrain_zero_m'] is None
    contract_bytes = (root / 'Data/Coordinate_Contract.json').read_bytes()
    assert hashlib.sha256(contract_bytes).hexdigest() == meta['coordinate_contract_sha256']
    contract = json.loads(contract_bytes)
    assert (meta['origin_easting_m'], meta['origin_northing_m'], meta['horizontal_crs']) == (contract['origin_easting_m'], contract['origin_northing_m'], contract['horizontal_crs'])
    assert hashlib.sha256((root / 'Sources/Elevation/GEDTM30_Guangzhou_context_20250611.tif').read_bytes()).hexdigest() == meta['source_sha256']
    for relative, digest in meta['artifacts'].items():
        assert hashlib.sha256((root / relative).read_bytes()).hexdigest() == digest, relative
    png = root / 'Export/Provisional/Canton_Modern_Context_2017_R16.png'
    raw = png.read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    width, height, depth, color = struct.unpack('>IIBB', raw[16:26])
    assert (width, height, depth, color) == (2017, 2017, 16, 0)
    assert meta['vertices'] == 2017 and meta['spacing_m'] == 2 and meta['extent_m'] == 4032
    assert meta['row_zero'] == 'north' and meta['column_zero'] == 'west'
    assert meta['code_zero'] == 32768 and meta['ue_z_scale'] == 50 and meta['tool_only_zero_m'] == 0
    assert meta['metres_per_code'] == meta['ue_z_scale'] / 128 / 100
    codes = np.asarray(Image.open(png)).astype('float64')
    with rasterio.open(root / 'GIS/Provisional/Modern_Context_Elevation_float32.tif') as src:
        terrain = src.read(1)
        assert src.crs.to_string() == meta['horizontal_crs'] and src.dtypes == ('float32',)
        transform = src.transform
    assert np.isfinite(terrain).all()
    step = meta['metres_per_code']
    assert terrain.min() >= -32768 * step and terrain.max() <= 32767 * step
    error = float(np.max(np.abs((codes - 32768) * step - terrain)))
    assert error <= step / 2 + 1e-6
    # Analytic frame checks: these are not surveyed independent landmarks.
    for row, col in [(0, 0), (0, 2016), (2016, 0), (2016, 2016), (1008, 1008)]:
        e, n = transform * (col + .5, row + .5)
        x, y = (e-meta['origin_easting_m'])*100, (n-meta['origin_northing_m'])*100
        assert abs(x - col*200) <= 1e-6 and abs(y-(2016-row)*200) <= 1e-6
        assert abs(meta['origin_easting_m']+x/100-e) <= 1e-9
        assert abs(meta['origin_northing_m']+y/100-n) <= 1e-9
    for relative, expected in [('GIS/Provisional/Terrain_Confidence_PROVISIONAL.tif', 4), ('QA/Provisional/Modern_Change_Mask.tif', 1)]:
        with rasterio.open(root / relative) as src:
            assert src.transform == transform and src.shape == terrain.shape
            assert src.crs.to_string() == meta['horizontal_crs']
            assert np.all(src.read(1) == expected)
    return {'encoding_passed': True, 'historically_accepted': False, 'ue_import_verified': False,
            'dimensions': [width, height], 'bit_depth': depth, 'code_range': [int(codes.min()), int(codes.max())],
            'clipped_samples': 0, 'max_quantization_error_m': error, 'allowed_error_m': step/2,
            'analytic_frame_checks': 5, 'surveyed_landmark_checks': 0}


if __name__ == '__main__':
    print(json.dumps(validate(), indent=2))
