# Navigation height versus actual floor — 2026-09-28

The [fresh-load traversal matrix](Traversal_Route_Matrix.json) traces vertically under **every navigation path corner** and records the hit actor, physics floor Z and `nav_z − floor_z`. This is a distinct measurement from comparing nav Z with the R16: junctions can sit on the principal slab rather than the mixed lane, and gutter/reference endpoints intentionally use open ground. It does **not** sample every Recast polygon or move a pawn.

| Intended route | Directness | Maximum absolute nav–floor gap | Corners on authored road |
|---|---:|---:|---:|
| Principal gate-side → interior | 1.000× | 14.09 cm | 2/2 |
| West mixed → junction | 1.000× | 15.63 cm | 2/2 |
| Junction → east mixed | 1.000× | 15.63 cm | 2/2 |
| Principal → mixed intersection | 1.001× | 13.08 cm | 2/2 |
| Gate-side staging approach | 1.002× | 9.85 cm | 2/2 |
| Covered-gutter crossing | 1.000× | 11.17 cm | 1/2; other corner on Landscape |
| Mixed → courtyard reference | 1.000× | 12.68 cm | 1/2; other corner on Landscape |

The open-route nav corner gap spans roughly 0.14–15.63 cm. Recast height quantisation may contribute, but this project has **no approved nav-to-floor clearance budget** and no physical pawn clearance test, so the height relationship is **measured, not passed**. The 2 m north-of-gate [threshold attempt](Gate_Threshold_Attempt.json) detoured 2.905×; the chosen staging point is about 6 m from the gate face. The original closed-door transit query remains a separate 2.583× obstruction diagnostic. See `CANTON-M05-004` and `CANTON-M05-006` in [open issues](../../Data/Open_Issues.csv).
