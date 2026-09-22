# Wuxianmen final finial root seating

Date: 2026-09-22. [Visual summary](2026-09-22-wuxianmen-roof-seat-root-fit.svg) · [Corner capture](2026-09-22-wuxianmen-roof-seat-corner.png) · [Side capture](2026-09-22-wuxianmen-roof-seat-side.png).

## Scope and intent

The first roof-root pass seated the finial geometry numerically, but the close camera still made its first segment read detached because the start point was hidden inside the fascia. This follow-up tightens the start point to the fascia plane while preserving the already seated ridge caps.

## Changed behavior

- Replaced the prior private gable mesh with `SM_ClosedGablesAndFinials_RoofSeat20260922`.
- Moved all eight finial root points onto the gable fascia plane at the roof corners and continued the curves outward with a small overlap margin.
- Retained the 20 cm downward ridge seating from the prior pass; the saved ridge centers remain Z 1188 cm and 1692 cm.

## Validation

- `roof-seat-geometry-validation.json`: all eight roots coincide with tile vertices and are within 6.09 cm of the fascia.
- `roof-seat-live-validation.json` and `roof-seat-fresh-validation.json`: saved mesh/material/instance bindings and serialized reload pass.
- `roof-seat-explicit-save.json`: the Blueprint and final private mesh are saved to local `.uasset` files.
- `roof-seat-corner.png` and `roof-seat-side.png` were reviewed from the close angles used to diagnose the detached parts. No map was saved.

Evidence packet and scripts: `C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c4cc-cb1b-78b1-bd83-f6c5a46f6e08/wuxianmen`.
