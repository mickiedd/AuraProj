# Guidemen rebuild — the re-import crashed the editor, and the safer rebuild path

Date: 2026-09-18.

Two things to record: an incident I caused, and the change of approach it forces.

## Incident — out-of-memory crash of the shared editor

While starting the from-scratch rebuild I re-imported the source GLB into Unreal and
**crashed the shared Unreal Editor** with a fatal out-of-memory error.

```
14:16:21  REBUILD_RECOVERY {"created": true, ...}          recovery copy saved
14:16:21  Interchange start importing source [...Guidemen_GuideGate_UE5_100M_Instanced.glb]
14:25:13  LogMemory: Warning: Freeing 33554432 bytes (32.0 MiB) from backup pool to handle out of memory
14:34:36  LogWindows: Error: Fatal error: Ran out of memory allocating 1485644460 (1416.8 MiB) bytes
          Last error msg: The paging file is too small for this operation to complete.
14:34:37  LogExit: Executing StaticShutdownAfterError
```

**Cause:** `bake_meshes = True`. The build report says this GLB is *"shared high-detail
mesh instancing"* — **169,891,784 expanded triangles against 38,276 unique**. Baking writes
each of the 18,823 nodes out as its own mesh, so the import tried to materialise the full
expanded count and exhausted the paging file. `bake_meshes` must be **False** for this
source; `combine_static_meshes` must also stay False so instancing survives.

**Damage: none.** Verified after the crash:

| artefact | state |
| --- | --- |
| `BP_Guidemen_V5_4K.uasset` | intact, 21:39 — includes the roof-shell correction |
| `BP_Guidemen_V5_4K_PreRebuild_20260918.uasset` | intact, 22:16 — the recovery copy |
| `M_WeatheredStone_4K_ReferenceTuned_RefTune4.uasset`, `M_GrayClayTile_…_RefTune4` | intact, 21:29 |
| `…/Guidemen_4K/Rebuild20260918/` | never written — no partial assets |
| editor | exited; needs restarting |

No committed work was lost and no partial import was left on disk. The cost is the editor
session and the time.

**Script fixed and guarded.** `Scripts/RebuildGuidemenStep1Import.py` now sets
`bake_meshes = False` and refuses to run at all if the projected expansion exceeds a
20M-triangle limit, with the failure reason in the assertion message. It also carries a
"DO NOT RUN AS WRITTEN" banner pointing at this record.

## What this changes about the rebuild

A scene re-import of this source is the wrong tool. It is expensive, it carries this OOM
risk, and — decisively — **it would not change anything that is wrong.**

What the source audit established:

- The current asset's 47 components and 18,822 instances **match the source exactly** —
  geometry names, instance counts and node transforms all correspond.
- The ridge runs along **X** at `Z = 0`, confirmed three independent ways in the source, and
  the asset agrees.
- The only geometry defect was the **eight inverted roof shells**, which is a defect *in the
  source*, and it is **already corrected** in the asset — flipped and reseated to
  `12.90 .. 14.62`, matching the source's own tile span (`12.92 .. 14.72` lower,
  `17.03 .. 18.65` upper) to within 2 cm on both tiers.

So the right rebuild is **the assembly, not the geometry**: rebuild the Blueprint's
component and instance payload from the source GLB's node transforms, reusing the
already-imported meshes, and apply the shell correction as part of it. That is exactly the
part that has been wrong across four passes — the assembly and its roof — and it needs no
heavy import, no expanded geometry, and carries no OOM risk.

## Revised rebuild plan

1. ~~Re-import the source scene.~~ Withdrawn — OOM risk, no benefit.
2. **Rebuild the assembly from source node transforms.** Read the 18,823 nodes from the GLB
   (JSON chunk + accessors, no geometry decode), map each to its geometry definition and
   material, and write the component/instance payload directly onto a rebuilt Blueprint.
3. **Apply the shell correction** as part of the rebuild, per the user's choice of
   correcting in the pipeline rather than upstream: flip the eight shells 180° about the
   world X axis through each one's ridge-side edge, then reseat so each sits just under its
   tier's tiled surface. Verified target: clearance below the tile top 9.9 cm and offset
   above the tile base 5.0 cm, identical across all four shells per tier.
4. **Re-apply the reference tuning** from the reference sheet: the eight `SourcePBR` sets and
   the palette the sheet names — 青石磚塊 stone, 灰白牆面 plaster, 舊木構件 wood, 灰色瓦片
   clay tile, 老舊木材 aged timber — plus the per-instance weathering already proven on this
   asset.
5. **Verify** against the source: component and instance counts, per-instance transforms,
   the ridge axis, the shell-to-tile clearances, and the material bindings.

The rebuild targets the existing `BP_Guidemen_V5_4K` in place, with
`BP_Guidemen_V5_4K_PreRebuild_20260918` kept as the recoverable prior state.

## Status

- Blocked on the editor being restarted — it is currently down.
- Recovery copy in place; no work lost.
- Rebuild scripts for steps 2–5 not yet written.

## Method note

For a heavily instanced glTF scene, **never enable mesh baking** and never assume an import
is cheap because the file is small: 74.8 MB on disk was 169.9M expanded triangles. Read the
build report or the GLB's own node/geometry counts and compute the expansion before
importing, and prefer rebuilding an assembly from node transforms over re-importing a scene
when the meshes are already present.
