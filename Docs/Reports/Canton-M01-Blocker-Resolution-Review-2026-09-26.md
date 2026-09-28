# Canton M01 blocker-resolution review — 2026-09-26

## Decision

The review confirms that the current blockers are evidence-contract failures rather than UE Landscape defects. The plan now separates technical prototype readiness from historical verification, replaces the single horizontal gate with H1/H2, and prevents failed-transform-derived district boxes from becoming historical survey bounds.

| Axis | Current status | Plan decision |
|---|---|---|
| `Technical_UE` | Provisional technical pass | Continue under the explicit modern-context/D-confidence contract. |
| `Historical_XY_City (H1)` | Blocked on current MAP-001 transform | Preserve the failed affine; build a city frame from a survey-capable backbone and independent measured controls. |
| `Historical_XY_GateLocal (H2)` | Blocked; strategy changed | Establish a separate local survey/conservation/archaeological network. Do not require MAP-001 to provide metre-level local accuracy. |
| `Historical_Z` | Blocked | Require a datum-backed 1880–1900 walking surface. Modern DTM remains broad-morphology context only. |

## Applied changes

1. H1 retains the city contract: at least five fit controls, eight disjoint holdouts with quadrant and perimeter/interior coverage, RMSE ≤15 m, and worst residual ≤30 m.
2. H2 retains the local contract: at least four independent local checks, RMSE ≤2 m, and worst residual ≤4 m or the stricter half-feature-width rule.
3. MAP-001 remains immutable period layout/provenance evidence. Its failed affine is not a metric backbone.
4. The gate district is selected by named historical feature and source coverage. Current provisional UTM squares are diagnostic until H2 passes.
5. Day 01 now carries a versioned working budget plus explicit `owner_approval`. Technical work may proceed while approval is pending; no historical axis can be `Verified`.
6. Days 06–20 may continue only on a provisional path whose geometry and elevation remain separable from verified historical layers.

## Source strategy

- Request the original/high-resolution 1907 `广东省城内外全图（河南附）` and its scale, grid/control marks and survey notes as a near-period geometry-backbone candidate.
- Request the archive original of the 1900 `粤东省城图` and use it to test persistence/change across the study-window boundary; do not assume equal metric quality.
- Request survey vector/CAD/GIS data and mathematical basis for surviving monuments such as Guangta.
- Request archaeological section sheets, benchmark tables, datum realization and explicit late-Qing walking-surface interpretation.

The actionable requests are recorded in [Authoritative_Source_Leads_2026-09-26.csv](../../Data/Authoritative_Source_Leads_2026-09-26.csv) and [Primary_Source_Request_M01.md](../../QA/Primary_Source_Request_M01.md).

## Stop/go

**Go:** UE terrain implementation, material/road prototyping and tooling may continue under the provisional technical contract.

**Stop for historical verification:** H1, H2 and Historical_Z remain blocked until primary evidence is acquired and passes the unchanged budgets.

## Immutable failed result

The failed transform remains an audit record and must not be overwritten. Its recorded SHA-256 is `7810a1541ba4c78ec5569d92c94dbfbdb4da29cffb70107b2bf2bbc224aa058c`.
