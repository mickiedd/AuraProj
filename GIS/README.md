# GIS work area

`Provisional/` holds an affine review raster and six vector layers traced from `MAP-001`. The transform uses unsurveyed landmark candidates and **fails** Day 03's independent-check budget (3 holdouts; 93.24 m RMSE; 132.155 m worst). The vectors are coarse visual traces, shown in `QA/Canton_Provisional_Trace_Overlay.jpg`, and are not survey-grade geometry.

GeoJSON geometry uses WGS84 longitude/latitude per RFC 7946. Properties retain the map source, 1880 publication, D confidence, map-pixel trace and EPSG:32649 working metrics. Inputs are `Data/Map_Pixel_Traces_Provisional.json` and `Data/Map_Transform_Provisional.json`; rebuild with `Scripts/build_canton_provisional_layers.py` after installing the terrain dependencies.

Modern candidate locations came from OpenStreetMap under ODbL 1.0; preserve OSM attribution and review derived-database obligations before redistributing the provisional vectors.

Accepted root layers (`Wall.geojson`, `Gates.geojson`, `Roads_Main.geojson`, `Waterways.geojson`) remain absent until surveyed controls pass Day 03. Provisional layers must not feed a production UE import or historical elevation interpolation.
