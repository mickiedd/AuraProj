# H2 local-control request

H2 establishes the gate-district local historical frame independently of the failed MAP-001 city transform.

Primary named candidate: **文明门 (Wenmingmen)**, described by the Guangzhou municipal historical-gate account as opposite the Workers Cultural Palace on Wenming Road. Fallback: **正东门 (Zhengdongmen / 大东门)** near the Zhongshan Road–Yuexiu Road intersection. These are approximate source descriptions, not measured points. The current technical map's unnamed south-wall marker remains a separate diagnostic location.

For each candidate named gate/district, request a local survey, conservation or archaeological dataset that provides:

- at least four independent local check points;
- exact physical point definitions and feature identity;
- coordinates with horizontal CRS/datum;
- survey date, method, accuracy/uncertainty and responsible authority;
- plan/vector/CAD/GIS geometry sufficient to reproduce the checks;
- measured alignment-feature widths where the half-feature-width rule could be stricter.

H2 passes only when holdout RMSE ≤2 m and worst residual ≤4 m, or the stricter half-feature-width limit. The current UTM squares in `Data/Gate_District_Candidates.csv` are `PROVISIONAL_DIAGNOSTIC_ONLY`; derive final 200 × 200 m bounds after H2 passes.

Use [QA/Primary_Source_Request_M01.md](../QA/Primary_Source_Request_M01.md) as the outward-facing request and record candidate sources in `Data/Authoritative_Source_Leads_2026-09-26.csv`. The named gate descriptions come from the [Guangzhou municipal account of historic gates](https://www.gz.gov.cn/zlgz/whgz/content/post_8929405.html), which does not provide survey controls.
