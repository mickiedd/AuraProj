# Map distortion and georeferencing status — 2026-09-24

**Provisional affine produced; acceptance blocked.** `MAP-001` is an 1880 publication (survey date unknown). `OSM-001` supplies eight manually matched map icons and modern OSM way centroids: five affine-fit controls and three independent holdouts. Pixel uncertainty is recorded per candidate. Coefficients and acceptance are in `Data/Map_Transform_Provisional.json`; residuals are in `QA/Map_GCP_Residuals.csv`.

The holdouts yield **93.24 m RMSE** and **132.155 m worst residual**. The proposed gate requires at least eight independent checks, ≤15 m RMSE and ≤30 m worst. Modern compound centroids are not stable surveyed marks. No point was discarded to improve the fit and no modern street line was forced to match.

`GIS/Provisional/Canton_Historic_Georef_PROVISIONAL.tif` is a reproducible affine GeoTIFF for visual review. The six `GIS/Provisional/*.geojson` layers use RFC 7946 WGS84 coordinates, with EPSG:32649 metric values in properties. `QA/Canton_Provisional_Trace_Overlay.jpg` shows the coarse digitization against the scan. A spatial distortion surface would be misleading with only three holdouts. Accepted root GIS outputs remain absent pending surveyed, distributed controls and a passing independent test.
