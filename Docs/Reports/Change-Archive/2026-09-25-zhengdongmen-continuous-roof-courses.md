# Zhengdongmen: continuous roof courses, and the plaque in traditional characters

Date: 2026-09-25

[Visual summary](./2026-09-25-zhengdongmen-continuous-roof-courses.svg)

## Intent

The user asked for the roof tiling to be fixed so "the tile courses cover each roof
slope continuously", to match the supplied reference board, and for the plaque's text
to be corrected.

The reference board is **interpretive guidance, not a measured survey**. Its roof-tile
detail panel shows the construction the courses should read as: rounded barrel ribs
(筒瓦) running unbroken from ridge to eave, pans (板瓦) between them, and course joints
that follow the rib profile.

## What was wrong

The previous pass modelled 2,998 closed tile shells but laid them in **brick bond** —
each row offset half a tile from the one above — and sized each row as a **rectangle of
that row's narrowest edge**. Three separate defects followed, and none of them is
visible in a size or count check:

1. **The stagger destroyed the courses.** Offsetting alternate rows makes the surface
   read as a staggered scale pattern. A roof reads as its down-slope courses, and
   courses only read if the ribs stay in line.
2. **A staircase of uncovered wedges ran down both hips.** Covering each row with a
   rectangle sized to the row's *top* edge leaves the trapezoid's widening area bare;
   the uncovered strip reached **0.420 m**, wider than the 0.34 m hip ridge that would
   have hidden it.
3. **The odd rows exposed both side edges**, because the half-pitch stagger moved their
   first tile a half pitch inboard.

The plaque read **正东门** — the simplified forms — on a square canvas that the 4.34:1
plaque quad then stretched, so the characters rendered about twice as wide as they are
tall, in a modern geometric sans. The reference board titles the gate 大東門/正東門 in
the traditional forms, which is also what a late-Qing reconstruction should carry.

## Changed behaviour

**Roof.** `tiled_roof_slope` now covers each slope with continuous down-slope runs:

- One run per pitch on a **fixed rib grid**, laid ridge to eave with a 14% downslope
  overlap and **no row offset** (`stagger_rows: 0`).
- Each run is **clipped to the slope's own trapezoid**. A run is split at the parameter
  where it first fits whole, so every course edge — the entry, the split and the trim —
  lies on the straight hip line. Chording across either corner is exactly what left the
  uncovered wedges; splitting makes the mesh exact instead.
- A run enters at the parameter where its clipped width reaches 3% of the pitch rather
  than at zero width, so no degenerate sliver is built.
- The cross-section is a rounded barrel rib **0.066 m proud over 46% of the pitch** with
  a 0.020 m concave pan either side, sampled 11 times across. The previous flat cap with
  a `.13*(1-q²)` crown was replaced because a corrugation is what makes the runs read as
  courses rather than as a surface.
- Shell thickness is 0.030 m, and the slope substrate planes were dropped so the tile
  troughs (which dip `BASE - SHELL` below the plane) cannot poke through.
- The hip ridges (垂脊) were widened 0.19 → 0.34 m so they cap the trimmed course ends.
  They are deliberately **seated on the hip line, not lifted**: a +7 cm lift made the hip
  ridge the tallest part of the building and pushed the measured height to 1940 cm
  against the package's declared 1933 cm.

**Plaque.** `Scripts/MakeZhengdongmenPlaque.py` authors the whole Sign map set at the
board's own aspect ratio — 2048 × 512 for a 3.3 × 0.76 m quad — with **正東門** in Songti
TC Bold, gold on the same dark brown board, keeping the original's frame geometry. All
six maps are generated together: a BaseColor swapped without its Height / Normal /
Roughness / Metallic / AO partners would smear the old square relief across the new
board. The generator's GLB embed no longer forces every BaseColor to a 1024² square,
which would have re-stretched the characters.

## Validation

- `Scripts/ValidateZhengdongmenRoofTiling.py` was **rewritten** because the old check
  asked only whether a tile was wider than its pitch — which the staggered layout passed
  while leaving the hip wedges open. It now reconstructs the courses in slope-parametric
  space and proves coverage:

  | check | before | after |
  |---|---|---|
  | widest uncovered strip per slope | **0.420 m** (failed vs the 0.34 m hip cap) | **0.0122–0.0131 m** |
  | stagger | half a pitch on every odd row | 0 |
  | shell topology | 36 v / 68 t | 44 v / 84 t, derived from the recorded sampling |
  | tiles | 2,998 | 3,448 (300 trimmed at a hip) |

  It also asserts every course sits on the rib grid, that every run reaches both ridge
  and eave, that no tile lies outside the trapezoid, and that the substrate planes sit
  below the tile troughs. [Results](../../../Saved/Reports/Zhengdongmen/roof-course-validation.json)
- `Scripts/PreflightZhengdongmenGlb.py` (new, plain Python) checks the GLB container
  length, identity node transforms, one mesh / one material / `TEXCOORD_0` per part, and
  that every part's triangle count matches the generator's manifest. Union
  **2870 × 1639 × 1933.12 cm**, within 0.12 cm of the declared envelope.
- `Scripts/ValidateZhengdongmenLandmark.py` no longer hardcodes the triangle total. It
  reads the tuned generator's report, so the comparison is engine against source, and it
  now checks **each part** against the source — a stale mesh cannot hide behind a
  whole-building tolerance. That is the check that would have caught a roof mesh that
  was never reimported.
- Gate geometry **251,386 → 337,154 triangles** (roof tiles 203,864 → 289,632).

## Native reimport, validation and visual review

Run as **isolated UE 5.5 commandlets** (editor closed, `-nullrhi`) with a **separate fresh
process** for the validator, then a real GUI editor process for the captures. Exit codes
were 0 but the JSON reports are the judge.

- Reimport: `passed: true`, no problems. `SM_ZDM_RoofTile` at **289,852 triangles** and
  `SM_ZDM_Sign` at 14, both roll −90 / scale 1.0, both re-asserted to Nanite
  `PERCENT_TRIANGLES` 1.0. All six Sign maps reimported at **2048 × 512** — they had to be
  force-refreshed, because `import_textures` skips a texture that already exists and the new
  plaque board would otherwise never have reached the engine.
- Landmark validation, fresh process: **0 errors / 0 warnings**. Total **337,154 triangles**,
  matching the tuned source exactly, and **every part** matching its source group — the new
  per-part check. Union 2870 × 1639 × 1933.12 cm. Facade still on local +Y (the import
  negates source Y), unchanged. [Report](../../../Saved/RawModelImport/zhengdongmen-validation.json)
- Native Unreal/Metal captures of the saved Blueprint, seven views including two new ones
  (a straight-on plaque view and a lower-roof view). Temporary actors cleaned up 0 → 0.

| | |
|---|---|
| [Native — front](./2026-09-25-zhengdongmen-continuous-roof-courses-native-front.png) | [Native — three-quarter](./2026-09-25-zhengdongmen-continuous-roof-courses-native-three-quarter.png) |
| [Native — lower roof courses](./2026-09-25-zhengdongmen-continuous-roof-courses-native-roof.png) | [Native — plaque](./2026-09-25-zhengdongmen-continuous-roof-courses-native-plaque.png) |

The native plaque capture reads **正東門** with the gold frame, and the native roof capture
shows the courses running unbroken down the slope. The review process now **quits itself**
when it is done: the previous run left its editor open, which then blocked the next isolated
reimport.

## Render artefacts

Source-space Blender renders, `Saved/Reports/Zhengdongmen/courses-final-*.png`, and archived
beside this record:

- [Before — the staggered, scale-like surface](./2026-09-25-zhengdongmen-continuous-roof-courses-before-native-timber.png)
  (the previous pass's native capture)
- [After — three-quarter](./2026-09-25-zhengdongmen-continuous-roof-courses-after-three-quarter.png)
- [After — front, plaque legible](./2026-09-25-zhengdongmen-continuous-roof-courses-after-front.png)
- [After — lower roof courses, straight on](./2026-09-25-zhengdongmen-continuous-roof-courses-after-roof-lower.png)
- [After — upper roof courses, straight on](./2026-09-25-zhengdongmen-continuous-roof-courses-after-roof-upper.png)

Two new straight-on roof views (`roof-lower`, `roof-upper`) were added to
`RenderZhengdongmenPreviews.py`, because course continuity is only legible looking squarely
at a pitch. These are **source** renders; they validate the generator, not the native
Blueprint — the native captures above are the engine evidence.

## Maintained source and saved assets

- Generator: `ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py`
- Course tuning: `Scripts/TuneZhengdongmenReference.py`
- Plaque texture: `Scripts/MakeZhengdongmenPlaque.py`, wired into
  `Scripts/PrepareZhengdongmenPackage.py`
- Reimport: `Scripts/ApplyZhengdongmenReference.py`
- Saved mesh: `Content/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/Meshes/SM_ZDM_RoofTile.uasset`
- Blueprint: `Content/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/BP_Zhengdongmen.uasset`

## Open

- The course joints follow the rib profile, so a straight-on view of a steep pitch shows
  a scalloped course line. The reference has the same feature — its 筒瓦 course joints are
  visible arcs — but a real roof's pans have straight exposed edges where this
  continuous corrugation does not. Recorded as a known difference rather than a defect.
- The 194 MB of extracted source still sits in `Raw3DPacket/Zhengdongmen/` uncommitted.
- A GUI editor launched from this shell **stalls at plugin mounting** unless it is given
  `-unattended`; with `-unattended` the review process loads, captures all seven views and
  exits in under a minute.
