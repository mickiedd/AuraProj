# Provisional heightmap encoding audit — 2026-09-26

The isolated export passes automated encoding checks; accepted historical Day 08 remains blocked. See [M01 review](../Review/M01_Terrain_Review.md) for commands and limitations and [machine results](Provisional/Heightmap_Validation.json).

`Export/Provisional/Canton_Modern_Context_2017_R16.png`: 2017 × 2017, single-channel uint16 PNG, codes 32045–50137. Encoding is `round(height_m / 0.00390625 + 32768)`, decoded with `(code - 32768) * 0.00390625`. UE Z scale 50 corresponds to this mapping, about a tool-only EGM2008 zero. Representable endpoints are −128 and +127.99609375 m, not +128 m. No samples clip; maximum error is 0.001953125 m.

North-up raster pixel centres span the 4032 m working envelope at 2 m spacing. Five analytic frame fixtures pass at 1e-6 cm forward and 1e-9 m inverse tolerance; these are implementation tolerances, not approved survey budgets. No surveyed landmarks or UE orientation check passed. Confidence D and potential contamination cover all cells. Historical zero remains null in the original coordinate contract.
