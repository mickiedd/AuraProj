# Day 03/05 measured-evidence request — 2026-09-24

The provisional Vrooman/Kerr map fit uses five unsurveyed OSM centroid controls and three independent checks. The independent checks are **NW 57.231 m**, **SW 132.155 m** and **SE 73.079 m** from the wall-trace bounding-box centre. There is **no NE holdout**; the SW check lies outside the wall bounding box. Even excluding the worst check for diagnosis, the other two have approximately **65.64 m RMSE**, above the 15 m target. This is a coverage and distortion problem, not a single discrepant point to remove.

## Horizontal evidence required

1. Supply at least **five fit controls plus eight separate independent checks**, with at least two checks in each of NW, NE, SW and SE and both perimeter and interior sites represented. Do not reuse a fit point as an independent check.
2. For each point, provide a dated survey record, source document/authority, coordinate pair, horizontal CRS/EPSG, survey method and stated uncertainty. Record an exact physically persistent feature point (for example, a documented tower foundation corner), not a temple-compound centroid or a road intersection altered after 1880. Photograph or survey sketch should identify the same point on the 1880 plate and today.
3. Survey at least **four additional independent local checks** within the selected 200 × 200 m south-wall district (E731107.3–731307.3, N2558531.8–2558731.8 in the *current provisional* EPSG:32649 frame). The fallback east-gate square is E732681.8–732881.8, N2559555.3–2559755.3. Confirm the gate identity before using either square for placement.
4. Refit only after replacing the candidate table with measured coordinates and explicit roles. Do not overwrite `MAP-001` or the failed transform; create a new versioned source/transform. Report all control and holdout residual vectors. Acceptance is ≥8 distributed map checks, ≤15 m RMSE and ≤30 m worst; the district requires ≥4 local checks, ≤2 m RMSE and ≤4 m worst, tightened to half the narrowest measured alignment feature where applicable.

If a global affine cannot pass, document distortion zones and test a local/higher-order model using the **same reserved holdouts**. A fit with low control residuals alone is insufficient.

## Vertical evidence required

For an 1880–1900 ground Z, provide a dated walking or ground surface tied to an explicitly named local benchmark and vertical datum, survey elevation/range, section location and archaeological interpretation. State whether an older height uses the Guangzhou system or the 1985 National Height Datum; the official `DATUM-001` relation is conditional on that identification. A separate documented conversion to the GEDTM30 EGM2008 reference is needed before comparison. `QING-STRATUM-001` is a cultural-layer range, not such a surface.

The current Days 03–05 artifacts can be reviewed without these inputs, but accepted root GIS layers and terrain-height interpolation cannot be produced from them.
