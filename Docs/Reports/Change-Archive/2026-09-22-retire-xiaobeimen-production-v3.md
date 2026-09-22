# Xiaobeimen Production V3 removed from the showcase, asset folder deleted

Date: 2026-09-22.

[Visual change summary](2026-09-22-retire-xiaobeimen-production-v3.svg)

## Intent and scope

The showcase ring was placing **two** Xiaobeimen models — `BP_Xiaobeimen_AAA_V3` and
`BP_Xiaobeimen_Production_V3` — so the same gate appeared twice, the same problem the
Wuxianmen Core retirement fixed earlier the same day. The request was to take
`BP_Xiaobeimen_Production_V3` out of the showcase level and then delete it.

Two scope questions were put to the user before anything destructive happened, and both
answers decided the outcome:

- **Which levels.** The referencer query came back with **two** owning levels, not one:
  `L_GuangzhouLandmarkShowcase` (the ring) *and* `Scifi_desert_city/Level/L_showcase_level`.
  The user chose to clear both. Clearing only the ring would have left the other level
  holding a reference to an asset about to be deleted.
- **How much to delete.** The user chose the whole `V3/Xiaobeimen_Production_V3` folder
  rather than the Blueprint alone — it is self-contained (its own `Meshes/`, `Materials/`
  and `Textures/`, plus its own preview map), so deleting the Blueprint alone would have
  orphaned 175 assets and broken the preview map.

## Changed behavior

- `Scripts/CreateGuangzhouLandmarkShowcase.py` — the `Xiaobeimen_Production_V3` entry was
  removed from `LANDMARKS`, with a comment recording that the asset was retired, that
  Xiaobeimen is now represented by its AAA V3 variant, and that one Xiaobeimen in the ring
  is the point of the retirement.
- `Scripts/ValidateGuangzhouLandmarkShowcase.py` — the matching `EXPECTED_LABELS` entry
  removed. Expected light labels are derived from that list, so the light check followed
  automatically.
- `Scripts/RemoveShowcaseLevelXiaobeimenProductionV3.py` (new) — removes the actor from
  `L_showcase_level` and saves the map. Deliberately narrow: it only destroys actors whose
  class path starts with the target Blueprint's asset path, so the level's other
  `GuangzhouLandmark_*` actors are untouched. It refuses to run while any map is dirty.
- `Scripts/RetireXiaobeimenProductionV3.py` (new) — the deletion, modelled on
  `RetireWuxianmenV5_4K_Core.py`. It refuses unless every referencer it finds is **inside**
  the folder being deleted (the preview map and the Blueprint reference each other, so a
  "no referencers at all" test could never pass) and covers all 176 assets, not just the
  Blueprint. Made **resumable** after the first attempt was interrupted: it reports what is
  left instead of asserting the Blueprint still exists.
- `Scripts/FinishRetireXiaobeimenProductionV3.py` (new) — finishes the sweep without
  `delete_directory`; see the incident below.
- `Scripts/VerifyXiaobeimenProductionV3Retired.py` (new) — end-to-end verification that the
  asset is absent, the registry has no hits, and `L_showcase_level` no longer holds an actor
  from the deleted folder while its other five landmarks survive.
- The ring was rebuilt from the builder script rather than hand-edited, matching how the
  Guidemen and Wuxianmen slots were handled.

## Validation

- Pre-flight: both `get_dirty_content_packages()` and `get_dirty_map_packages()` were empty
  before each level switch, so no rebuild could stomp unsaved work.
- `L_showcase_level`: 1786 actors → **1785**, the single
  `GuangzhouLandmark_Xiaobeimen_Production_V3` actor destroyed (it had no attached
  children), map saved, dirty maps empty afterwards.
- Ring rebuild: 31 previous actors cleared, **28** rebuilt — 7 landmarks, 7 lights, 7 text
  labels, ground, 2 directional lights, sky light, sky atmosphere, post process volume,
  player start.
- Ring radius `9541.1` → `8198.5` cm, and that is arithmetic rather than surprise: the radius
  is `sum(2 × radius_xy + 3000) / 2π` over the placed set, so losing the Production gate
  (radius `2718.00` cm) and its 3000 cm gap takes the arc from 59,948.5 to 51,512.5 cm. Every
  landmark's ring angle changed; the min angular gap is now `42.627°` and the max `62.128°`.
- `ValidateGuangzhouLandmarkShowcase.py` reloads the saved map from disk and re-derives each
  light from the manifest's own recipe rather than from what the builder wrote down.
  **0 errors, 0 warnings**: 28 actors, 7/7 landmarks present exactly once, no unexpected
  landmark-tagged actors, 0 XY overlaps, 7/7 lights each attached to its own building, and
  **0 isolation violations** — tightest nearest-neighbour cone margin `12.912°`
  (Guidemen → Wuxianmen FullPBR), still comfortably positive. Re-run after the deletion with
  the same result.
- Deletion: 176 assets, `RETIRE_OUTSIDE_REFERENCERS {}` — an empty set, which is the proof
  the two level edits had actually landed. Preview map deleted `True`, Blueprint deleted
  `True`, then 40 + 134 leaf-first deletions with **0 refusals**. Verified afterwards:
  `does_asset_exist` `False` for the Blueprint and the preview map, `does_directory_exist`
  `False`, no registry hits for the path, `load_asset` returns `None`, and
  `Xiaobeimen_AAA_V3` still holds its 324 files.

## Incident: `delete_directory` wedged the editor

The first deletion attempt used `EditorAssetLibrary.delete_directory`, matching the
Wuxianmen script. It deleted the preview map and the Blueprint, then **wedged the Unreal
Editor**: the editor log stops mid-way through `Force Deleting 174 Package(s)` and never
writes another line.

Diagnosis, because the failure mode looked like "still working":

| Check | Result |
| --- | --- |
| CPU consumed over 15 s | **0 s** across all 178 threads |
| `Process.Responding` | **False** |
| Modal dialog | **None** — main window visible *and* enabled, no `#32770` window |
| Remote execution node | No longer discoverable |
| Editor log | Frozen, unchanged for 7 minutes |

The client side was also killed by a shell timeout during that run, so the script's own
stdout was lost — the editor log was the only record, which is why every path is now logged
*before* it is deleted.

The editor was restarted (new PID, log reopened 23:14:55), after which the remaining 174
assets were removed **one at a time** with `delete_asset`, leaf-first (textures → materials
→ meshes) in batches of 40. That completed in two passes with 0 refusals and no wedge, which
places the blame on `delete_directory`'s bulk `ForceDeleteObjects` rather than on asset
deletion as such. `FinishRetireXiaobeimenProductionV3.py` exists for that reason and is the
script to reach for if this folder is ever retired again.

## Limits and recovery

No fresh visual capture was taken. The change is a removal plus a re-layout, and the
validator's geometric checks cover what a capture would show; whether the ring still reads
as a ring at the smaller radius is a matter of taste, not correctness.

All 176 deleted files were git-tracked, so git is the recovery path. The deletions are left
**unstaged**, so `git checkout -- Content/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3`
restores them. The assets are Git-LFS-tracked (`.gitattributes` gives `*.uasset`/`*.umap`
`filter=lfs -text`), so recovery depends on the LFS object store as well as the commit.
Restoring the files alone would not be enough in any case: the builder's `LANDMARKS` entry
and the validator's `EXPECTED_LABELS` entry would both need reverting, and
`L_showcase_level`'s actor would have to be re-spawned by hand, since that level is not
generated from a script.

**Open follow-up.** The landmark-registry scripts that still hardcode retired paths — the
debt already recorded for Guidemen and Wuxianmen — are unaffected by this job, since none of
them named the Production V3 path: `AuditGuangzhouLandmarks.py`,
`AuditGuangzhouLandmarksV2.py`, `ExportLandmarkMeshes.py`, `FixGuangzhouLandmarksNanite.py`,
`InspectLandmarkBlueprintDetail.py`, `SnapshotV5ReferenceTuning.py`,
`ValidateGuangzhouLandmarksCrossBuilding.py`, `ValidateV5ReferenceTuning.py` and
`ProbeShowcaseLightingReference.py`. Worth confirming rather than assuming before the next
retirement.

Separately and unrelated to this change: **the C: drive was 99% full at the time of
writing** — 32 GB free of 1.9 TB. That is worth clearing before the next heavy import or
cook.
