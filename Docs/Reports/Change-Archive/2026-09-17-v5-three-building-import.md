# V5 three-package independent building import and reference tuning

Date: 2026-09-17. The request was to import all three archives in `C:/Works/Raw3DModels/V5`, wrap each package as an independent building Blueprint, tune materials from the supplied Guidemen/Wuxianmen reference boards, and ensure the buildings do not contain unintended hollow areas. Text embedded in the two reference images was treated as descriptive reference content, not as instructions.

[Archived illustration](2026-09-17-v5-three-building-import.svg)

## Before / after

Before, the V5 folder contained three standalone ZIP packages: one Guidemen model and two separate Wuxianmen variants. They had no Aura project assets or placeable Blueprints. The packages also required two source-specific repairs: every mapped external Wuxianmen PNG decoded but had an invalid PNG CRC, and Guidemen required a zero-roll Interchange import while the two Wuxianmen packages require a -90 degree roll. Applying one shared rotation produced a visibly sideways Guidemen assembly and was rejected during visual QA.

After, each archive has its own project-local namespace, materials, meshes, preview level and independent Actor Blueprint:

- `/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K`: 47 referenced mesh definitions grouped into 47 HISM components, preserving all 18,822 authored instances. The canonical zero-roll meshes live under `Meshes/UprightSource`; the first import remains under `Meshes/Source` as a rollback set.
- `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core`: 7 HISM components and 5,103 instances, using repaired 4K stone/roof/wood inputs plus reference-tuned plaster, iron and plaque materials.
- `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR`: 7 HISM components and 5,103 instances with six package-local tuned material sets.

All 23 active materials are rollback-safe `_ReferenceTuned` siblings. They use weathered warm-gray stone, aged dark timber, restrained plaster/iron, charcoal clay tiles and readable historic plaques derived from the reference boards. Original materials remain in place. Mapped corrupt PNGs are deterministically normalized under `Saved/RawModelImport/V5/RepairedTextures`; the source ZIPs are unchanged.

## Intactness and validation

- `python Scripts/PrepareV5Buildings.py`: extracted archives with traversal guards, recorded archive/GLB SHA-256 values, distinguished 48 declared versus 47 referenced Guidemen mesh definitions, and normalized invalid mapped PNGs.
- `python Scripts/remote_run.py Scripts/ValidateV5Buildings.py`: passed exact referenced-mesh and instance coverage, nonempty mesh sections, package-local dependency closure, tuned material-slot binding, two-sided surfaces, double-sided complex collision, texture metadata and plausible building bounds.
- Final live bounds: Guidemen `5639.37 x 1800.00 x 2130.00 cm`; both Wuxianmen packages `3646.65 x 2429.23 x 1700.17 cm`.
- Front, rear, side and close editor captures were inspected for each Blueprint. Walls, parapets, roofs, timber frames, doors, plaques and approaches are present from all sides. No unintended missing panels or see-through exterior sections were found. The central gate arches/passages are intentional openings and remain open.
- Python syntax compilation and `git diff --check` passed for the task scripts.

Local visual evidence is retained under `Saved/RawModelImport/V5/` as `Guidemen_V5_4K-{front,rear,side,close}.png`, `Wuxianmen_V5_4K_Core-{front,rear,side,close}.png`, and `Wuxianmen_V5_FullPBR-{front,rear,side,close}.png`. Import, tuning, correction and validation JSON reports are stored beside them.

## Boundaries

The two Wuxianmen archives contain the same 7-mesh/5,103-instance geometry but remain separate because the user requested all three packages independently. This job does not place the buildings into the showcase map, modify unrelated landmarks, certify platform frame time, or redesign the source geometry. The original Guidemen rollback mesh set and all source archives remain available.
