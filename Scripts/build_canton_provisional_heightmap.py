"""Build isolated modern-context terrain; never writes accepted historical outputs."""
import hashlib
import json
from pathlib import Path
import numpy as np
import rasterio
from rasterio.transform import from_origin
from rasterio.warp import reproject, Resampling
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    contract = json.loads((ROOT / 'Data/Coordinate_Contract.json').read_text())
    n = contract['proposed_landscape']['vertices_per_axis']
    spacing = contract['proposed_landscape']['spacing_m']
    east, north = contract['origin_easting_m'], contract['origin_northing_m']
    extent = (n - 1) * spacing
    # GeoTIFF pixel centres are UE vertices, including both envelope endpoints.
    transform = from_origin(east - spacing / 2, north + extent + spacing / 2, spacing, spacing)
    source = ROOT / 'Sources/Elevation/GEDTM30_Guangzhou_context_20250611.tif'
    with rasterio.open(source) as src:
        values = src.read(1, masked=True).astype('float32') * src.scales[0] + src.offsets[0]
        heights = np.full((n, n), np.nan, dtype='float32')
        reproject(values.filled(np.nan), heights, src_transform=src.transform, src_crs=src.crs,
                  dst_transform=transform, dst_crs=contract['horizontal_crs'],
                  src_nodata=np.nan, dst_nodata=np.nan, resampling=Resampling.bilinear)
    if not np.isfinite(heights).all():
        raise ValueError('Missing elevation coverage; no gap filling permitted')
    # Tool-only zero: EGM2008 0 m, not the unresolved historical terrain zero.
    step = 50 / 128 / 100
    if heights.min() < -32768 * step or heights.max() > 32767 * step:
        raise ValueError('Clipping: revise the explicit encoding before export')
    codes = np.rint(heights.astype('float64') / step + 32768).astype('uint16')
    paths = []
    for relative, array, dtype, tags in [
        ('GIS/Provisional/Modern_Context_Elevation_float32.tif', heights, 'float32', {'vertical_datum': 'EPSG:3855', 'source_id': 'ELEV-002'}),
        ('GIS/Provisional/Terrain_Confidence_PROVISIONAL.tif', np.full((n, n), 4, dtype='uint8'), 'uint8', {'legend': '4=D; modern context only; no historical measurements'}),
        ('QA/Provisional/Modern_Change_Mask.tif', np.ones((n, n), dtype='uint8'), 'uint8', {'legend': '1=potential modern contamination, unassessed; not a detected-change classification'}),
    ]:
        path = ROOT / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        with rasterio.open(path, 'w', driver='GTiff', width=n, height=n, count=1,
                           dtype=dtype, crs=contract['horizontal_crs'], transform=transform,
                           compress='deflate') as dst:
            dst.write(array, 1)
            dst.update_tags(status='PROVISIONAL_MODERN_CONTEXT_ONLY', **tags)
        paths.append(relative)
    png = 'Export/Provisional/Canton_Modern_Context_2017_R16.png'
    Image.fromarray(codes).save(ROOT / png)
    paths.append(png)
    metadata = {
        'status': 'PROVISIONAL_MODERN_CONTEXT_ONLY', 'historically_accepted': False,
        'source_id': 'ELEV-002', 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'license': 'CC BY 4.0; OpenGeoHub GEDTM30 v1.1',
        'coordinate_contract_sha256': hashlib.sha256((ROOT / 'Data/Coordinate_Contract.json').read_bytes()).hexdigest(),
        'horizontal_crs': contract['horizontal_crs'], 'origin_easting_m': east,
        'origin_northing_m': north, 'vertices': n, 'spacing_m': spacing, 'extent_m': extent,
        'row_zero': 'north', 'column_zero': 'west', 'ue_x': 'easting', 'ue_y': 'northing',
        'ue_import_orientation': 'Reverse image rows so UE positive Y increases north; verify in editor before acceptance',
        'vertical_datum': 'EPSG:3855', 'tool_only_zero_m': 0,
        'historical_terrain_zero_m': None, 'ue_z_scale': 50, 'code_zero': 32768,
        'metres_per_code': step, 'decode': '(code - 32768) * metres_per_code + tool_only_zero_m',
        'interpolation': 'bilinear resampling of modern DTM; no historical interpolation or corrections',
        'breaklines': 'none: no datum-backed historical breaklines available',
        'height_min_m': float(heights.min()), 'height_max_m': float(heights.max()),
        'artifacts': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths},
    }
    (ROOT / 'Export/Provisional/Heightmap_Metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps({'status': metadata['status'], 'range_m': [metadata['height_min_m'], metadata['height_max_m']], 'artifacts': paths}, indent=2))


if __name__ == '__main__':
    main()
