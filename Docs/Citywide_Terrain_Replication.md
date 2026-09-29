# Canton district replication contract

The `/Game/Canton/DistrictPrototype/` map is a 200 × 200 m engineering test inside the provisional modern-context Landscape. Its coordinates are a diagnostic sample, not the historical location of Wenmingmen. Keep it isolated from the M01 map and from any later historically accepted walled-city map.

For each future district, first obtain the H1 city frame, an H2 local control network for the named gate or feature, and a dated walking surface tied to a named vertical benchmark and datum. Preserve the source observation date, licence, measurement reliability, temporal relevance and modern-change risk in the feature register. Only then transform road centrelines, parcels, drains and gate foundations into accepted metric bounds. Do not reuse this district's diagnostic UTM box as historical geometry.

Keep the imported height layer locked. Store new broad grading, road corridors and drainage corrections in independent edit layers, with source IDs on each correction. Use meshes for sub-2 m details such as narrow lanes, kerbs and gutters, and write a local grade profile before placement. The slab prototype uses 2 m segment stations and samples the R16 bilinearly at centers and edges; the native validator checks all 100 main-road centers, collision hits and long edges. Repeat those checks on the accepted terrain variant and add a gate-threshold test against its surveyed datum.

Before promoting a district beyond provisional, run source and heightmap contracts, a fresh UE map load, map check, collision and navigation route, Development cook, and the fixed 1920 × 1080 High route on the target M4 host after 60 s warm-up and 180 s capture. Compare p95/p99 frame time, RSS and streaming failures with `Docs/Terrain_Acceptance_Budget.md`. Record screenshots under identical lighting for dry/wet comparisons and preserve the package/checksum manifest. A screenshot alone does not satisfy an acceptance gate.

## Implementation continuation — 2026-09-29

The 2026-09-29 packaged district proves a bounded 200 m walking route on the approved M4/Metal raster target (p95 30.425 ms; p99 31.815 ms). Do not extrapolate its 823 loaded mesh components / 617 max draw calls or ordinary-actor foliage to a complete city. Reuse the checked graph generation, simple collision, physical traversal harness and raw-sample reconciliation for new districts. Establish accepted H1/H2/Z before historical geometry replication, then re-run streaming/performance at the expanded scale.
