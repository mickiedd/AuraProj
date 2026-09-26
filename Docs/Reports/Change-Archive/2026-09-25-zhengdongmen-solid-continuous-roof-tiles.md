# Zhengdongmen: continuous, modeled roof tiles

Date: 2026-09-25

[Visual summary](./2026-09-25-zhengdongmen-solid-continuous-roof-tiles.svg)

## Intent

The supplied viewport showed incomplete roof courses that read as a thin surface. Build the roof tiles as actual 3D pieces, close the gaps, and keep the mesh practical to import and render.

## Changed behavior

- Generated **2,998 overlapping curved tile shells** over all eight lower and upper roof slopes, including the four hip faces that lacked coverage. Courses are staggered, and both lateral and downslope pitches have overlap.
- Every tile is a closed 4 cm shell with a curved crown, underside and side rails. Shared vertices form its faces: **36 vertices and 68 triangles per tile**. The one combined roof mesh contains 203,864 tile triangles and 204,084 roof-mesh triangles including the eave/hip pieces. The complete gate has 251,386 triangles.
- Kept the repeated tile geometry in one mesh group, with per-course UVs and the existing roof material. A per-tile topology check confirms each shell is manifold; the course validator confirms overlap on all eight slopes.
- Reimported the roof mesh and saved the existing BP_Zhengdongmen components in Unreal. The source generator and tuning stage retain the repair for future package rebuilds.

## Visual review

Native Unreal/Metal captures of the saved Blueprint:

- [Three-quarter roof and gate](./2026-09-25-zhengdongmen-solid-continuous-roof-tiles-native-three-quarter.png)
- [Roof and pavilion detail](./2026-09-25-zhengdongmen-solid-continuous-roof-tiles-native-timber.png)
- [Front roof coverage](./2026-09-25-zhengdongmen-solid-continuous-roof-tiles-native-front.png)

The adjacent illustration diagrams the original gap/surface issue and the modeled solution; it does not substitute for these engine captures.

## Validation

- `Scripts/ValidateZhengdongmenRoofTiling.py`: passed; 2,998 tiles, eight slopes, overlap on every course, 36 vertices / 68 triangles per shell, and all shell edges used exactly twice. [Detailed results](../../../Saved/Reports/Zhengdongmen/roof-course-validation.json).
- Fresh Unreal `Scripts/ValidateZhengdongmenLandmark.py`: passed with zero errors and zero warnings; total geometry 251,386 triangles. [Validation report](../../../Saved/RawModelImport/zhengdongmen-validation.json).
- Native review completed on the saved Blueprint; temporary actors cleaned up (0 before / 0 after).

## Maintained source and saved asset

- Generator: `ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py`
- Course tuning: `Scripts/TuneZhengdongmenReference.py`
- Saved mesh: `Content/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/Meshes/SM_ZDM_RoofTile.uasset`
- Blueprint: `Content/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/BP_Zhengdongmen.uasset`
