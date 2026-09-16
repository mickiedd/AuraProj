# GreatNorthGate updated to the DabeiMen model

## Intent

Replace the Great North Gate built in the previous job with the corrected
`C:/Works/Raw3DModels/V2/GreatNorthGate_DabeiMen_UE5_Nanite_6M.glb`, so the gate
uses a model whose materials match the design reference (青砖石材, 风化灰泥,
木构件, 灰瓦) instead of the earlier HighDetail import.

## Findings

- The 192 MB source holds 5 meshes, 5 PBR materials, 15 embedded PNG textures and
  6,005,624 triangles. **Unlike the previous model there is no instancing** — every
  mesh is referenced by exactly one node, and all five nodes sit at identity under a
  single `world` root, so the parts already share one building space.
- The material names map one-to-one onto the design sheet's material block, and
  `M_Roof_GrayClayTile` replaces the older landmark's green glazed roof.
- The source is again **authored Z-up** (stone base spans Z 0–13.575 m with the ground
  plane at Z 0), so the established `import_offset_rotation = roll −90°` +
  `bake_meshes = True` recipe applies unchanged.
- The plaque (`GNG_PLAQUE`) lands at **+Y** after the roll-90 import, so the gate's
  front faces +Y — the same side as the level's sun. Cameras on −Y shoot the back.
- **The "6M" roof mesh has no top surface.** Measuring area-weighted face orientation
  from the GLB gives 0.30% up / 83.65% down / 16.05% side for `GNG_ROOF`, while
  `GNG_STONE` and `GNG_WOOD` are symmetric at ~25% up / ~25% down. The symmetry of the
  other meshes confirms the measurement, so the roof really is a downward-facing
  soffit shell with no modelled tile layer.
- Consequence: with backface culling on, the roof is invisible from every
  above-horizon view. An isolated overhead capture (all other parts hidden) showed only
  thin edge slivers, and the full-gate captures read as an exposed timber frame.

## Changed behavior

- Imported to `/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_DabeiMen`
  with `combine_static_meshes = False`, `build_nanite = True`, `bake_meshes = True`,
  `import_offset_rotation = roll −90°`, `replace_existing = True` and collision off.
  All five meshes imported upright at full fidelity; every imported bound matches its
  glTF source AABB to the centimetre (6800 × 1210 × 2450 cm overall).
- **Retired** the superseded wrapper: `GuangzhouLandmark_GreatNorthGate_HighDetail`
  and its 6 part actors were destroyed. The `GreatNorthGate_HighDetail` assets are
  deliberately kept on disk so the previous import remains available for rollback.
- Added one static root actor `GuangzhouLandmark_GreatNorthGate_DabeiMen`
  (tags `ImportedGuangzhouLandmark`, `GuangzhouLandmarkPark`, `GreatNorthGateDabeiMenRoot`)
  with the 5 mesh parts (`GreatNorthGateDabeiMenPart`) attached via `KEEP_WORLD` at
  identity, placed at the same footprint centre `(-140400.0, 110442.5, 100.0) cm`,
  rotation `(0°, 0°, 0°)`, grounded at `Z = 100.0 cm`.
- Applied a workaround for the roof defect: `M_Roof_GrayClayTile` is set
  `two_sided = True`, which makes the soffit render from above so the gate reads as a
  complete building. This is **not a fix** — the surface shown is still the authored
  underside, not a tile layer. A re-import regenerates the material and clears it.
- Deliberately untouched: `GuangzhouLandmark_GreatNorthGate` (the original landmark),
  `GuangzhouLandmark_GreatSouthGate`, `GuangzhouLandmark_Xiaobeimen` and
  `GuangzhouLandmark_ZhenhaiTower`.

## Visual and automated validation

- Hero capture: `Saved/RawModelImport/great_north_gate_dabeimen_hero.png`.
- Row context: `Saved/RawModelImport/great_north_gate_dabeimen_visual.png`.
- Close-up: `Saved/RawModelImport/great_north_gate_dabeimen_closeup.png`.
- Roof evidence: `..._roof_isolated.png` (overhead, other parts hidden) and
  `..._roof_twosided.png` (same angle with the workaround applied).
- `remote_run.py Scripts/ImportGreatNorthGateDabeiMen.py` — passed; 5 Nanite meshes,
  each name mapped back to its glTF source, all material slots assigned, and the
  upright + per-axis size assertions (`6800 / 1210 / 2450 ± 60 cm`) satisfied.
- `python Scripts/AnalyzeDabeiMenGlb.py <glb>` — offline structural decode used to
  choose the import flags before launching the editor.
- `python Scripts/CheckDabeiMenRoofFacing.py <glb>` — measured the roof face
  orientation and produced the soffit finding above.
- `remote_run.py Scripts/PlaceAndWrapGreatNorthGateDabeiMen.py` — passed; 7 superseded
  actors retired, 5 parts parented, grounded, overlap guard clear.
- `remote_run.py Scripts/ValidateGreatNorthGateDabeiMen.py` — passed after save and
  reload: root label/tags/rotation, 5 parent links, grounding, exactly one roof part,
  the full 5-material set, no HighDetail actors remaining, and the four prior landmarks
  re-measured with ≤ 0.03 cm drift and 3562.5 cm clearance to each neighbour.

## Follow-up

- The roof needs a re-export from the 3D pipeline with an upward-facing tile shell;
  the two-sided material is only a stopgap.
- The original `GuangzhouLandmark_GreatNorthGate` still carries its green glazed roof
  while the design reference specifies 灰瓦 grey tiles. It was left in place because an
  earlier decision was to keep it untouched; consolidating the row to a single Great
  North Gate is still an open question.

## Evidence

- Source analysis: `Saved/RawModelImport/dabeimen-source-analysis.json`.
- Roof facing measurement: `Saved/RawModelImport/dabeimen-roof-facing.json`.
- Import report: `Saved/RawModelImport/GreatNorthGate_DabeiMen.json`.
- Placement manifest: `Saved/RawModelImport/great-north-gate-dabeimen-placement.json`.
- Validation report: `Saved/RawModelImport/great-north-gate-dabeimen-validation.json`.
- [Visual summary diagram](2026-09-12-great-north-gate-dabeimen-update.svg).
