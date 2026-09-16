# V4 reference-conformance review & next-pass requirements (2026-09-15)

## Purpose

Review the Codex V4 landmark import + tuning result against the four supplied reference
sheets, and hand back concrete, testable requirements for the next tuning pass.

**Reviewed:** `Dadongmen`, `Guidemen`, `Wuxianmen`, `Zhengximen` under
`/Game/Assets/Environment/GuangzhouLandmarks/V4/`.
**References:** the four `codex-clipboard-*.png` sheets recorded in each
`*-reference-tuning.json` (`5ea059eb…` Dadongmen, `58990fa0…` Guidemen, `c9737a14…`
Wuxianmen, `f1f30b9a…` Zhengximen).
**Renders compared:** `<name>_V4-front.png`, `-rear.png`, `-close.png` in
`Saved/RawModelImport/V4/`, plus the source package previews in
`Dadongmen_GreatEastGate_UE5/Previews/`.

[Visual summary](2026-09-15-v4-reference-conformance-review.svg)

## Headline finding

**The models are structurally faithful but tonally inverted against the reference.**

Every building has the right massing — battered crenellated wall, deep arched gate,
multi-storey timber pavilion with a double-eave tiled roof, balcony railing and lattice
windows (confirmed against `Previews/02_Front_Elevation.png`, which shows the full
pavilion that the 3/4 clay preview hides). So the gap is **not** missing geometry.

The gap is that the tuned materials render **much darker and flatter than the reference
sheets**. The reference posters are bright daylight renders: light weathered stone with
dark mortar lines, mid-grey clay tiles, warm timber. The tuned buildings read as dark
olive/tan slabs with near-black roofs.

## Quantitative confirmation

Measured on the shipped renders with `Saved/RawModelImport/V4/analyze_tone_gap.py`
(Pillow, read-only). Because the render background is a smooth sky gradient plus a large
flat ground plane, a plain crop average measures mostly background — so the script masks
to *textured surface* pixels (local deviation ≥ 6 grey levels after a radius-2 box blur)
and reports statistics on those only.

| Building | view | surface mean | p10 | p90 | stdev | textured fraction |
|---|---|---|---|---|---|---|
| Dadongmen | close | 0.2465 | 0.1216 | 0.3373 | 0.0957 | 25.2 % |
| Dadongmen | **reference** | 0.5050 | 0.2471 | 0.7686 | 0.1901 | 73.5 % |
| Guidemen | close | 0.3865 | 0.1373 | 0.6745 | 0.2000 | 17.5 % |
| Guidemen | **reference** | 0.4905 | 0.1529 | 0.8078 | 0.2431 | 67.5 % |
| Wuxianmen | close | 0.3627 | 0.1647 | 0.5333 | 0.1449 | 16.4 % |
| Wuxianmen | **reference** | 0.3786 | 0.1490 | 0.6863 | 0.2044 | 71.9 % |
| Zhengximen | close | 0.3573 | 0.1059 | 0.5412 | 0.1697 | 20.5 % |
| Zhengximen | **reference** | 0.4396 | 0.1843 | 0.7176 | 0.2046 | 65.3 % |

Gap versus reference:

| Building | mean ratio | contrast (stdev) ratio |
|---|---|---|
| Dadongmen | **0.49×** | **0.50×** |
| Guidemen | 0.79× | 0.82× |
| Wuxianmen | 0.96× | 0.71× |
| Zhengximen | 0.81× | 0.83× |

Two conclusions hold. **All four renders are darker and flatter than their reference**,
with Dadongmen at roughly half the brightness *and* half the contrast. And the rendered
building region is only **16–25 % textured surface against 65–74 %** in the reference —
the surfaces are predominantly flat fields rather than reading material.

*Caveat, stated plainly:* the reference crops include surrounding street scene, boats and
foliage, which are highly textured, so the absolute textured-fraction figures flatter the
reference and are not strictly like-for-like. The **mean and contrast ratios are the
defensible numbers**; the textured-fraction column is directional evidence only.

## Root cause

Every tuned material is built as:

```
BaseColor = texture_BC  ×  Constant3Vector(tint)
```

and the tints are all mid-grey (≈0.28–0.78). Multiplying a mid-grey constant into an
already-correct albedo texture roughly **halves the brightness and compresses the
luminance range**, so surface detail (mortar courses, tile ridges, plank grain) collapses
into a narrow band and stops reading. The roofs suffer most:

| Building | Roof material | Tint | Effect in render |
|---|---|---|---|
| Guidemen | `Tile_ClayGrey` | (0.31, 0.34, 0.38) | reads black |
| Wuxianmen | `ClayRoof` | (0.31, 0.35, 0.39) | reads black |
| Zhengximen | `ClayRoofTile` | (0.34, 0.37, 0.41) | reads black |
| Dadongmen | `RoofClay` | (0.44, 0.48, 0.53) | dark, tile courses not readable |

The same multiply flattens the walls: Guidemen `Stone_BlueGrey` (0.56, 0.52, 0.45) leaves
the masonry nearly uniform, and the close-up shows no readable block coursing.

## Per-building conformance

### Dadongmen — Great East Gate
Reference shows a bright, weathered grey-stone battered wall, a deep arch, and a
two-storey pavilion with prominent bracket sets and grey clay tile roof.

- **F1** Wall reads dark grey-brown with the mortar joints washed out; reference is much
  lighter with strong joint contrast.
- **F2** **Floating green vegetation shards** hover detached in front of the wall in
  `Dadongmen_V4-close.png`. These are source vegetation elements rendered with the flat
  `M_Dadongmen_Vegetation` constant (no texture maps at all — see Q5 in the earlier
  review). They are the single most obvious visual defect in the whole set.
- **F3** The door-detail pass added flat black cube/sphere components inside the arch
  (`Dadongmen_V4-front.png` shows two plain black slabs). They carry a near-black constant
  material, so the "readable wooden door detail" is unreadable and looks worse than the
  source's own door leaves.
- **F4** `hide_source_ground_material()` deleted every expression from the tuned Dirt and
  Water materials and forced opacity to 0, so the source ground slabs now render as a dark
  void. The replacement `Approach_Path` is a flat brown quad that reads as a pasted
  rectangle, not ground.
- **F5** `Dadongmen-detail-tuning.json` reports `"geometry_changed": false` while the same
  run added ~50 components and rewrote two materials. The manifest is wrong.

### Guidemen — Guide Gate
Reference shows a deep barrel-vaulted passage, a large two-storey pavilion with a
double-eave roof and signboard, and a prominent carved 歸德 stone inscription over the arch.

- **F6** Roof reads black; reference is mid-grey clay tile with readable tile courses.
- **F7** Wall is flat olive-tan with essentially no visible masonry in the front view;
  reference shows varied weathered stone blocks with strong joint shadowing.
- **F8** The 歸德 inscription is a small pale blank plaque with no legible carved
  characters. It is a named, surviving-heritage feature in the reference and should read.
- **F9** The pavilion is proportionally smaller than the reference, where it dominates the
  wall. (Geometry, not material — flag for a geometry decision, not a fix.)

### Wuxianmen — Gate of the Five Genii
Reference shows a stone gate on the waterfront with a single-eave hall, a large 五仙門
signboard, and a stone voussoir arch surround.

- **F10** **Two stray objects float in the sky** above the building in
  `Wuxianmen_V4-close.png` — detached geometry, same class of defect as F2.
- **F11** The wall texture is an obvious **repeating tile grid**, reading as a synthetic
  pattern rather than stone. The UV scale needs to change so the repeat is not legible at
  building distance.
- **F12** The hall is far smaller than the reference relative to the wall.
- **F13** Signboard is present but small and low-contrast.

### Zhengximen — Great West Gate
Reference shows irregular stone-block masonry with weathering, a deep arch, and a
two-storey pavilion with 正西門 plaque.

- **F14** The wall renders as **dense horizontal stripes**, like a stack of thin slats.
  The archive describes this as "Zhengximen's procedural horizontal wall courses/UV
  repetition" being preserved — but at render scale it reads as a banding artifact, not
  masonry. This is the worst material defect after the floating geometry.
- **F15** The flanking wall segments are flat tan with no texture at all.
- **F16** Roof reads black.

### Cross-cutting

- **F17** **LOD1–LOD3 still bind the untuned original materials** on all four buildings,
  because the tuned rebind only covered the primary LOD0 mesh and the validator assertion
  was narrowed to `entry['source']['primary']` to make it pass. Every shipped alternate LOD
  is now visually inconsistent with the blueprint.
- **F18** Byte-identical textures are still imported as separate assets (Wuxianmen 6×
  `*_Metallic_4K`, Zhengximen 7×, Guidemen 4×, Dadongmen 3×).
- **F19** All four tuning manifests cite a `%TEMP%\codex-clipboard-*.png` path as their
  reference. Those files are purgeable, so no tuning can be re-derived later.
- **F20** Validation heights are not reproducible between the live editor and a headless
  commandlet (Dadongmen 2248.0 vs 2138.5 cm), and `get_actor_bounds` disagrees with the
  mesh's own bounds, so the orientation guard is not measuring what it claims.

## Requirements for the next tuning pass

Each item is written to be independently verifiable.

### R1 — Rebuild the material graph around a neutral base, not a mid-grey multiply
Replace `BaseColor = tex_BC × Constant3Vector(tint)` with a **luminance-preserving**
correction. Either connect the base colour texture directly and drive tinting through a
`Desaturation`/`Hue-shift`/`Contrast` chain, or multiply by a constant normalised so its
mean is 1.0 (e.g. a colour whose components average ≈1.0), so the texture's authored
brightness survives.
**Measured baseline:** surface mean ratio vs reference is 0.49× (Dadongmen), 0.79×
(Guidemen), 0.96× (Wuxianmen), 0.81× (Zhengximen).
**Acceptance:** for every tuned material, `MP_BASE_COLOR`'s mean output luminance must be
within ±15 % of the original material's, measured on the same texture. Re-run
`analyze_tone_gap.py` afterwards: every building's surface-mean ratio must land in
**0.85–1.15**.

### R2 — Fix roof tone
All four roofs currently read black. Target the reference's mid-grey clay tile.
**Measured baseline:** the render `p90` for Dadongmen's close-up is 0.3373 against 0.7686
in the reference — the brightest 10 % of the building never reaches mid-grey.
**Acceptance:** in a front-elevation capture under the preview lighting, the roof region's
mean pixel luminance is **≥ 0.35** and tile courses are visually distinguishable at the
`-close` capture distance. Roof materials: Guidemen `Tile_ClayGrey`, Wuxianmen `ClayRoof`,
Zhengximen `ClayRoofTile`, Dadongmen `RoofClay`.

### R3 — Restore wall masonry readability
**Measured baseline:** surface contrast (stdev) ratio vs reference is 0.50× (Dadongmen),
0.82× (Guidemen), 0.71× (Wuxianmen), 0.83× (Zhengximen).
**Acceptance:** in `-close.png` for each building, at least 6 distinct horizontal mortar
courses are countable across a 3 m span of wall, the wall's luminance standard deviation is
**≥ 0.06**, and its surface-contrast ratio vs reference is **≥ 0.85**. Specifically address
F1, F7, F11, F14, F15.

### R4 — Remove the floating geometry
Delete or re-seat the detached vegetation / ornament elements that hover away from the
surface (F2 Dadongmen, F10 Wuxianmen).
**Acceptance:** no geometry element is more than 5 cm from the surface it belongs to;
verify by bounding-box distance to the nearest wall/pavilion surface, and by a fresh
`-close.png` with no free-floating fragments against the sky.

### R5 — Give the Dadongmen vegetation a real material, or remove it
`M_Dadongmen_Vegetation` has no texture maps and is a flat green constant, which is why the
shards read as bright green crystals (F2).
**Acceptance:** the material either samples a real moss/foliage texture, or the geometry is
removed. No flat-constant green surface may remain.

### R6 — Fix the Zhengximen wall banding
The horizontal striping (F14) must not read as banding at render scale.
**Acceptance:** in `Zhengximen_V4-close.png`, the wall shows irregular block masonry; the
horizontal-stripe spatial frequency is no longer visually dominant. Re-scale or re-map the
UVs rather than leaving the source repetition.

### R7 — Fix the Wuxianmen wall tiling
**Acceptance:** in `Wuxianmen_V4-close.png`, no repeating grid motif is identifiable across
the wall; the texture repeat count across the visible wall is **< 4**.

### R8 — Restore the Dadongmen ground, without destroying materials
`hide_source_ground_material()` blanked the tuned Dirt/Water materials to opacity 0 (F4).
**Acceptance:** hide the ground **components** (visibility/hidden-in-game) instead of
rewriting the material graph; `M_Dadongmen_Dirt_ReferenceTuned` and
`M_Dadongmen_Water_ReferenceTuned` must retain their texture-driven base colour, and the
replacement approach ground must read as ground, not a flat pasted quad.

### R9 — Make the Dadongmen door detail read as timber
The added door components are flat black (F3).
**Acceptance:** the door leaves sample the wood texture (or a planked wood material with
visible grain), and in `Dadongmen_V4-front.png` the door region's mean luminance is
**≥ 0.15** with visible plank/brace separation.

### R10 — Rebind every LOD to the tuned materials
(F17) Rebind LOD1–LOD3 on all four buildings to the matching `*_ReferenceTuned` materials,
then **restore** the validator assertion to cover every non-collision mesh.
**Acceptance:** `ValidateV4Buildings.py` asserts the tuned-material binding for **all**
LOD meshes, not only `entry['source']['primary']`, and passes.

### R11 — Legible heritage inscription (Guidemen)
(F8) The 歸德 inscription is a named surviving-heritage feature.
**Acceptance:** the inscription plaque renders with legible carved characters at the
`-close` capture distance, or the limitation is explicitly recorded as a known gap rather
than left as a blank plaque.

### R12 — Correct the manifests
(F5, F19) `geometry_changed` must be `true` whenever components or geometry are added.
**Acceptance:** every `*-tuning.json` accurately reports whether geometry changed, and
copies the reference image **into the project** (e.g. `Saved/RawModelImport/V4/Reference/`)
instead of citing a `%TEMP%` path.

### R13 — Deduplicate shared textures
(F18) **Acceptance:** where two materials reference byte-identical source maps, they share
one imported `Texture2D` asset. Report the asset-count reduction.

### R14 — Make the height measurement trustworthy
(F20) **Acceptance:** the same validation run reports the same building height in the live
editor and in a headless commandlet, to within 1 cm; the orientation guard asserts on a
value that matches the mesh's own bounds.

## Suggested order

R1 → R2/R3 (tone and readability, biggest visual return) → R4/R5/R6/R7 (defect removal) →
R9/R8 (Dadongmen detail) → R10/R12/R13/R14 (integrity) → R11 (heritage detail).

## Note on the observed run

Codex completed all four imports, all four reference tunings, and a Dadongmen door-detail
pass, each with an archive record. At the time of writing it had spent over an hour in a
non-converging capture-framing loop (`CaptureDadongmen*` scripts, ~20 iterations,
07:24–08:37) without settling the render. That loop is itself a finding: the capture
harness needs a deterministic camera (fixed transform + fixed FOV, one screenshot per
process) rather than the current trial-and-error search.
