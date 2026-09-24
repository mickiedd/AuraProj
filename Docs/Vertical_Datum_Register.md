# Vertical datum and elevation register — 2026-09-24

**Historical Z gate blocked.** [GEDTM30](https://zenodo.org/records/15689805) provides a modern modeled bare-earth raster and uncertainty raster with EGM2008 / EPSG:3855 heights. It is not a late-Qing ground survey. Terrain zero remains unset in `Data/Coordinate_Contract.json`; no height has been inferred from DEM minima or gate mesh pivots.

The [Copernicus GLO-30 product handbook](https://dataspace.copernicus.eu/sites/default/files/media/files/2024-06/geo1988-copernicusdem-spe-002_producthandbook_i5.0.pdf) states WGS84 horizontal coordinates and EGM2008 vertical reference. The [public COG documentation](https://copernicus-dem-30m.s3.amazonaws.com/readme.html) calls it a **digital surface model** with buildings and vegetation; it was not used as bare earth.

The [Guangta excavation report](https://wglj.gz.gov.cn/attachment/7/7994/7994473/10742941.pdf) mentions depths below present ground, but no absolute vertical benchmark for a late-Qing surface was identified. The [Guangzhou Museum account](https://www.guangzhoumuseum.cn/website_cn/Web/Resource/findingsDetail.aspx?id=53) describes Guangta's present base as 1.8 m below mosque ground; that is also relative.

The [Jiefang Middle Road stratigraphy paper](https://doi.org/10.13284/j.cnki.rddl.003304) (`ARCH-004`, p. 68) reports the Qing L3 layer at **5.92–6.22 m sea-level elevation**, at a site located at 23°07′11.45″N, 113°15′47.48″E. The datum realization is **not named**; the given coordinate is site-level rather than a point on the section; the stratigraphic band is not an 1880 ground surface. `QING-STRATUM-001` in `Data/Elevation_Constraints.csv` retains the published range with blank `height_m`, D terrain confidence and `candidate_stratum_not_terrain` status. Do not transform this range to EGM2008 or interpolate it into a heightmap until the datum and surface interpretation are established.

## Prototype district candidates

| Rank | Candidate | Provisional 200 × 200 m EPSG:32649 square | Asset readiness | Evidence / decision |
|---|---|---|---|---|
| 1 | Unnamed south-wall gate near cathedral (`GATE-SOUTH-01`) | E731107.3–731307.3; N2558531.8–2558731.8 | South-gate asset usable as a stand-in | D; no local holdout pass; working prototype only |
| 2 | Unnamed east gate by Examination Hall (`GATE-EAST-01`) | E732681.8–732881.8; N2559555.3–2559755.3 | Existing gate assets need identity review | D; no local holdout pass; fallback only |

The primary ranks first for available asset work and legibility on the map; its historical gate identity is unproven. Both squares are **working candidates**, not accepted districts. Require local surveyed checks, gate identity and absolute period height evidence before terrain or gate placement is accepted. Review modern earthworks and fill for every candidate source.
