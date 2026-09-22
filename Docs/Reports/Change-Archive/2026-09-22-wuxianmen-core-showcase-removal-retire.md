# Wuxianmen V5 4K Core removed from the showcase, asset folder deleted

Date: 2026-09-22.

[Visual change summary](2026-09-22-wuxianmen-core-showcase-removal-retire.svg)

## Intent and scope

`BP_Wuxianmen_V5_4K_Core` was the earlier Wuxianmen import. The variant that carries the
reference repair work is `BP_Wuxianmen_V5_FullPBR`, whose own
`ReferenceRepair20260922/` folder holds the 2026-09-22 materials and closed-surface
meshes. The showcase level was placing both, so the gate appeared twice in the ring and
the superseded model was still on display.

Two changes, as requested: take `BP_Wuxianmen_V5_4K_Core` out of the showcase level, and
delete it. Because the ring radius is derived from the landmarks' own geometry, removing
one entry also re-spreads the survivors — this was confirmed with the user beforehand
rather than assumed. The user also chose to delete the whole `Wuxianmen_4K_Core` folder
rather than the Blueprint alone.

## Changed behavior

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — the `Wuxianmen_V5_4K_Core` entry was
  removed from `LANDMARKS`, with a comment recording that the asset was retired and that
  Wuxianmen is now represented by its FullPBR variant only. Docstring and recipe counts
  updated: "seven reference-tuned Blueprints" → "six", "all nine landmarks" → "all eight".
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — the matching `EXPECTED_LABELS` entry
  removed. Expected light labels are derived from that list, so the light check followed
  automatically. The measurement-split comment now reads "five HISM-based landmarks".
- `Scripts/RetireWuxianmenV5_4K_Core.py` (new) — performs the deletion. Unlike
  `RetireGuidemenV5_4K.py`, which only had to prove the Blueprint itself was unreferenced,
  this one refuses to run unless every referencer it finds is **inside** the folder being
  deleted. That distinction is required: `L_Wuxianmen_V5_4K_Core_Preview` and the Core
  Blueprint reference each other, so a naive "no referencers at all" test could never pass.
  The preview map is deleted first, then the Blueprint, then the rest of the folder, with a
  multi-pass sweep as a fallback because `delete_directory` can decline assets that
  reference each other.
- The ring was rebuilt from the builder script rather than hand-edited, matching how the
  Guidemen slot was handled.

## Validation

- Pre-flight on the shared editor: the showcase level was already the open level, and both
  `get_dirty_content_packages()` and `get_dirty_map_packages()` were empty, so the rebuild
  could not stomp unsaved work.
- Rebuild: 34 previous actors cleared, 31 rebuilt — 8 landmarks, 8 lights, 8 text labels,
  ground, 2 directional lights, sky light, sky atmosphere, post process volume, player
  start.
- Ring radius `10612.11` → `9541.1` cm. This is arithmetic, not a surprise: the radius is
  `sum(2 × radius_xy + 3000) / 2π` over the placed set, so dropping the Core gate (radius
  `1864.69` cm) and its 3000 cm gap takes the arc from 66,677.9 to 59,948.5 cm. Every
  landmark's ring angle therefore changed, as the diagram shows; the min angular gap is
  now `36.628°` and the max `52.431°`, up from a nine-way split.
- `ValidateGuangzhouLandmarkShowcase.py` reloads the saved map from disk and re-derives
  every light from the manifest's own recipe rather than from what the builder wrote down.
  **0 errors, 0 warnings**: 31 actors, 8/8 landmarks present exactly once, no unexpected
  landmark-tagged actors, actor Z within 0.004 cm of the manifest, 0 XY overlaps, 8/8
  landmark lights each attached to its own building with 0.0 cm placement error, 0.0° aim
  error, and 0 isolation violations. Nearest-neighbour cone margins stay positive
  (`11.096°` tightest, Guidemen → Wuxianmen FullPBR). Re-run after the deletion with the
  same result.
- Deletion: 56 assets under the folder, outside referencers `{}` — confirming the rebuild
  had already dropped the level's reference — preview map deleted `True`, Blueprint deleted
  `True`, `delete_directory` `True`, leftovers `[]`, `does_asset_exist` `False`. Verified
  on disk afterwards: `V5/Wuxianmen_4K_Core` is gone, and `V5/Wuxianmen_FullPBR` still
  holds its 91 files.

## Limits and recovery

No fresh visual capture was taken. The change is a removal plus a re-layout, and the
validator's geometric checks cover what a capture would show; the interesting question —
whether the ring still reads as a ring at the smaller radius — is a matter of taste, not of
correctness.

All 56 deleted files were git-tracked, so git is the recovery path. Unlike the Guidemen
retirement, no separate `Saved/` backup directory was made. The deletion was committed as
`66d25a6d` (its parent, `a09fe291`, still holds the folder), so restore with
`git checkout a09fe291 -- Content/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core`.
The assets are Git-LFS-tracked (`.gitattributes` gives `*.uasset`/`*.umap` `filter=lfs
-text`), so the commit stores 3-line pointers rather than binaries and recovery depends on the
LFS object store rather than the commit alone — **checked, not assumed**: all 56 objects are
present under `.git/lfs/objects/`, and the Blueprint's object is `a0c4542e…` at 1,308,482
bytes, the exact size of the original. Restoring the files alone would not be enough in any
case: the builder's `LANDMARKS` entry and the validator's `EXPECTED_LABELS` would both need
reverting to place the gate again.

**Open follow-up.** 31 further scripts still name the retired path. The historical
capture, probe, fix and snapshot scripts for the Wuxianmen reference-tuning work
(`CaptureV5Wuxianmen*.py`, `CaptureV5ZeroRollProbe.py`, `FixV5Wuxianmen*.py`,
`ProbeV5Wuxianmen*.py`, `ProbeV5ZeroRollImport.py`, `SnapshotV5Wuxianmen*.py`,
`DiffV5WuxianmenBaselineRefTune2.py`, `DumpV5WuxianmenTransformsRefTune2.py`,
`ValidateV5Wuxianmen*.py`, `ImportV5WindingVariants.py`, `RebindV5WindingVariants.py`,
`RebindRepairedMeshes.py`, `PrepareV5Buildings.py`) are deliberately left alone, as they
document past passes. The landmark-registry tools are the ones that matter:
`AuditGuangzhouLandmarks.py`, `AuditGuangzhouLandmarksV2.py`, `ExportLandmarkMeshes.py`,
`FixGuangzhouLandmarksNanite.py`, `InspectLandmarkBlueprintDetail.py`,
`SnapshotV5ReferenceTuning.py`, `ValidateGuangzhouLandmarksCrossBuilding.py`,
`ValidateV5ReferenceTuning.py` and `ProbeShowcaseLightingReference.py` each carry a
hardcoded landmark list that still includes the Core gate and will now skip or fail on the
missing asset. `ValidateV5ReferenceTuning.py` additionally pins per-asset expected
dimensions for it, so repointing that group is its own decision — the same debt the
Guidemen retirement left open, and it is now one entry larger.
