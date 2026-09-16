# GreatNorthGate HighDetail import, actor wrap and placement

## Intent

Import `C:/Works/Raw3DModels/V2/GreatNorthGate_UE5_HighDetail.glb` into the project
and wrap it as the GreatNorthGate building — one level actor — without disturbing
the four landmarks already placed in `L_showcase_level`.

## Findings

- The 156 MB source holds 6 meshes, 5 PBR materials, 15 embedded PNG textures and
  6,040,704 triangles. The five detail meshes sit at identity in a shared building
  space; the roof is a single 6,000,000-triangle / 3,000,000-vertex unit tile
  (1.0 × 1.0 × 0.098 m) referenced by **9 nodes** with individual transforms, so
  the building renders ~54M triangles.
- **The source is authored Z-up, not Y-up.** The stone base spans Z 0–9.7 m, the
  roof trim Z 13.2–17.9 m and the gate sign sits on the front face at Y ≈ −3 m.
  Interchange assumes Y-up, so a first import landed the gate on its side
  (measured `2460 × 987.5 × 629 cm` for the stone body instead of `2460 × 629 × 987.5`).
- `import_offset_rotation` is **silently ignored** unless `bake_meshes` is enabled.
  The first correction attempt set `roll = −90` with `bake_meshes = False` and the
  bounds were byte-for-byte identical to the uncorrected import.
- With `bake_meshes = True`, Interchange also collapses the 9 roof nodes into the
  single roof mesh and bakes their combined placement into it. The resulting asset
  measures `1943.43 × 1006.34 × 716.0 cm`, which matches the computed union of all
  nine node transforms exactly — so no manual instancing is required.
- `mesh.get_num_triangles(0)` reports the **Nanite fallback** (74,606 for the roof,
  9,862 after the bake), not source LOD0. Fidelity was confirmed from the asset size
  (248 MB for the merged roof) and the glTF index accessor instead.

## Changed behavior

- Imported to `/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_HighDetail`
  with `combine_static_meshes = False`, `build_nanite = True`, `bake_meshes = True`,
  `import_offset_rotation = roll −90°` and collision generation off.
- Added one static root actor labeled `GuangzhouLandmark_GreatNorthGate_HighDetail`
  with tags `ImportedGuangzhouLandmark`, `GuangzhouLandmarkPark`,
  `GreatNorthGateHighDetailRoot`.
- Attached all 6 mesh parts (`GreatNorthGateHighDetailPart`) beneath the root with
  `KEEP_WORLD` rules, at identity in the shared building space.
- Placed at `(-140400.0, 110442.5, 120.0) cm`, rotation `(0°, 0°, 0°)`, grounded at
  `Z = 99.999 cm`, footprint `2460.0 × 1006.34 × 1812.0 cm`.
- The spot sits between the existing `GuangzhouLandmark_GreatNorthGate` and
  `GuangzhouLandmark_GreatSouthGate`, 3767 cm clear of both.
- Collision is deliberately not generated: the roof asset is 54M triangles, so
  building collision remains a separate gameplay step, as with GreatWestGate/Guidemen.

## Visual and automated validation

- Hero capture: `Saved/RawModelImport/great_north_gate_highdetail_visual_after.png`.
- Row overview: `Saved/RawModelImport/great_north_gate_highdetail_row_overview.png`.
- `remote_run.py Scripts/ImportGreatNorthGateHighDetail.py` — passed; 6 Nanite meshes,
  5 materials, upright assertion (`height > depth`) satisfied.
- `remote_run.py Scripts/AnalyzeGreatNorthGateRoofBake.py` — passed; the merged roof
  asset bounds match the union of the 9 node transforms.
- `remote_run.py Scripts/PlaceAndWrapGreatNorthGateHighDetail.py` — passed; 6 parts
  parented, grounded, overlap guard clear.
- `remote_run.py Scripts/ValidateGreatNorthGateHighDetail.py` — passed after save and
  reload: root label/tags/rotation, 6 parent links, grounding, per-part materials, and
  the four prior landmarks re-measured in place with no footprint overlap.
- The pre-existing `GreatNorthGate` asset folder (a different, older model) is
  untouched; its `SM_GreatNorthGate.uasset` still carries its Sep 10 timestamp.

## Evidence

- Import report: `Saved/RawModelImport/GreatNorthGate_HighDetail.json`.
- Roof bake analysis: `Saved/RawModelImport/great-north-gate-roof-bake-analysis.json`.
- Placement manifest: `Saved/RawModelImport/great-north-gate-highdetail-placement.json`.
- Validation report: `Saved/RawModelImport/great-north-gate-highdetail-validation.json`.
- [Visual summary diagram](2026-09-12-great-north-gate-highdetail-import.svg).
