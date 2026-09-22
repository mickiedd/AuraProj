# Canton terrain plan hardening

## Intent

Make the 20-working-day historical Canton terrain plan executable and auditable without changing its prototype scope.

## Changed behavior

- Georeference acceptance now uses predeclared numeric tolerances and independent check points.
- Modern measurement quality is separated from historical relevance and reconstruction confidence.
- Day 01 now covers engine/plugin readiness, data licensing/storage, checksums and target performance criteria.
- Landscape Edit Layers are enabled and the imported base is locked during Day 09 import.
- The gate district is selected by Day 05, with a ranked fallback.
- The 2 m Landscape grid is explicitly limited to broad grading; small street features use higher-resolution geometry.
- Python contracts, UE automation, map check, cook and fixed-route performance evidence are required at the relevant gates.

## Validation

- Rechecked the 2017-vertex / 2016-quad / 2 m-spacing / 4032 m Landscape arithmetic.
- Checked all Day 01–20 references to confidence, edit layers, PCG, performance and automated validation for consistent sequencing.
- Verified the plan remains a 20-working-day prototype and evidence audit rather than a promise of a complete city.

## Illustration

[Plan hardening flow](2026-09-22-canton-terrain-plan-hardening.svg)
