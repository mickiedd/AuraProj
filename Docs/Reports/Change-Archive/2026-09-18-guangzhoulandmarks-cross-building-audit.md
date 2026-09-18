# GuangzhouLandmarks cross-building audit and tuning

Date: 2026-09-18.

Inspected every building under `Content/Assets/Environment/GuangzhouLandmarks/`,
then tuned what the inspection found. Seven building Blueprints are in scope, in
two different component architectures.

[Archived diagram](2026-09-18-guangzhoulandmarks-cross-building-audit.svg)

## What was inspected

| Building | Architecture | Instances | Extent (cm) |
| --- | --- | --- | --- |
| `BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset` | 114 HISM | 12,383 | 2700 × 1705 × 2303 |
| `BP_Xiaobeimen_AAA_V3` | 51 StaticMeshComponent | 51 | 3040 × 3166 × 2253 |
| `BP_Xiaobeimen_Production_V3` | 16 StaticMeshComponent | 16 | 3260 × 3089 × 2218 |
| `BP_Zhengnanmen_AAA_V3` | 25 StaticMeshComponent | 25 | 2560 × 1580 × 2250 |
| `BP_Guidemen_V5_4K` | 47 HISM | 18,822 | 5639 × 1800 × 2026 |
| `BP_Wuxianmen_V5_4K_Core` | 7 HISM | 5,103 | 3647 × 1813 × 1594 |
| `BP_Wuxianmen_V5_FullPBR` | 7 HISM | 5,103 | 3647 × 1813 × 1594 |

A first audit pass only understood HierarchicalInstancedStaticMesh components, so
the three V3 builds — which carry 102 / 32 / 50 plain StaticMeshComponents —
reported zero. The unified pass covers both.

Checks applied to every building: roof orientation (does the top surface rise
toward the centre or the perimeter), elongated-instance outliers (the signature of
a mis-axed ridge bar), role/mesh name agreement, Nanite state, per-instance
custom-data width, and the material set.

## Finding 1 — no Wuxianmen-class geometry defects elsewhere

The inverted-roof and mis-axed-ridge defects found on Wuxianmen are **not** present
in any other building:

- **Zhengnanmen HighFidelity** — across 7,086 roof instances the mean top Z near the
  centre is 1,660 cm against 1,286 cm at the perimeter: the roof rises to a ridge.
- **Guidemen V5 4K** — verified directly from the tile surface rather than from the
  large shell instances: 2,002 cm at the centre falling to 1,465 cm at the
  perimeter. Its two main ridges run 31.2 m and 27.2 m along X, matching its
  pavilion width. No mis-axed bar.

Two suspicious readings were run down and **dismissed**:

- A 2,230 cm "elongated outlier" on Zhengnanmen is the 22.3 m top rail of its
  balustrade (24 cm square, running along the building's long axis) — legitimate.
  Its sibling "outlier", 239 cm, is a balustrade panel.
- Guidemen's initial "inverted" verdict came from correlating only its 11 large
  roof-shell and ridge instances, whose own extents dominate the sample. The direct
  tile-surface profile shows a correct ridge. The generic metric is a screening
  tool, not a verdict.

## Finding 2 — Nanite fallbacks were simplified across five buildings

`fallback_relative_error = 1.0` and `fallback_percent_triangles = 1.0` on 214
meshes in five buildings, so the fallback mesh kept **1% of triangles**:

| Building | Meshes | Notable |
| --- | --- | --- |
| Guidemen V5 4K | 47 of 47 | the 16,248-instance roof tiles, both roof shells, both main ridges, both plaques |
| Xiaobeimen AAA V3 | 38 | roof tiles, roof, ornaments, plaque, doors, railings, flags |
| Xiaobeimen Production V3 | 10 | roof tiles, ridge ornament, plaque, wood |
| Zhengnanmen AAA V3 | 7 | signboard, stone, roof glaze, red wood, door wood |
| Zhengnanmen HighFidelity | 103 | beams, doors, floor cores, arch fills, rails, ridge ends, dressed stone, tiles |

This matters on this project specifically: the visual change archive records the
editor rendering with **Nanite disabled** on an SM5 fallback path, and on that path
the fallback mesh is what draws. A 53,238-triangle tiled roof would have fallen back
to roughly 532 triangles, and a 12-triangle wall slab to nothing meaningful.

**Fix.** Every Nanite mesh whose fallback was not full-detail now reports
`fallback_relative_error = 0.0` and `fallback_percent_triangles = 100.0`, applied
through `StaticMeshEditorSubsystem.set_nanite_settings` followed by
`post_edit_change` and save. Nanite-disabled collision meshes (`UCX_*`) were
correctly left alone. Wuxianmen already carried `0.0 / 100.0`, so this makes that
the project-wide standard.

The target rule scans the Blueprints directly rather than an earlier report. That
mattered: the report flagged only `error > 0` and so missed **103 Zhengnanmen
meshes that already had error 0.0 but percent 1.0**. The validation caught the
partial fix, and a second run closed it — 111 + 103 = 214 meshes, 0 skipped.

## Finding 3 — Guidemen V5 4K had never had the detail pass

Guidemen is the V5 sibling of Wuxianmen and shares its `_ReferenceTuned` material
family, but it never received the revision-2/3 treatment. Its two largest surfaces
were flat: 2,170 stone-block instances and 16,248 roof-tile instances.

- **Stone** — 15 components, 1,889 instances. Per-instance custom data now carries
  `0 = UV offset`, `1 = wall height 0..1`, `2 = tone seed`, driving a wall-height
  stain, a per-instance tone of `0.72 .. 1.22`, and a restrained moss tint on the
  lower band. New material `M_WeatheredStone_4K_ReferenceTuned_RefTune4`.
- **Roof tile** — 12 components, 16,270 instances. Per-instance tone
  `0.86 .. 1.16`. New material `M_GrayClayTile_2K_ReferenceTuned_RefTune4`.

Unlike Wuxianmen, these materials also sample an **AO map**, so the rebuild
replicates that hookup: BaseColor × tint → BaseColor, Normal → Normal,
MetallicRoughness G → Roughness and B → Metallic, AO R → Ambient Occlusion.

**Every instance transform is byte-identical to the pass baseline** — 47 components
and 18,822 instances preserved.

## Tests

- `python Scripts/remote_run.py Scripts/AuditGuangzhouLandmarks.py` — first pass;
  found the HISM-only blind spot and the Nanite issue.
- `python Scripts/remote_run.py Scripts/AuditGuangzhouLandmarksV2.py` — PASS.
  Both architectures covered.
- `python Scripts/remote_run.py Scripts/ProbeGuidemenRoof.py` — PASS. Guidemen's
  roof surface falls monotonically from the centre to the perimeter.
- `python Scripts/remote_run.py Scripts/FixGuangzhouLandmarksNanite.py` — PASS,
  twice: 111 then 103 meshes, 0 skipped.
- `python Scripts/remote_run.py Scripts/FixGuidemenRefTune4.py` — PASS.
  18,822 instances, transforms unchanged, 1,889 stone + 16,270 tile custom-data
  writes, AO hookup preserved.
- `python Scripts/remote_run.py Scripts/ValidateGuangzhouLandmarksCrossBuilding.py`
  — PASS. 228 Nanite meshes checked across all seven Blueprints, **0 offenders**;
  Guidemen component/instance counts, collision, visibility and shadow flags
  unchanged, transforms byte-identical, materials bound to 15 stone and 12 tile
  components, custom-data channels sized 3 and 1, wall height spanning −13 to
  1,095 cm.
- `python -m py_compile` on all new scripts — PASS.

## Known limits and open items

- **No perspective review was possible.** The deferred high-res screenshot is still
  writing one stale frame per requested view, so the Guidemen material values are
  measured and numerically validated but **not visually confirmed**.
- The three V3 builds and Zhengnanmen HighFidelity were audited and had their Nanite
  fallbacks corrected, but were **not materially retuned**. Each has had dedicated
  passes of its own (Xiaobeimen AAA materials, Zhengnanmen flat masonry, intact
  envelope and manual 4K materials), and their roof orientation is verified correct.
  Their per-instance custom-data width is 0, so the same weathering treatment
  Wuxianmen and Guidemen received is available to them if wanted.
- The roof-orientation metric is a screening tool. It gave a false positive on
  Guidemen because a handful of large shell instances dominated the sample; the
  direct surface profile is the reliable test.
- The Nanite change trades fallback memory for fidelity. The per-mesh triangle
  counts are recorded in the fix report so the cost is reviewable; the largest
  affected meshes are the Xiaobeimen roof tiles at 53,238 and 49,500 triangles, and
  the rest are small.
- Cooked-build performance, platform memory and collision cooking remain separate
  checks.
