# Landscape extent check — 2026-09-24

**Provisional geometric fit; historical gate blocked.** `Scripts/build_canton_provisional_layers.py` derives this from the coarse wall trace and failed Day 03 affine. `Data/Extent_Provisional.json` is the machine-readable result.

| EPSG:32649 envelope | West | South | East | North |
|---|---:|---:|---:|---:|
| Coarse wall | 730165.7 | 2557901.7 | 732791.4 | 2560859.3 |
| Wall plus 200 m | 729965.7 | 2557701.7 | 732991.4 | 2561059.3 |
| Proposed 4032 m Landscape | 729400 | 2557300 | 733432 | 2561332 |

Clearance from the wall is west **765.7 m**, south **601.7 m**, east **640.6 m**, north **472.7 m**. The proposed 2017-vertex, 2 m grid fits this **provisional** trace with ≥200 m margin. Lower-left working origin: E729400/N2557300, pending approval. Redo the calculation with accepted wall geometry before locking Landscape extent.
