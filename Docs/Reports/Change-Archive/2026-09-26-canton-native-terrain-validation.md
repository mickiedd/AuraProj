# Canton native terrain validation — 2026-09-26

Intent: resolve the technical blockers in days 01–10 and continue their provisional implementation without fabricating missing historical evidence.

Before: only an isolated PNG/raster pipeline existed, and no native Landscape, edit-layer, collision or map-load evidence was available. After: an editor-only native importer creates a saved World Partition Landscape in `/Game/Canton/Provisional/`; row order and overlay CRS conversion are explicit; Base_Imported is locked; the map and its external actor packages are included in the repository scope. A separate engineering contract makes the prototype executable while leaving the historical contract unchanged.

Validation: AuraEditor build succeeds; Python source/encoding/import tests pass; 256 terrain and 256 collision components survive reload; seven asymmetric imported height samples agree exactly; maximum collision height error is 0.0156879425 cm. `Aura.Canton.Provisional.MapConfiguration` succeeds with no test warnings/errors. Three native captures were visually inspected after exposure adjustment. World-creation lifecycle, macOS name collision and module dependency errors encountered during implementation were corrected and rerun.

Historical M01 remains Blocked: unsurveyed map controls and missing datum-backed late-Qing ground Z are not repairable as code errors. Every raster cell and overlay remains provisional. The focused source follow-up and remaining evidence request are in the review packet. Days 11–20 were not executed.

[Diagram](2026-09-26-canton-native-terrain-validation.svg) · [M01 review](../../../Review/M01_Terrain_Review.md) · [Execution](../../../QA/Canton_Days_01_10_Execution.md) · [Native package hashes](../../../Data/M01_Native_Artifacts.csv)
