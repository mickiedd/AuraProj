# Wuxianmen V5 — review of both tuning passes, and a revision-3 detail pass

Date: 2026-09-18.

This record covers two things: a review of the tuning already applied to
`BP_Wuxianmen_V5_4K_Core` and `BP_Wuxianmen_V5_FullPBR`, and a further
detail/weathering pass (revision 3) on both.

[Archived diagram](2026-09-18-wuxianmen-v5-review-and-detail-pass.svg)

## Part 1 — review of revisions 1 and 2

### What revision 2 fixed, re-confirmed

| Item | Status |
| --- | --- |
| Roof un-inverted on both tiers | Confirmed: ridge `1572 cm` vs eave `1312 cm`, rise `+260 cm` (it was `−260 cm`) |
| Wood roof decks re-seated below the tiles | Confirmed: decks sit 9–42 cm below the tile tops |
| Ridge re-bound to `main_ridge` and re-axed onto X | Confirmed: bars span X 2250 / 2100 cm, Y 42 cm |
| Roof tile UV contract measured from the sheet | Confirmed: `U = 1/14.629`, `V = frac(UV · 2.8) · 0.4431` |
| Stone one-stone-per-block (Core) / magnified (FullPBR) | Confirmed: tiling `0.157` / `0.06` |
| FullPBR assembly identical to Core | Confirmed: all seven components' transform payloads match byte for byte |

Both assets were re-validated before this pass touched anything.

### What was still missing

| Gap | Evidence | Addressed in revision 3 |
| --- | --- | --- |
| **The wood material had never been touched.** One material slot, UV0 of 0..1 per unit cube, instances from 0.80 m to 22.59 m. | 451 instances; 269 exceed 1.3 m; the largest is a 22.59 m roof deck, which smears the sheet across its whole span. | Yes |
| **Stone weathering was shallow** — per-instance tone only, no staining, no moss. | The project's standing rule records that these gates read too clean against the reference, which shows dark staining and moss. | Yes |
| **The roof was one uniform surface.** | No tonal variation across 324 tile strips. | Yes |
| **Ridge cylinders reused the roof-strip contract**, which maps arbitrarily on a cylinder. | `main_ridge` is a 0.42 m cylinder 22.5 m long. | Yes |

### Suspicions that did not hold

Both of these were investigated and then deliberately **not** changed:

- **Nanite fallback was already correct.** The workflow asks for a full-triangle
  fallback with zero relative error when thin roofs, tiles or caps must stay
  visible. Measured on all seven meshes in both variants:
  `enabled = True`, `fallback_relative_error = 0.0`,
  `fallback_percent_triangles = 100.0`. Nothing to fix.
- **The eight ridge ornaments are not buried.** An earlier reading suggested the
  un-inversion had buried them. Measuring the corrected roof surface under each
  one with an adequate sampling window shows four sit 3 cm above the lower roof
  (base `12.175 m` vs roof `12.143 m`) and four sit 26 cm low inside the main roof
  (base `13.275 m` vs roof `13.537 m`). Both are within the ornaments' own 0.35 m
  size, so they were left in place. The earlier reading came from a sampling
  window too small to see the roof surface at those points.

## Part 2 — revision 3: detail and weathering

Materials and per-instance custom data only. **Every instance transform in both
assets is byte-identical to the revision-3 baseline**; 7 components and 5,103
instances preserved per asset.

### Stone

Per-instance custom data now carries three values, so the weathering is
deterministic and independent of where the asset is placed (a world-position
gradient would have made the asset weather differently in every level):

```
0 = UV offset        1 = wall height 0..1        2 = tone seed 0..1
```

Three layers were added to `M_Stone_ReferenceTuned_RefTune3`:

- **Wall-height stain** — `lerp(0.58 grey, white, saturate(height · 1.6))`, so the
  base of the wall darkens the way the reference shows.
- **Wider per-instance tone** — `0.72 .. 1.22`, up from `0.85 .. 1.15`.
- **Moss** — a restrained `0.38` blend toward `0.42 / 0.50 / 0.30`, confined to
  the lower band by `saturate(1 − height · 2.2)`.

Measured inputs: wall height spans `0.0031 .. 0.9476` across 4,213 instances, so
the stain and moss gradients are exercised over their full range.

### Wood

`M_Wood_ReferenceTuned_RefTune3` derives its tiling from each instance's own size,
which is what the measurement demands — no single tiling serves both a 1.3 m
column and a 22.5 m roof deck:

```
tiling = 1 + custom_data · 16      custom_data = (instance_size / 1.3 − 1) / 16
```

269 instances tile above 1.0 (up to 17×); the rest are unchanged. The tint moved
from `0.43 / 0.27 / 0.17` to `0.78 / 0.56 / 0.40`, because the timber had been
reading as flat dark maroon. FullPBR's wood sheet is pattern-free, so it takes a
fixed `0.3` tiling that reads as soft grain instead of a per-instance one.

### Roof tile and ridge cap

- `M_RoofTile_ReferenceTuned_RefTune3` adds a per-instance tone of `0.86 .. 1.16`,
  breaking the uniform roof into weathered patches.
- `M_RidgeCap_ReferenceTuned_RefTune3` is new and dedicated: `U 5.127` along the
  cap axis (75 tiles, 0.30 m each) and `V 0.4646` around it, instead of reusing
  the roof-strip contract.

## Tests

- `python Scripts/remote_run.py Scripts/SnapshotV5WuxianmenRefTune3.py` — PASS.
  Baselines written for both variants before mutation.
- `python Scripts/remote_run.py Scripts/FixV5WuxianmenRefTune3.py` — PASS for both
  variants. `instance_total` 5103 each; `transform_payload_unchanged: true`; stone
  4,213 instances × 3 floats and tile 324 instances × 1 float written; wood 451
  instances × 1 float for Core.
- `python Scripts/remote_run.py Scripts/ValidateV5WuxianmenRefTune3.py` — PASS for
  both variants. Transforms byte-identical to the baseline; component count,
  instance count, collision profile, visibility and shadow flags unchanged;
  materials bound as expected on stone, wood, tile and ridge; roof rise `+260 cm`
  retained.
- `Scripts/AuditV5WuxianmenRefTune3.py` — the measurements quoted above.
- `python -m py_compile` on all new scripts — PASS.

## Known limits and open items

- **No perspective review was possible for this pass.** The deferred high-res
  screenshot is still writing one stale frame for every requested view, in both
  variants. Already ruled out: camera readback, missing transient actors,
  `editor_invalidate_viewports()`, a 10 s screenshot delay, `force_game_view`
  off, an explicit `CameraActor` argument, and a `SceneCapture2D` +
  `render_target` export. The editor window not being in the foreground remains
  the leading cause, and Python cannot toggle that throttle. The revision-3
  materials are therefore measured and numerically validated but **not visually
  confirmed**.
- **The plaque reading direction is still unconfirmed** and needs a front-on
  capture.
- Per-instance custom data is write-only from Python in this engine version
  (there is no `get_custom_data_value`), so the payload is verified by its channel
  size, by the variation of the data it was derived from, and by the write counts
  in the fix report — not by reading it back.
- The moss and stain strengths were chosen from the reference's appearance, not
  fitted to a measurement, and will want one visual pass once captures work.
- Cooked-build performance, platform memory and collision cooking remain separate
  checks.
