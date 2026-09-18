# V5 reference tuning: winding, basis audit, and intactness review

Date: 2026-09-17. This revision applies the Unreal asset reference-tuning workflow to the three imported V5 building Blueprints. The attached Guidemen and Wuxianmen boards were used as visual evidence only; embedded labels and captions were not treated as instructions.

[Archived diagram](2026-09-17-v5-reference-winding-tuning.svg)

## Baseline

Before mutation, an immutable live Blueprint snapshot was written to `Saved/RawModelImport/V5/reference-tuning-baseline-20260917.json`. It records every HISM component, mesh path, instance count and local transform, component transform, material overrides, visibility, collision profile, shadow state, UV/section metadata, generated class, and the pre-existing dirty editor map (`/Temp/Untitled_10`). No map was saved or reset to take the snapshot.

Baseline payload:

- Guidemen: 47 components, 18,822 instances.
- Wuxianmen 4K Core: 7 components, 5,103 instances.
- Wuxianmen FullPBR: 7 components, 5,103 instances.

## Finding and repair

The source audit (`reference-tuning-source-audit-20260917.json`) examined all 48 Guidemen mesh definitions and both 7-mesh Wuxianmen scenes. The Guidemen roof-shell sheets have open boundaries and negative signed volumes, but are intentional four-sided visual roof sheets and were left unchanged. The Wuxianmen stone, wood, plaster, iron, tile, and plaque prototypes were consistently inward-wound (negative signed volume); the ridge prototype was already outward-wound.

For each Wuxianmen package, a private GLB variant was generated under `ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917/`. Triangle order was reversed only for the negative-volume prototypes. Vertex positions, UV indices, node transforms, mesh names, and the 5,103 authored scene nodes were preserved. Signed volumes changed from approximately `-1.000` to `+1.000` for the box-like prototypes; the ridge remained `+0.785`. Boundary edges and triangle counts were unchanged.

The variants were imported to package-local `Meshes/ReferenceTuned20260917` folders with the measured Wuxianmen `-90°` import contract. A zero-roll probe was intentionally rejected after it produced the depth/height swap and an incorrect visual orientation. Only the Wuxianmen Blueprint mesh references changed; Guidemen remains on its canonical zero-roll `Meshes/UprightSource` set.

## Binding and invariants

`RebindV5WindingVariants.py` rebound exactly seven Wuxianmen components per Blueprint to the private corrected meshes and restored the baseline tuned material overrides. Every component and instance transform hash matched the immutable baseline; component counts and instance counts remain 7/5,103 for each Wuxianmen Blueprint. Original source meshes and materials remain recoverable.

## Validation

Command:

```powershell
python Scripts/remote_run.py Scripts/ValidateV5ReferenceTuning.py
```

Result:

```text
V5_REFERENCE_TUNING_PASS Guidemen_V5_4K [5639.37109375, 1800.0, 2130.0] 18822
V5_REFERENCE_TUNING_PASS Wuxianmen_V5_4K_Core [3646.6473388671875, 2429.23095703125, 1700.17431640625] 5103
V5_REFERENCE_TUNING_PASS Wuxianmen_V5_FullPBR [3646.6473388671875, 2429.23095703125, 1700.17431640625] 5103
```

The post-bind validator checks exact payload coverage, private mesh roots, transform preservation, tuned material slots, two-sided surface materials, complex-as-simple collision, double-sided geometry, Nanite fallback requests, package-local dependency closure, plausible bounds, and intentional gate openings. The imported corrected GLBs passed the host-side source audit; direct UE render-buffer winding export is not exposed by the available remote Python API, so that limitation is recorded rather than inferred away.

## Visual review

Twenty-four 1600x1000 transient captures were taken: hero three-quarter, front, rear, both sides, low/under-eave, top, and arch-close views for each Blueprint. Temporary actors, lights, ground, and post-process volume were destroyed after capture. The dirty map was not saved. The views show one assembly per Blueprint, upright orientation, complete walls/parapets/roofs/timber/doors/plaques, readable openings, and no accidental see-through exterior surfaces. The central arches remain intentional architectural passages.

Evidence:

- `Saved/RawModelImport/V5/*-reference-tuning-{hero,front,rear,side_l,side_r,low,top,arch_close}.png`
- [Guidemen hero](../../Saved/RawModelImport/V5/Guidemen_V5_4K-reference-tuning-hero.png)
- [Wuxianmen Core hero](../../Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reference-tuning-hero.png)
- [Wuxianmen FullPBR hero](../../Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reference-tuning-hero.png)

## Boundaries

This pass does not remodel source architecture, retile walls, place assets in a showcase map, or certify cooked-build frame time and platform memory. Intentional open roof sheets and gate passages are not treated as Boolean manifold failures. The private zero-roll probe assets remain available for coordinate-audit evidence but are not referenced by any canonical Blueprint. No independent external reviewer was performed; the implementation packet contains the local evidence and concrete reviewer questions.
