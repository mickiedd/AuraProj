# Wuxianmen V5 FullPBR reference tuning, revision 2

Date: 2026-09-17.

Applied the asset-reference tuning workflow to `BP_Wuxianmen_V5_FullPBR` against
the canonical Wuxianmen reference sheet
(`Docs/Reference/GateSheets/Wuxianmen-reference.png`). This is the sibling of the
`Wuxianmen_4K_Core` asset tuned in the preceding pass; both come from the same
50M-instanced source scene.

[Archived diagram](2026-09-17-wuxianmen-fullpbr-reference-tuning.svg)

## What was wrong

The FullPBR assembly is **geometrically identical to the Core assembly**: same
seven components, same 5,103 instances, and every component's recorded transform
payload matches the Core baseline exactly (verified before mutation, recorded in
the baseline report). So it carried the same defects:

| # | Defect | Evidence |
| --- | --- | --- |
| 1 | **Both roof tiers inverted** — tiles and both wood roof decks rise from `z 12.75 m` at `y = 0` to `z 15.72 m` at `\|y\| = 4.5 m`. | Recorded transform comparison plus the same per-band vertex measurement used for Core. |
| 2 | **Both ridge bars on the wrong axis** — `main_ridge` `0.42 × 22.5 × 0.42 m` along **Y**, projecting 6.9 m past each eave; `lower_ridge` 6.15 m. | Same world extents as Core. |
| 3 | **The ridge role was bound to a roof-tile mesh**; `main_ridge` unused. | `HISM_004_ridge_000010` → `main_tile_-1_000`. |
| 4 | **The source textures carry no architectural pattern.** The roof tile sheet is flat charcoal noise and the stone sheet is flat mottle, so no UV scale can produce tile courses or ashlar blocks. | `_Repaired` PNGs are byte-identical to the archive's own PNGs. p99 image gradient: stone `0.004` (FullPBR) vs `0.071` (Core) — 18× weaker; roof `0.012` vs `0.051` — 4× weaker. |
| 5 | **The plaque was tinted gold** (`0.75/0.59/0.34`) although the reference board is light. | The FullPBR plaque texture is a light tan board reading 五仙門 left to right. |

## Changed behaviour

Geometry — identical to the Core pass, 328 of 5,103 instance transforms:

- Both roof tiers corrected with a 180° rotation about the world X axis through
  each tier's mid-plane (`main` pivot `z = 1423.5 cm`, `lower` `1251.5 cm`); a
  rotation, not a Z mirror, so winding and normals stay valid.
- The wood roof decks mirrored about a pivot 20 cm / 15 cm lower
  (`1403.5 / 1236.5 cm`) so the mirror does not lift them above the tiles.
- `HISM_004` re-bound to `main_ridge`, both ridge bars turned 90° about Z to run
  along X and seated at `z = 1572 / 1338 cm`.

Materials:

- **Roof.** Per the user's decision, the Core package's authored 4K tile sheet was
  copied into the FullPBR package as a task-owned texture (byte-identical copies;
  the Core source was not modified) and imported as
  `Textures/RoofTile_{BaseColor,Normal,Roughness}_4K_TileSheet`. A new
  `M_RoofTile_ReferenceTuned_RefTune2` drives it with the same measured contract:
  `U tiling 1/14.629 = 0.0684` (one tile column across the 0.359 m strip) and
  `V = frac(UV · 2.8) · 0.4431` (14 courses over the 5.068 m strip, 0.362 m
  pitch).
- **Stone.** `M_Stone_ReferenceTuned_RefTune2` keeps the FullPBR base colour and
  normal but tiles at `0.06` so the mottle is magnified to roughly 0.1 m features,
  and adds a per-instance custom data value (`num_custom_data_floats = 1`, 4,213
  values) driving a UV offset and a ±20% tone variation. Tint
  `0.61/0.58/0.51` → `0.53/0.50/0.44`.
- **Plaster.** `M_Plaster_ReferenceTuned_RefTune2`, tint `0.72/0.66/0.56` →
  `0.62/0.57/0.49`.
- **Plaque.** `M_Plaque_Wuxianmen_RefTune2`, tint `0.75/0.59/0.34` →
  `0.92/0.90/0.86`.
- Roughness and metallic are read from the packed MetallicRoughness maps
  (G = roughness, B = metallic), which the Core package did not use.

Preserved: component count (7), instance count (5,103), the other 4,775 instance
transforms, collision profiles, visibility, shadow flags, mesh assignments for
every component except the ridge, and all prior rollback materials.

## Tests

- `python Scripts/remote_run.py Scripts/SnapshotV5WuxianmenFullPBRRefTune2.py` —
  PASS. Baseline written before mutation; the Core comparison reported
  `transform_payload_identical: true` for all seven components.
- `python Scripts/remote_run.py Scripts/FixV5WuxianmenFullPBRRefTune2.py` — PASS.
  - `tile:main` 168 instances 0 unmatched; `tile:lower` 156 0 unmatched;
    `wooddeck:main` 2 0 unmatched; `wooddeck:lower` 2 0 unmatched.
  - Baseline restore was not needed (`restored_instances` all 0).
  - Ridge after: index 4 spans X 2100 × Y 42 cm at `z 1317..1359`; index 9 spans
    X 2250 × Y 42 cm at `z 1551..1593`.
  - Stone custom data: `num_custom_data_floats = 1`, 4,213 values assigned.
  - `instance_total` 5103.
- `python Scripts/remote_run.py Scripts/ValidateV5WuxianmenFullPBRRefTune2.py` —
  PASS. 7 components, 5,103 instances; live spawn bounds
  `3646.6 × 1813.3 × 1721.0 cm`; roof ridge `1572 cm` vs eave `1312 cm`
  (rise `+260 cm`); ridge 2 changed / 8 unchanged with no mismatches; wood 4
  changed / 447 unchanged with no mismatches; transient actor destroyed.
- `Scripts/AnalyzeV5WuxianmenFullPBRTexturesRefTune2.py` and
  `Scripts/CompareV5WuxianmenVariantsRefTune2.py` — the measurements quoted above.
- `python -m py_compile` on all new scripts — PASS.

## Visual evidence

- After (geometry readable despite the exposure issue):
  [`Wuxianmen_V5_FullPBR-reftune2-roof_side.png`](../../../Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reftune2-roof_side.png)
  shows the corrected pitched roof with a ridge and tile courses, the wall with
  block courses, and the plaque board. The matching Core capture
  ([`Wuxianmen_V5_4K_Core-reftune2-wall_close.png`](../../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-wall_close.png))
  confirms the borrowed tile sheet reads as gray tile courses under the same
  contract.
- Before: [`Wuxianmen_V5_FullPBR-reference-tuning-hero.png`](../../../Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reference-tuning-hero.png).

## Known limits and open items

- **The capture harness stopped tracking the camera partway through this session.**
  Every requested view wrote the same stale frame, for both variants, so the
  FullPBR review set is not a set of distinct views. Verified against camera
  readback, a forced viewport invalidate, a 10 s screenshot delay,
  `force_game_view` off, and a `SceneCapture2D` + render-target export path (which
  silently failed to write). The editor window being in the background is the
  likely cause. **A foregrounded editor should restore it**; the capture set
  should be re-run then.
- **The plaque reading direction is unconfirmed.** The FullPBR plaque art reads
  五仙門 left to right in texture space; the reference board reads 門仙五 left to
  right. Whether the model shows it correctly depends on the mesh's UV
  handedness on its front face, which needs a front-on capture to settle. The UV
  orientation was deliberately left unchanged rather than guessed.
- The FullPBR stone sheet has no joints, so the wall relies on the block geometry
  for joints and on per-instance tone for variation. It will read as varied
  masonry but not as strongly as the Core package, which has an authored ashlar
  sheet.
- The eight small ridge ornaments were left in place, as in the Core pass.
- Cooked-build performance, platform memory and collision cooking remain separate
  checks.
