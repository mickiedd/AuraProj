"""Freeze a tool-only contract and explicit south-first R16 for native import."""
import hashlib
import importlib.util
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('validate', ROOT/'Tools/Validate_Heightmap.py')
validator=importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)
validator.validate()
meta=json.loads((ROOT/'Export/Provisional/Heightmap_Metadata.json').read_text())
output=ROOT/'Export/Provisional/Canton_Modern_Context_SouthFirst.r16'
source=ROOT/'Export/Provisional/Canton_Modern_Context_2017_R16.png'
codes=np.asarray(Image.open(source))
output.write_bytes(np.flipud(codes).astype('<u2').tobytes())
contract={
 'status':'Provisional engineering contract; historical acceptance excluded',
 'decision_basis':'2026-09-26 user requested blocker resolution and completion; master plan permits isolated prototype',
 'map':'/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL',
 'raw_height_path':str(output.relative_to(ROOT)), 'raw_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
 'png_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'source_metadata_sha256':hashlib.sha256((ROOT/'Export/Provisional/Heightmap_Metadata.json').read_bytes()).hexdigest(),
 'origin_easting_m':729400,'origin_northing_m':2557300,'horizontal_crs':'EPSG:32649',
 'terrain_zero_elevation_m':0,'vertical_datum':'EPSG:3855 EGM2008; modern-context only',
 'historical_terrain_zero_m':None,'historically_accepted':False,
 'vertices':2017,'quads_per_section':63,'sections_per_component':2,'components_per_axis':16,
 'actor_location_cm':[0,0,0],'actor_scale':[200,200,50],
 'source_png_row_zero':'north','r16_row_zero':'south','flip_y_applied_in_preparation':True,
 'native_import_additional_flip_y':False,'grid_size_components':4,'region_size_components':16,
 'edit_layer':'Base_Imported','base_locked':True,
 'validation_budget':{'encoded_height_error_codes':0,'collision_height_error_cm':2,'analytic_xy_error_cm':1e-6},
 'historical_gates':'Original Coordinate_Contract and acceptance budget remain unchanged and unresolved',
}
(ROOT/'Data/Canton_Prototype_Contract.json').write_text(json.dumps(contract,indent=2)+'\n')
print('Prepared south-first little-endian R16:',output.stat().st_size,'bytes')
from pyproj import Transformer
project=Transformer.from_crs('EPSG:4326','EPSG:32649',always_xy=True)
overlay={'status':'Provisional failed georeference; visual markers only','layers':{},'source_sha256':{}}
for layer in ('Wall','Gates'):
    path=ROOT/f'GIS/Provisional/{layer}.geojson'
    data=json.loads(path.read_text())
    overlay['source_sha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    for feature in data['features']:
        geometry=feature['geometry']
        if geometry['type']=='Point':geometry['coordinates']=list(project.transform(*geometry['coordinates']))
        elif geometry['type']=='LineString':geometry['coordinates']=[list(project.transform(*p)) for p in geometry['coordinates']]
        else:raise ValueError('Unsupported marker geometry')
    overlay['layers'][layer]=data
(ROOT/'Export/Provisional/UE_Overlay_Metres.json').write_text(json.dumps(overlay,indent=2)+'\n')
