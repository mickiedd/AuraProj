# Canton Days 11–20 provisional district

**Intent.** Continue the 20-working-day terrain plan past M01 without treating an unsurveyed city map or modern DTM as verified late-Qing geometry.

**Changed behavior.** The M01 World Partition map remains a separate provisional technical baseline. A new `/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL` map contains a 200 × 200 m engineering sample: classified road and parcel-reference data, 100 principal slabs, 76 mixed-lane pieces, three covered-gutter flow reaches, six Landscape paint targets, sparse vegetation and dampness proxies, a scale-only Wenmingmen Blueprint, a test PlayerStart and navigation bounds. The imported base layer stays locked; three separate edit layers are available for later evidence-backed corrections and remain unmodified. All authored period-independent layout is tagged D-confidence and the diagnostic XY/Z is explicitly excluded from historical acceptance.

The first visual pass exposed stepped slabs and floating parcel pads. The parcel pads were removed in favor of non-rendered GIS references. Road pieces now use bilinear R16 center and edge samples, local slope rotations and explicit saves for all 476 changed World Partition external-actor packages. A second edge probe found the roll sign reversed; correcting it reduced the worst long-edge deviation from 11.31 cm to 0.024 cm.

A close dry/wet comparison exposed pale damp cards floating above the terrain. The 14 cards were re-saved as thin, dark, non-shadowing ground overlays with a 0.5 cm bottom offset; their fixed camera now shows the local toggle clearly.

**Validation.** `./BuildEditor.command` passed. Source inventory, heightmap contract, UE import contract, M01 readiness and district contracts pass; the M01 native UE automation passed again. A fresh UE reload validates 100/100 principal road center heights and collision traces, 0 cm maximum center error, 0.024 cm maximum long-edge error, six target paint layers and no broad parcel pads. Map check reports zero errors and warnings. `BuildPaths` plus a fresh navigation query returned a complete 145.01 m non-partial route. The targeted Mac Development cook completed 994/994 packages and emitted both Canton maps. Fixed-camera overview, gate, road, courtyard and paired dry/wet captures are retained under `QA/Canton_District/`. The Development fixed-route performance gate remains blocked and is reported in `Review/M05_Final_Handoff.md`.

**Limitations.** H1 city controls, H2 Wenmingmen local survey checks, a dated benchmarked walking surface and Day-01 owner approval remain unavailable. The road layout, colors, cube-based slab modules, plant proxies and weathering are engineering blockout. A Development or packaged fixed-route performance acceptance run is still required. See `Data/Open_Issues.csv`.

Illustration: [Canton Days 11–20 provisional district](2026-09-27-canton-days-11-20-provisional-district.svg).
