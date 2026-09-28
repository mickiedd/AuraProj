# Day 03/05 measured-evidence request — 2026-09-24

The provisional Vrooman/Kerr map fit uses five unsurveyed OSM centroid controls and three independent checks. The independent checks are **NW 57.231 m**, **SW 132.155 m** and **SE 73.079 m** from the wall-trace bounding-box centre. There is **no NE holdout**; the SW check lies outside the wall bounding box. Even excluding the worst check for diagnosis, the other two have approximately **65.64 m RMSE**, above the 15 m target. This is a coverage and distortion problem, not a single discrepant point to remove.

## Horizontal H1 evidence required

1. Supply at least **five fit controls plus eight separate independent checks**, with at least two checks in each of NW, NE, SW and SE and both perimeter and interior sites represented. Do not reuse a fit point as an independent check.
2. For each point, provide a dated survey record, source document/authority, coordinate pair, horizontal CRS/EPSG, survey method and stated uncertainty. Record an exact physically persistent feature point (for example, a documented tower foundation corner), not a temple-compound centroid or a road intersection altered after 1880. Photograph or survey sketch should identify the same point on the 1880 plate and today.
3. Use a survey-capable geometry backbone for the city frame. Preserve `MAP-001` as historical layout/provenance evidence unless its own metric quality is independently demonstrated.
4. Refit only after replacing the candidate table with measured coordinates and explicit roles. Do not overwrite `MAP-001` or the failed transform; create a new versioned source/transform. Report all control and holdout residual vectors. H1 acceptance is ≥8 distributed holdouts, ≤15 m RMSE and ≤30 m worst.

If a global affine cannot pass, document distortion zones and test a local/higher-order model using the **same reserved holdouts**. A fit with low control residuals alone is insufficient.

## Horizontal H2 evidence required

1. Select the prototype district by **named historical gate/feature and source coverage**. The current south-wall and east-gate UTM squares were derived from a failed city transform and are diagnostic only; do not use them as accepted survey limits or historical placement.
2. Acquire a local survey, conservation or archaeological control network independent of the failed MAP-001 transform. Provide at least **four independent local checks** with exact feature definitions, coordinates, horizontal CRS/datum, method, date and stated uncertainty.
3. Derive the final 200 × 200 m metric district bounds only after the local frame is accepted. H2 acceptance is RMSE ≤2 m and worst residual ≤4 m, tightened to half the narrowest source-measured alignment feature where applicable.
4. Record the chosen gate identity, ranked fallback, evidence coverage, transform version and all residual vectors. If no qualifying local network exists, classify H2 `Blocked` and keep downstream district work explicitly provisional and separable from verified geometry.

The named request now starts with **Wenmingmen** (primary) and **Zhengdongmen / 大东门** (fallback). A [Guangzhou municipal historical account](https://www.gz.gov.cn/zlgz/whgz/content/post_8929405.html) gives approximate modern street descriptions for both; it supplies no H2 control points or accepted metric bounds. Request the original survey/conservation geometry for each named site before choosing a final district.

## Source-route leads to verify

- A 1907 `广东省城内外全图（河南附）` is a near-period survey-capable candidate. It remains outside the 1880–1900 target and must be compared with in-period evidence before its geometry is carried backward.
- A photographed `粤东省城图（1900年）` is an in-period temporal corroboration lead; the public description does not yet establish its metric method or accuracy.
- The Guangta protection-plan material is a conservation-geometry lead. Its published coordinate/dimensions are not H2 controls until the datum, survey method, exact point definitions and uncertainty are supplied.

Track these requests in `Data/Authoritative_Source_Leads_2026-09-26.csv` and `QA/Primary_Source_Request_M01.md`.

## Vertical evidence required

For an 1880–1900 ground Z, provide a dated walking or ground surface tied to an explicitly named local benchmark and vertical datum, survey elevation/range, section location and archaeological interpretation. State whether an older height uses the Guangzhou system or the 1985 National Height Datum; the official `DATUM-001` relation is conditional on that identification. A separate documented conversion to the GEDTM30 EGM2008 reference is needed before comparison. `QING-STRATUM-001` is a cultural-layer range, not such a surface.

The current Days 03–05 artifacts can be reviewed without these inputs, but accepted root GIS layers and terrain-height interpolation cannot be produced from them.
