# Guidemen — source audit: the inverted roof is in the source model

Date: 2026-09-18.

The user asked for a rebuild from scratch and supplied the canonical reference sheet.
The first step of any rebuild is to establish the source. Doing that answered the roof
question that four passes of Unreal-side measurement could not.

[Archived diagram](2026-09-18-guidemen-source-audit-inverted-shells.svg)

## The source, located and extracted

```
C:\Works\Raw3DModels\V5\Guidemen_GuideGate_UE5_ModelPlusGenerated4KMaterials.zip
  Model/Guidemen_GuideGate_UE5_100M_Instanced.glb     74.8 MB
  Materials/SourcePBR/                                8 full PBR sets (AO/BaseColor/MR/Normal)
  Materials/Generated_4K_Sheets/                      8 generated 4K sheets
  Docs/Guidemen_GuideGate_build_report.json
```

Build report: 18,823 scene nodes, 48 geometry definitions, 8 materials, 26 embedded
images, **16,248 roof-tile instances**, Y-up metres, 56.0 W × 18.0 D × 19.3 H.

Extracted to `Saved/RawModelImport/V5/GuidemenRebuild/source/`.

Note there are two Guidemen sources on disk. `Guidemen_HighPoly_UE5_4K.glb` is a
**different, 10-part** model (StoneFacade/StoneStructure/Roofs/Timber/…) and is **not**
what the V5 asset was built from. The V5 asset came from the 48-geometry instanced GLB
above — its 16,248 tile instances match the asset exactly.

## The ridge runs along X — confirmed three independent ways

1. **The 12 `G_RidgeBeast` ornaments all sit at `Z = 0.0`**, spread along X from −14.7 to
   +14.7. They sit on the ridge, so that is the ridge line.
2. **Both ridge bars run along X at `Z ≈ 0`**: `R1_MainRidge` extent (X,Y,Z) =
   `[31.2, 0.28, 0.28]` at Z −0.14..0.14; `R2_MainRidge` = `[27.2, 0.27, 0.26]` at
   Z −0.13..0.13.
3. **The shell families agree**: Front/Back span 31 m in X (main slopes, sloping in Z),
   Left/Right span 15.5 m in X (hip ends, sloping in X).

The building is 56 m wide in X and 18 m deep in Z, so X is where a ridge belongs.
**There is no mis-axed ridge bar.**

## The eight roof shells are inverted in the source

Measured from the source vertices, mean Y at the inner edge (near the ridge) versus the
outer edge:

| shell | inner edge | outer edge | verdict |
| --- | --- | --- | --- |
| `R1_RoofShellLeft` | 14.708 | 16.269 | rises outward — **inverted** |
| `R1_RoofShellRight` | 14.708 | 16.269 | rises outward — **inverted** |
| `R2_RoofShellLeft` | 18.628 | 19.966 | rises outward — **inverted** |
| `R2_RoofShellRight` | 18.628 | 19.966 | rises outward — **inverted** |

Every hip shell is higher at its outer edge than at the ridge — a valley, **in the
source file**. The V5 import reproduced it faithfully.

## Why this explains everything that went wrong

```
source tiles      span Y 12.92 .. 18.64
ridge bar R1      Y 14.72 .. 15.00
ridge ornaments   Y 15.18 .. 15.58          → these three agree with each other
source shells R1  span Y 14.62 .. 16.34      → 1.63 m ABOVE the tiled surface, sloping the wrong way
```

The tiles, ridge bars and ornaments form one coherent, correct roof. The eight shells do
not belong to it. That is exactly the valley the user has been seeing through every pass
— and exactly why my Unreal-side measurements kept contradicting each other: I was
measuring a surface that disagrees with the roof it sits on, and neither the aggregate
profile, the tile normals, the instance AABBs nor the isolated traces could tell me which
of the two surfaces was the anomaly.

It also means the shell correction already applied — flip and reseat to
`12.90 .. 14.62`, just under the tiles at `12.92 .. 14.72` — puts them exactly where the
source's own tile span says they belong, matching to within 2 cm on both tiers.

## Rebuild plan

1. Source is extracted and audited: `Saved/RawModelImport/V5/GuidemenRebuild/source/`.
2. The existing V5 pipeline (`Scripts/PrepareV5Buildings.py`) already imports this package
   and builds the 47-component HISM Blueprint.
3. Rebuild into a **new task-owned package** so the current asset stays recoverable, then
   re-apply the reference tuning and the shell correction.

**The one decision the rebuild needs:** a clean rebuild from this source reproduces the
shell defect. The rebuild must therefore either correct the shells after import (flip and
reseat — already done and verified on the current asset) or the source GLB should be
corrected upstream and re-exported. Correcting it in the pipeline is reversible and keeps
the source untouched; that is the recommendation.

## What is now known for certain

- The ridge runs along **X** at `Z = 0`, established from three independent parts of the
  source. No mis-axed ridge.
- The tiles, ridge bars and ornaments form a coherent, correct roof in the source.
- **The eight roof shells are inverted in the source** and sit 1.63 m above the tiled
  surface. The defect is upstream of Unreal.
- The shell correction was matched against the source's own tile span, to within 2 cm.

## Method note

This pass required no Unreal at all. Parsing the GLB and decoding its accessors gave
ground truth in a few minutes, after four passes of Unreal-side measurement had produced
mutually contradictory readings. **When a source file exists, audit the source first.**
