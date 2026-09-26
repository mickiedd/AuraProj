# Zhengdongmen: continuous timber walls, intact arch and fitted wooden doors

Date: 2026-09-25

[Visual summary](2026-09-25-zhengdongmen-continuous-walls-arch-door.svg)

## Intent

Tune BP_Zhengdongmen against the user's supplied Great East Gate reference, especially the hollow timber walls, broken arch surround and rectangular doors that did not reach the arch. Text on the supplied reference board was treated as reference content, not as instructions.

## Changed behavior

- Both storeys now have continuous timber backing on all four sides. Corners overlap; the upper boundary follows the sloping roof. Existing posts, balcony rails and window framing remain in front, with additional end-wall lattice.
- Filled the curved spandrel wedges above the arch through the wall depth. Increased the main arch to 128 segments and made tunnel ribs flush with the opening.
- Replaced the two short rectangular leaves and central opening with a closed double door following the same semicircle: opening radius 2.66 m, door radius 2.68 m, spring height 3.40 m. The 2 cm overlap is hidden in the frame. The door remains recessed inside the passage. This is a closed-door interpretation; the reference board depicts an open passage.
- Corrected UV projection on the new side walls and tunnel lining. Window infill uses timber instead of the studded DoorWood artwork. Door artwork is mapped once across the pair.
- In-place reimport of Stone, Wood, DoorWood and Iron; refreshed and saved the existing Blueprint components. Seven material groups, full-resolution Nanite fallback and complex structural collision remain. Building union remains 2870 × 1639 × 1933 cm; total 71,162 triangles.

## Maintained source and workflow

`ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py` derives from the supplied procedural generator. The original package is retained untouched. `Scripts/TuneZhengdongmenReference.py` writes the repaired staged meshes and a manifest; the preparation script invokes this repair after restaging the original package so future preparation cannot silently restore the defects. `Scripts/ApplyZhengdongmenReference.py` imports the four changed groups and refreshes the existing component references by mesh path.

## Visual comparison

Matched Blender views use the actual original/repaired source GLBs, not an illustration generated from a prompt:

| View | Before | After |
|---|---|---|
| Whole building | [Original](2026-09-25-zhengdongmen-continuous-walls-arch-door-before-three-quarter.png) | [Repaired](2026-09-25-zhengdongmen-continuous-walls-arch-door-after-three-quarter.png) |
| Door and arch | [Original](2026-09-25-zhengdongmen-continuous-walls-arch-door-before-arch-front.png) | [Repaired](2026-09-25-zhengdongmen-continuous-walls-arch-door-after-arch-front.png) |
| Timber enclosure | [Original](2026-09-25-zhengdongmen-continuous-walls-arch-door-before-pavilion-detail.png) | [Repaired](2026-09-25-zhengdongmen-continuous-walls-arch-door-after-pavilion-detail.png) |

## Validation

`ValidateZhengdongmenClosure.py` tests actual triangle intersections, with maximum hit distances so a rear wall cannot hide a missing front wall. [Raw results](2026-09-25-zhengdongmen-continuous-walls-arch-door-validation.json).

| Region | Before gaps | After gaps | Samples |
|---|---:|---:|---:|
| Eight timber faces | 9,974 | 0 | 26,936 |
| Arch spandrels | 879 | 0 | 2,227 |
| Door perimeter and lower leaves | 2,492 | 0 | 4,728 |

Additionally, 1,681 passage rays before the closed door have no masonry obstruction (28 were blocked by projecting ribs before the repair).

Fresh-process `ValidateZhengdongmenLandmark.py`: passed, zero errors, zero warnings. Fresh-process `ValidateGuangzhouLandmarkShowcase.py`: passed, zero errors, zero warnings. Python compilation passed for changed scripts. The existing showcase map was not rebuilt by this repair.

The reference is an architectural illustration, not a measured model specification. This repair addresses continuity and door fit; it does not claim a pixel-perfect reconstruction of the board's roof tile shapes, weathering, or surrounding scene.

The already-open editor can retain the old meshes in memory; reload the affected assets or restart the editor before reviewing or saving that cached copy.

## Native Unreal visual review

Rendered the saved Blueprint in a separate UE 5.5 GUI process with live Metal RHI, warmed shaders and resident texture mips. Five captures completed; temporary actor count returned from 0 to 0, no map saved, process exited 0. [Front](2026-09-25-zhengdongmen-continuous-walls-arch-door-native-front.png) · [Arch and fitted door](2026-09-25-zhengdongmen-continuous-walls-arch-door-native-arch.png) · [Timber corner](2026-09-25-zhengdongmen-continuous-walls-arch-door-native-timber.png) · [Three-quarter](2026-09-25-zhengdongmen-continuous-walls-arch-door-native-three-quarter.png) · [Rear](2026-09-25-zhengdongmen-continuous-walls-arch-door-native-rear.png). The dark band around the recessed door is the shadowed barrel vault, not a through-hole.

Remote editor discovery was unavailable and native UI command input did not focus reliably, so this review used a dedicated temporary GUI process. The first immediate capture was rejected because shaders were not ready; preloading the material and texture mips resolved it. An intermediate review process failed during shutdown after writing images; the final runner defers exit until the next tick and completed with exit 0.
