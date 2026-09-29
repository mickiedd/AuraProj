# Canton source-to-prototype change log

This log distinguishes retained source evidence from the new diagnostic implementation. “D” means a temporary engineering placeholder; it is not a historical accuracy grade earned by the prototype.

| Feature | Input / source status | Prototype change | Historical status |
|---|---|---|---|
| Walled-city XY | `MAP-001` in `Data/Source_Register.csv`; failed candidate fit and unsurveyed holdouts in `QA/Map_GCP_Residuals.csv` | Retained coarse wall/gate overlay in `GIS/Provisional/`; it was not used to derive the district's metric bounds | H1 blocked; overlay D |
| Terrain elevations | `ELEV-002` modern EGM2008 context, approximately 30 m source resolution | Resampled to 2 m working grid and encoded south-first R16; no period cut/fill, breaklines or measured walking surface added | Historical Z blocked; raster D |
| Diagnostic district XY | No accepted H2 local control network | Selected an unrelated 200 × 200 m sample documented in `Maps/District_Test_Bounds.json` | H2 blocked; no Wenmingmen location claim |
| Streets and parcels | No source-traced, H2-aligned late-Qing street or parcel survey | Authored a 6 m principal corridor, 3.2 m mixed lane and non-rendered parcel references in district GIS and UE | D engineering geometry only |
| Drainage | No mapped Qing drain alignment or invert | Added three monotone flow reaches, 48 covered-gutter pieces and two outlet markers for slope/collision testing | D engineering flow only |
| Paving and ground materials | Monochrome/uncertain period surface evidence; no approved module dimensions or color | Added repeated cube slabs, six source-neutral Landscape paint targets and flat diagnostic materials | D blockout; final art open |
| Vegetation | No documented period species or planting positions | Added 38 sparse non-colliding green cube proxies under deterministic exclusion rules | D blockout; M04 incomplete |
| Wetness and debris | No period deposition or hydrology model | Added 14 separable dark damp cards; no debris/litter/grit art | D blockout; M04 incomplete |
| Wenmingmen gate | Existing project Blueprint, not a surveyed threshold reference | Placed one unchanged-scale instance with a diagnostic pivot offset for integration testing | Scale/interface test only; H2 and Z blocked |

The prototype did not replace the M01 source register, failed georeference residuals, or locked `Base_Imported` Landscape layer. Historic promotion requires new measured controls and dated Z evidence, then a versioned reconstruction pass.

## Implementation continuation — 2026-09-29

2026-09-29: added project-authored procedural paving/texture/blade/leaf/wet-mesh source assets with hashes and D-confidence provenance. Replaced only the district gate instance collision/nav export with source-derived simple proxies; retained closed door, source bridge, shared gate asset and scale. Added mesh approach/foundation support without editing the modern-context raster or locked base. Implemented map-specific native pawn, real runtime sampling, dedicated cook/stage and sandbox-compatible report collection. Historical source/control/vertical registers admitted no new measurements. See QA/Canton_Continuation and the current M05 handoff.
