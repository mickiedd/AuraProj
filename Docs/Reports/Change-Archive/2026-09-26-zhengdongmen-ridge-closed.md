# Zhengdongmen: the top roof slopes now meet at the ridge

Date: 2026-09-26

[Visual summary](./2026-09-26-zhengdongmen-ridge-closed.svg)

## Intent

The user, looking down the upper roof in the editor: "fix the hollows on the top roof side."

They were right. Viewed along the ridge, the upper roof showed a **flat band of bare
substrate on each side of the ridge cap**. The two north/south slopes started at
**y = ±0.5 m** instead of meeting at the ridge, so the top of the roof was a **1.0 m wide flat
band** — and the 0.30 m ridge cap only covered the middle of it, leaving **0.35 m of bare
substrate exposed on each side**.

The reference board is **interpretive guidance, not a measured survey**. Its 3D render shows a
hip roof whose ridge is a line: the two slopes meet at it and the cap sits on the seam.

## Changed behaviour

**The upper roof's slopes now meet at the ridge (y = 0).** Three consequences, all of them
corrections rather than side effects:

- The north/south slopes run from y = 0 to the eave at y = ±5.42, so the ridge cap covers the
  seam and the courses run right up to it on both sides.
- The east/west faces become **true triangles** rising to the ridge ends
  (`half_width_at(0)` goes from 0.5 to 0), which is what a 庑殿 hip roof is.
- Their substrate quads follow: the north/south one now has its top edge on the ridge line, and
  the east/west one is a triangle rather than a degenerate quad.

The hip ridges start on the ridge line too (the hip-cap table's upper `yin` 0.5 → 0.0), so the
three caps converge at the apex instead of leaving a notch.

## Validation

| | before | after |
|---|---|---|
| upper N/S slope length | 5.25 m | **5.72 m** |
| courses per upper N/S slope | 16 | 17 |
| tiles | 3,448 | 3,528 |
| roof tile mesh | 289,648 tris | **296,030 tris** |
| gate | 337,630 tris | **344,012 tris** |
| widest uncovered strip, upper E/W | 0.0124 m | **0.0000 m** |
| envelope | 2870 × 1639 × 1933.12 cm | 2870 × 1639 × **1933.24 cm** |

- `Scripts/ValidateZhengdongmenRoofTiling.py`: `passed`; all eight slopes within the 0.34 m hip
  cap, and the upper east/west faces now have **no uncovered strip at all** because they taper
  to a point. [Results](../../../Saved/Reports/Zhengdongmen/roof-course-validation.json)
- `Scripts/PreflightZhengdongmenGlb.py`: `passed`; the envelope moved **+0.12 cm** — the hip caps
  shifted a hair as the hip line moved, and the ridge cap still sets the height.
- `Scripts/ValidateZhengdongmenLandmark.py`, fresh process: **0 errors / 0 warnings**, **8/8
  parts** matching the tuned source exactly.
- Native Unreal/Metal captures, **9 views**, cleanup 0 → 0.

| | |
|---|---|
| [Before — the user's viewport, looking down the ridge](./2026-09-26-zhengdongmen-ridge-closed-before-viewport.png) | [After — native, same angle](./2026-09-26-zhengdongmen-ridge-closed-native-roof-ridge.png) |
| [Before — source render reproducing it](./2026-09-26-zhengdongmen-ridge-closed-before-source.png) | [After — source render](./2026-09-26-zhengdongmen-ridge-closed-after-source.png) |

[After — native three-quarter](./2026-09-26-zhengdongmen-ridge-closed-native-three-quarter.png)

## A new view, because the old ones could not see it

Course continuity and the ridge material were both verified with **straight-on** views, and
neither of those shows this defect: a bare band along the ridge is only visible looking **down**
the roof. `roof-ridge` was added to `RenderZhengdongmenPreviews.py` *and*
`CaptureZhengdongmenReference.py` — a camera above and off one end, aimed along the ridge. It
reproduced the user's viewport exactly, which is what made the cause unambiguous rather than a
guess.

## Maintained source and saved assets

- Generator: `ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py`
- Views: `Scripts/RenderZhengdongmenPreviews.py`, `Scripts/CaptureZhengdongmenReference.py`
- Saved meshes: `.../Meshes/SM_ZDM_RoofTile.uasset`, `SM_ZDM_Ridge.uasset`
- Blueprint: `.../BP_Zhengdongmen.uasset`

## Open

- **A GUI editor was open when the reimport ran.** The commandlet and the editor were both
  writing the project at once; the disk assets came out correct and the fresh-process validator
  confirms them, but the editor's in-memory copies of `SM_ZDM_RoofTile` and `BP_Zhengdongmen`
  were stale until it was closed. Check `pgrep -f UnrealEditor` before any writer — the earlier
  run in this session had the check and this one did not.
- No ridge ornaments (脊兽 / 吻兽). The reference render shows small finials at the main-ridge
  ends and the corners; they are a geometry change that would move the envelope, so they were
  left alone.
- The 194 MB of extracted source still sits in `Raw3DPacket/Zhengdongmen/` uncommitted.
