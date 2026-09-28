# Zhengdongmen: the roof ridges get their own material

Date: 2026-09-26

[Visual summary](./2026-09-26-zhengdongmen-ridge-material.svg)

## Intent

The user, looking at `BP_Zhengdongmen` in the editor: "the 屋脊 parts of the roof should use
different textures."

They were right, and the viewport showed why. The main ridge, the four hip ridges and the
eight upturned eave corners were all built with the **`RoofTile` material**, so a hip cap
carried a corrugated tile field along its length and read as a continuation of the roof
rather than as a ridge. Compounding it, `beam()` assigned UVs from a **world-space XZ planar
projection**, so on a diagonal hip the tile field's ribs and courses smeared across the cap
instead of running down it.

The reference board is **interpretive guidance, not a measured survey**. Its 3D render shows
the 屋脊 as narrow, smooth, slightly darker bands with a distinct silhouette, not as tiled
surface.

## Changed behaviour

**A new `Ridge` material group.** The supplied package has no artwork for it, so the whole
set is authored here. The ridges moved onto it:

| part | before | after |
|---|---|---|
| `Upper_roof_ridge` (正脊) | RoofTile | **Ridge** |
| `Lower_diagonal_hip_caps` / `Upper_diagonal_hip_caps` (垂脊), 8 beams | RoofTile | **Ridge** |
| `Upturned_eave_corners` / `Upper_upturned_corners` (翼角), 8 beams | RoofTile | **Ridge** |

**The section became a ridge cap.** A new `ridge_beam` sweeps a half-round 脊瓦 section — a
flat base carrying a semicircle — instead of the square tube `beam` produced. It is sized to
**fill the same bounding box** (a semicircle of radius `width/2` on a `width/2` base), which
is the point: the Ridge part measures 2697.36 × 1408.97 × 722.15 cm, *identical* to the
`RoofTile` box it was carved out of, so the building's envelope did not move. A square tube
swept along a hip is a flat-sided beam; the viewport showed exactly that.

**The UVs follow the ridge.** `ridge_beam` maps **u along the ridge** at 1.5 m per repeat and
**v once around the section**. `beam()` gained an `uv='axis'` mode on the same principle, and
its section "up" is now kept pointing up — flipping both section axes leaves the eight
corners in exactly the same places, so it changes no geometry, but without it the horizontal
ridge beams had their "up" pointing down and a section-following UV would land on the wrong
face.

**The texture adds only what the mesh does not model.** `MakeZhengdongmenRidgeTextures.py`
authors all six maps at 1024 × 512, registered to that mapping: four ridge-tile joints per
repeat (one every 0.375 m) with a matching groove in the height map, and weathering shaded
across the section — dirt gathering low, washed bright over the crest at v 0.66. It
deliberately does **not** paint a second crown: the geometry already has one, and doubling a
profile the mesh models is the defect this project has hit before.

## Validation

- `Scripts/PreflightZhengdongmenGlb.py`: `passed`, 8 parts. Union **2870 × 1639 × 1933.12 cm**
  — unchanged to the centimetre, which is the check that the cap really does fill the old box.
- `Scripts/ValidateZhengdongmenRoofTiling.py`: `passed`; the roof-tile courses are untouched.
- `Scripts/ValidateZhengdongmenLandmark.py`, fresh process: **0 errors / 0 warnings**. All
  **8/8 parts** match the tuned source exactly (Ridge 680 triangles), and `MI_Ridge` is bound
  with all five texture parameters pointing at the `T_ZDM_Ridge_*` assets.
- Gate geometry **337,154 → 337,630 triangles** (Ridge 680; RoofTile 289,852 → 289,648).
- Native Unreal/Metal captures, eight views including a new straight-on ridge view. Cleanup
  0 → 0.

| | |
|---|---|
| [Before — the user's viewport, ridge sharing RoofTile](./2026-09-26-zhengdongmen-ridge-material-before-viewport.png) | [After — native ridge and eave corner](./2026-09-26-zhengdongmen-ridge-material-native-ridge.png) |
| [After — native front](./2026-09-26-zhengdongmen-ridge-material-native-front.png) | [After — native three-quarter](./2026-09-26-zhengdongmen-ridge-material-native-three-quarter.png) |

Source-space render of the same hip: [ridge-corner](./2026-09-26-zhengdongmen-ridge-material-source-ridge-corner.png).

## Two defects found on the way

- **A new group has no material instance.** The apply path only reimported meshes and rebound
  components, and material instances are built by the import script's own `main()`, which it
  does not run — so `MI_Ridge` did not exist and the Ridge mesh could not be bound. It now
  calls the import's idempotent `build_instances(master)`, which creates the missing instance
  and re-binds every group (picking up the re-authored Sign maps at the same time).
- **The Ridge mesh imported with degenerate tangent bases.** The end caps of a ridge beam are
  perpendicular to its axis, so the default world-XZ projection gave every one of their
  vertices the same `u` — the engine reported "degenerate tangent bases which will result in
  incorrect shading" and "nearly zero bi-normals". The caps now take a planar UV in the
  section's own plane; the warnings are gone (0, from 2).

## Why the Blueprint was migrated in place

`CreateZhengdongmenLandmarkBlueprint.py` would have detected the 7-vs-8 component count,
force-deleted the Blueprint and recreated it — and `L_GuangzhouLandmarkShowcase` holds a
reference to its generated class. `ApplyZhengdongmenReference.py` instead **adds the missing
HISM to the existing asset**, so the asset identity survives. `HISM_Ridge` is present on the
root `LevelInstanceComponent`, carrying `SM_ZDM_Ridge` and `MI_Ridge`.

## Maintained source and saved assets

- Generator: `ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py`
  (`ridge_beam`, `MATERIALS` += `Ridge`)
- Ridge textures: `Scripts/MakeZhengdongmenRidgeTextures.py`, wired into
  `Scripts/PrepareZhengdongmenPackage.py` as a generated group
- Group threaded through `PrepareZhengdongmenPackage.py`, `ImportZhengdongmenLandmark.py`,
  `CreateZhengdongmenLandmarkBlueprint.py`, `ValidateZhengdongmenLandmark.py`,
  `PreflightZhengdongmenGlb.py`, `ApplyZhengdongmenReference.py`,
  `CaptureZhengdongmenReference.py`, `RunZhengdongmenNativeReview.py`
- Saved mesh: `Content/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/Meshes/SM_ZDM_Ridge.uasset`
- Material: `.../Materials/MI_Ridge.uasset`; Blueprint: `.../BP_Zhengdongmen.uasset`

## Open

- The ridges carry no ridge ornaments (脊兽 / 吻兽). The reference render shows small finials
  at the main ridge ends and the corners; the user asked about textures, so none were added.
- The 194 MB of extracted source still sits in `Raw3DPacket/Zhengdongmen/` uncommitted.
