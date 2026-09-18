# Guidemen V5 4K — inverted roof shells corrected

Date: 2026-09-18.

The user reported that `BP_Guidemen_V5_4K`'s roof was still inverted, after the
previous pass had concluded it was correct. The user was right and the earlier
conclusion was wrong.

[Archived diagram](2026-09-18-guidemen-roof-shell-inversion-fix.svg)

## What the earlier pass got wrong

The first probe correlated the top surface Z against distance from the **whole roof
group's** centre and read a clean fall from 2,002 cm at the centre to 1,465 cm at the
perimeter, which was taken as a correct ridge.

That measurement is confounded on a two-tier roof: the upper tier sits near the centre
and higher, the lower tier reaches further out and lower, so the aggregate falls with
radius **even when each tier's own slope is inverted**. A cross-tier correlation is
not a roof test.

## The diagnosis

Per-tier measurement separated the tiers, but the slope of the shells still could not
be read from instance AABBs. The decisive evidence turned out not to be the slope at
all — it was where the **ridge bars** sit.

A ridge bar must cap the roof's highest line. Guidemen's ridge bars sit exactly on
their tier's tile apex, and the tile surface rises to that apex, so the tiles and the
ridges agree with each other. The eight roof shells do not agree with either:

| tier | shells spanned | ridge bar | tile surface | shells sat |
| --- | --- | --- | --- | --- |
| R1 | 1462 – 1634 | 1472 – 1500 | 1292 – 1472 | **342 cm above the tiles at the eaves** |
| R2 | 1855 – 2002 | 1864.5 – 1891.5 | 1710 – 1865 | **342 cm above the tiles at the eaves** |

Sheathing cannot sit above its own tiles. That is the inversion the user saw.

Reflecting each shell about the horizontal plane at its own minimum Z — its
ridge-side edge — makes it lie 2 cm under the tiled surface on **both** tiers. A
2 cm clearance on both tiers is the authored relationship, not a coincidence.

## The fix

Eight instances. Per shell: a 180° rotation about the world X axis through the pivot
`(y = 0, z = that shell's own minimum Z)`. A rotation rather than a Z mirror, so
triangle winding and shading normals stay valid.

The front/back pair and the left/right pair are each mirror-symmetric about `y = 0`,
so the rotation's incidental y flip maps each shell onto its partner's footprint
exactly, and each hip end's slope in X is flipped without moving it.

**Deliberately not touched:**

- The tiles and both ridge bars — they are the surfaces that already agreed with each
  other.
- The 12 roof ornaments (`Beast_0_-1_0`): measured at z 1494–1558 and 1886–1950, i.e.
  seated on the ridge bars, so they never needed moving.

## Result and validation

| tier | shells now | tile surface | clearance below tile top | offset above tile base |
| --- | --- | --- | --- | --- |
| R1 | 1290 – 1462 | 1285.04 – 1471.89 | 9.89 cm | 4.96 cm |
| R2 | 1708 – 1855 | 1703.14 – 1864.86 | 9.86 cm | 4.86 cm |

All four shells within a tier report identical values, and both tiers land within
3 mm of each other.

- Each shell lies under its tier's tiled surface with a positive clearance at the top
  and a small positive offset at the base, and its span matches the tile span within
  tolerance — asserted for all eight.
- Exactly the 8 shells changed; the other **39 components' transforms are
  byte-identical**, including the tile component and both ridge bars. 18,822 instances
  preserved; materials, collision, visibility and shadow flags intact.
- The tile surface itself is unchanged.

## Method note

The shell meshes are not modelled with world-aligned local frames — each shell's
local `+Z` maps to a world horizontal axis (front `−Y`, back `+Y`, left `−X`,
right `+X`) — so an orientation-based slope test is unavailable on them. Vertex
readback is also blocked: `VertexID` cannot be constructed from Python
(`get_vertex_position` requires one, and the struct's `id` property is not settable).

The evidence is therefore the geometric relationship — nesting under the tiles with a
uniform clearance — which the previous state failed by 342 cm.

## Known limits

- **No perspective review was possible.** The deferred high-res screenshot still
  writes one stale frame per requested view. This correction is verified numerically.
  That is also why the user, not the tooling, caught the original error.
- The earlier cross-building audit's conclusion that "no building outside Wuxianmen
  has an inverted roof" was **wrong for Guidemen**. The other five buildings were
  assessed with the same confounded metric and should be re-checked per tier before
  that conclusion is trusted. Zhengnanmen HighFidelity's own per-instance figures
  (centre 1,660 cm vs perimeter 1,286 cm across 7,086 roof instances) are consistent
  with a correct ridge, but the check should be repeated properly.
