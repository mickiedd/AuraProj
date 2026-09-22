# Guidemen showcase slot repointed, un-repaired Blueprint retired

Date: 2026-09-22.

[Visual change summary](2026-09-22-guidemen-showcase-swap-retire-v5-4k.svg)

## Intent and scope

The showcase level was placing `BP_Guidemen_V5_4K`, the model that never received the
reference repairs. The repairs — the 2026-09-21 window and roof pass and the 2026-09-22
arch, door and plaque follow-up — were applied to the Blueprint named
`BP_Guidemen_V5_4K_PreRebuild_20260918`. That name is a leftover: it was a backup taken
before the rebuild, and the repair work then targeted it by name. So the showcase was
showing the un-repaired gate while the corrected model sat unused, referenced only by
`L_Guidemen_V5_4K_Preview`.

Two changes: repoint the showcase at the repaired model, and delete the now-orphaned
`BP_Guidemen_V5_4K`.

## Changed behavior

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — the Guidemen `LANDMARKS` entry now names
  `V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918`. The slot key changed from
  `Guidemen_V5_4K` to `Guidemen_ReferenceRepaired` (chosen with the user), so the actor is
  `Landmark_Guidemen_ReferenceRepaired`, its light is `Light_Guidemen_ReferenceRepaired`,
  and the text label reads `Guidemen (Guide Gate) - V5 4K reference-repaired`. A comment
  records why the "PreRebuild" name must not be read as "stale".
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — the matching `EXPECTED_LABELS` entry.
  Expected light labels are derived from that list, so the light check followed
  automatically.
- `ensure_level()` in the builder now returns early when the showcase level is already the
  open level, instead of asking the level editor to load the level that is already current
  — a re-run from a live editor previously depended on that call reporting success. The
  clear-and-rebuild is what makes the run idempotent, not the load.
- `BP_Guidemen_V5_4K` deleted. `Scripts/RetireGuidemenV5_4K.py` performs the deletion and
  refuses to run unless the asset's referencer list is empty, so a still-referenced asset
  cannot be removed and leave a broken reference behind.

## Validation

- The level was rebuilt from the builder script, not hand-edited. It cleared the 34
  previous actors and rebuilt all 34.
- Pose is unchanged, which is the point: ring radius `10612.11` cm (was `10612.11`), and
  the Guidemen actor sits at `[-10534.14, 1284.03, 24.00]` with yaw `83.05°` — the same
  location and yaw it had before. The repaired model measures the same bounding circle
  (`3008.32` cm) and height (`1967.5` cm), so no other landmark moved.
- `ValidateGuangzhouLandmarkShowcase.py` reloads the saved map from disk and re-derives
  every light from the manifest's own recipe. **0 errors, 0 warnings**: 34 actors, 9/9
  landmarks present exactly once, actor Z within 0.004 cm of the manifest, 0 XY overlaps,
  9/9 landmark lights with 0.0 cm placement error, 0.0° aim error and 0 isolation
  violations. Re-run after the deletion with the same result.
- The placed actor's class is `BP_Guidemen_V5_4K_PreRebuild_20260918_C_0`, confirming the
  repaired model is what the level instantiates. Its 47 components and 18,822 instances
  are intact.
- `Light_Guidemen_ReferenceRepaired` is attached to its own building, cone `48.396°`,
  attenuation `9029.71` cm, and clears the nearest neighbouring light by `9.817°` — the
  tightest margin in the ring and still positive.
- Deletion: referencers `[]` before deleting, `delete_asset` returned `True`, and the
  asset was confirmed gone rather than trusted. The saved showcase map was then checked
  directly: **0** occurrences of the retired package name, **2** of the repaired one.

## Limits and recovery

The two Blueprints measure to the same bounds, so this change is about which authored
model the level points at — it is not expected to change the silhouette in a capture, and
no fresh visual capture was taken. The difference is in the geometry and materials the
repairs touched (roof and window enclosures, arch voussoirs, door leaves, plaque).

Recovery: `Saved/GuidemenV5_4K_Retired_20260922/BP_Guidemen_V5_4K.uasset`, md5
`54482594609fbbf60b4fa0bf6c94af03`, byte-identical to the original. The asset is also
tracked in git, so `git checkout -- <path>` restores it. Note that restoring the file is
not enough on its own — the builder's `LANDMARKS` entry and the validator's
`EXPECTED_LABELS` would both need reverting to place it again.

Fifteen further scripts still reference the retired path and were deliberately left
alone: they are historical capture and probe scripts for the rebuild work
(`CaptureGuidemen*.py`, `ProbeGuidemen*.py`, `FixGuidemen*.py`, `RebuildGuidemen*.py`,
`_probe_*.py`, `_cleanup_and_restore.py`, `DiagnoseGuidemenMaterials.py`,
`DumpGuidemenAssembly.py`, `RebindRepairedMeshes.py`, `ValidateGuidemenRoofShells.py`),
plus the landmark-registry tools `AuditGuangzhouLandmarks.py`,
`AuditGuangzhouLandmarksV2.py`, `ValidateGuangzhouLandmarksCrossBuilding.py`,
`ExportLandmarkMeshes.py`, `SnapshotV5ReferenceTuning.py`,
`FixGuangzhouLandmarksNanite.py`, `InspectLandmarkBlueprintDetail.py` and
`ValidateV5ReferenceTuning.py`. The last group audits the current landmark set and will
skip or fail on the missing asset if run; they were not repointed here because
`ValidateV5ReferenceTuning.py` carries per-asset expected dimensions tied to the old
model, which needs its own decision.
