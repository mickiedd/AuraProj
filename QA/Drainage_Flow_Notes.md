# Provisional district drainage check

The covered-gutter blockout is an engineering flow test on the modern-context raster, not a mapped Qing drain. Its three directional catchments and two test outlets are recorded in `GIS/Drainage_Locations.geojson`. The geometry follows terrain without changing the locked imported Landscape base.

| Catchment | Flow | Modern surface at high end | Modern surface at low end | Exit |
|---|---|---:|---:|---|
| ENG-DRAIN-01A | local y −22 m → −90 m | 10.1328 m | 7.3320 m | south edge outlet |
| ENG-DRAIN-01B | local y −22 m → +34 m | 10.1328 m | 7.8594 m | central sump |
| ENG-DRAIN-01C | local y +98 m → +34 m | 9.4102 m | 7.8594 m | central sump |

All sampled 4 m steps in each declared flow direction are nonincreasing in `QA/Street_Grade_Profiles.csv`; `Scripts/test_canton_district_contract.py` checks that property. A real drain invert, capacity, outfall, historical location and vertical datum are unresolved. The low-point meshes are location cues only; they are not functioning hydraulic infrastructure.
