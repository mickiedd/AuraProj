# Wuxianmen V5 Core reference tuning, revision 2

Date: 2026-09-17.

Applied the asset-reference tuning workflow to `BP_Wuxianmen_V5_4K_Core` against
the canonical Wuxianmen reference sheet (`Docs/Reference/GateSheets/Wuxianmen-reference.png`).
The audit found that the imported assembly's roof was inverted, its ridge bars
were rotated onto the wrong axis, and three materials were tiling at the wrong
scale. This record covers that delta.

[Archived diagram](2026-09-17-wuxianmen-reference-tuning-revision-2.svg)

## What was wrong

Measured from the source assembly, not inferred from renders:

| # | Defect | Evidence |
| --- | --- | --- |
| 1 | **Both roof tiers were inverted.** Tile strips and both wood roof decks rose from `z 12.75 m` at `y = 0` to `z 15.72 m` at `\|y\| = 4.5 m` — a valley, not a ridge. | Per-band vertex heights of every tile and deck instance. The top view rendered a funnel with four faces converging inward. |
| 2 | **Both ridge bars ran on the wrong axis.** `main_ridge` was `0.42 × 22.5 × 0.42 m` along **Y** while the roof pitches along Y and is only 8.7 m deep there, so 6.9 m protruded past each eave. `lower_ridge` protruded 6.15 m. | World extents `[0.42, 22.5, 0.42]`, y span ±11.25 vs roof ±4.35. These were the long blades in every earlier capture. |
| 3 | **The ridge role was bound to a roof-tile mesh** and `main_ridge.uasset` was imported but never bound. | `HISM_004_ridge_000010` → `main_tile_-1_000`; the rebind pass had filtered `main_ridge` out of the tile-role candidate list. |
| 4 | **Stone tiling was inverted.** The sheet holds 6.36 stone periods per unit; the material tiled by 6.36, shrinking each stone to about 13 mm inside a 0.56 m block, so the wall averaged out to flat pale brickwork. | Autocorrelation of `Stone_BaseColor_4K_Repaired.png`; the render showed no per-block stone detail. |
| 5 | **Roof tile tiling was wrong on both axes.** The strip is one tile wide and one slope long. | The sheet holds 14.63 columns per unit U and 5.24 courses per authored region. |
| 6 | **The plaque had no texture at all** — the material was a flat gold tint, so the 五仙門 sign was unreadable. | `packages.json` maps no BaseColor for `M_Plaque_Wuxianmen`. |

## Changed behaviour

Geometry — 328 of 5,103 instance transforms, all in three components:

- **Un-inverted both roof tiers** with a 180° rotation about the world X axis
  through each tier's mid-plane (`main` pivot `z = 1423.5 cm`, `lower` pivot
  `z = 1251.5 cm`). A rotation rather than a Z mirror, so triangle winding and
  shading normals stay valid. Both tiers are symmetric in y, so their footprints
  are unchanged.
- **Re-seated the wood roof decks.** A Z mirror reverses vertical stacking, which
  lifted the decks 28.6 cm (main) and 16.2 cm (lower) above the tiles and hid
  them. The decks are mirrored about a pivot 20 cm / 15 cm lower
  (`1403.5 / 1236.5 cm`), which seats them 9–42 cm below the tile tops.
- **Re-bound `HISM_004` to `main_ridge`**, turned both ridge bars 90° about Z to
  run along X, and seated them on the corrected apex at `z = 1572 / 1338 cm`.

Materials:

- New private `M_RoofTile_ReferenceTuned_RefTune2`: `U` tiling `1/14.629 = 0.0684`
  (one tile column across the strip's 0.359 m width) and
  `V = frac(UV · 2.8) · 0.4431` (14 courses over the 5.068 m strip, 0.362 m
  pitch, wrap landing on a course boundary so no seam shows).
- New private `M_Stone_ReferenceTuned_RefTune2`: tiling `1/6.36 = 0.157` so each
  0.56 m block shows one stone, plus a per-instance custom data value
  (`num_custom_data_floats = 1`, 4,213 values assigned) that offsets each block's
  UV and varies its tone by ±15%, so the wall does not repeat. Tint moved from
  `0.61/0.58/0.51` to `0.53/0.50/0.44`.
- New private `M_Plaster_ReferenceTuned_RefTune2`: tint `0.72/0.66/0.56` →
  `0.62/0.57/0.49`.
- New private `M_Plaque_Wuxianmen_RefTune2` sampling a generated plaque board
  texture imported to `Textures/Wuxianmen_Plaque_BaseColor_2K`.

Preserved: component count (7), instance count (5,103), the other 4,775 instance
transforms, collision profiles, visibility, shadow flags, mesh assignments for
every component except the ridge, and all prior rollback materials.

## Tests

- `python Scripts/remote_run.py Scripts/FixV5WuxianmenRefTune2.py` — PASS. Every
  corrected instance box equals the Z mirror of an original box about that part's
  pivot: 168 + 156 tiles and 2 + 2 decks, 0 unmatched. Ridge bars measured at
  X span 2250 / 2100 cm, Y span 42 cm.
- `python Scripts/remote_run.py Scripts/ValidateV5WuxianmenRefTune2.py` — PASS.
  Live transient spawn: 7 components, 5,103 instances, bounds
  `3646.6 × 1813.3 × 1721.0 cm`, roof ridge `1572 cm` vs eave `1312 cm`
  (rise `+260 cm`; it was `−260 cm`), transient actor destroyed.
- Baseline diff (`Scripts/DiffV5WuxianmenBaselineRefTune2.py`) — the four
  untouched components match the recorded baseline exactly, and inside the three
  repaired components only the intended instances differ (168 + 156 tiles, 2 + 2
  decks, 2 ridge bars).
- `python -m py_compile` on all eight new scripts — PASS.
- 12 transient capture views reviewed; no map saved.

## Visual evidence

- Before: [Core hero capture](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reference-tuning-hero.png)
  and [Core top view](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reference-tuning-top.png)
  — the roof dips to a funnel and the ridge bars project as blades.
- After: [hero](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-hero.png),
  [roof side](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-roof_side.png),
  [roof close](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-roof_close.png),
  [wall close](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-wall_close.png),
  [top](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-top.png).

## Known limits

- The source roof deck and tile surfaces have slightly different pitches
  (0.619 vs 0.660), so the gap between them varies from 9 cm to 42 cm. Closing it
  exactly would need the tile strips re-laid, which this pass did not do.
- The eight small ridge ornaments were left in place; their intended role is
  ambiguous and they are 0.35 m on a 36 m building. They are reported as
  unchanged rather than moved.
- The ridge caps remain simple cylinders from the source; the reference shows a
  proper 正脊 with ridge tiles. Remodelling it is out of scope here.
- Cooked-build performance, platform memory and collision cooking remain separate
  checks.
