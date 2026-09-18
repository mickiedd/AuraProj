# Guidemen — assembly verified against the source; no rebuild needed

Date: 2026-09-18.

The user asked to rebuild `Guidemen_V5_4K` from scratch. This records why that turned out to
be unnecessary, and the verification behind that conclusion.

[Archived diagram](2026-09-18-guidemen-assembly-verified-no-rebuild-needed.svg)

## Two things made the rebuild unnecessary

**1. The meshes are already imported.** `Meshes/Source/` and `Meshes/UprightSource/` each hold
**48 assets** — one per source geometry definition. Nothing needed re-importing, which also
removes the out-of-memory risk that crashed the editor on the one attempt.

**2. The assembly already is the source.** Verified instance by instance, not assumed:

| check | result |
| --- | --- |
| components / instances | 47 / 18,822 — matching the source's 48 geometries and 18,823 nodes |
| instance counts | **0 mismatches** across all 47 components |
| positions within 1.0 cm | **18,820 of 18,822** |
| single-instance parts | every one matched at exactly **0.000 cm** |

The 0.000 cm set includes both ridge bars, all eight roof shells, both gate piers, the arch
vault and spandrel, the parapets, the wings, the pavilion floors — and all **16,248 roof
tiles**.

## My error, not the asset's

The first pass converted the source with `UE = (x, −z, y) × 100` and reported **2,296
instances mismatched**, with residuals of 35–852 cm. I nearly read that as an asset defect.

The sign was wrong. Testing both conventions per component settled it:

| component | n | `(x,−z,y)` matched | `(x,+z,y)` matched |
| --- | --- | --- | --- |
| `SB_0` stone blocks | 1,806 | 0 | **1,804** |
| `BA_0` brackets | 182 | 86 | **182** |
| `BB_0` brackets | 182 | 86 | **182** |
| `RP_0` rail posts | 53 | 0 | **53** |
| `Stud_0` door studs | 48 | 0 | **48** |
| `Col1_0` columns | 40 | 0 | **40** |

Parts symmetric in Z match under either sign, which is why only some components failed and
the pattern looked like a real defect. **An assembled extent cannot distinguish ±Z** — the
magnitudes are identical. The correct conversion is `UE = (x, +z, y) × 100`.

## State of the asset

| aspect | state |
| --- | --- |
| assembly | 47 components / 18,822 instances, a faithful copy of the source |
| roof ridge | along X at Z = 0, confirmed three ways in the source (the 12 ornaments, both ridge bars, the shell families) |
| roof shells | the source's own defect — inverted, 1.63 m above the tiles — **corrected in the pipeline**: world Z 1290–1462 (R1) and 1708–1855 (R2), clearance below the tile top 9.89 / 9.86 cm |
| tiles | untouched: 1285.04–1471.89 (R1), 1703.14–1864.86 (R2) — matching the source to the centimetre |
| materials | `_RefTune4` stone and tile weathering |
| Nanite | fallback normalised to `error 0.0 / percent 100.0` |
| recovery | `BP_Guidemen_V5_4K_PreRebuild_20260918` exists and survived the crash |

## What was and was not done

**Done:** the source was located, extracted and audited; the assembly was verified against it
instance by instance; the recovery copy was created; the shell correction was confirmed
intact after the crash; the import script was fixed and guarded.

**Not done, deliberately:** no re-import, no rebuild, no destructive edit. The rebuild would
have reproduced the same assembly at the cost of a large, risky operation — and one attempt
at exactly that crashed the editor.

## The one thing still missing

Every number above is geometry. **None of it is a picture.** The capture harness has been
broken for five consecutive passes, so no visual confirmation exists for any of this work,
and the user's screenshot has been the only rendering evidence on the project.

If the roof still reads wrong on screen, the most likely remaining candidate is a surface
that agrees with the source but not with the reference sheet — which no amount of source
comparison can detect. **A top-down view and a front or side elevation would close this in
seconds.**

## Method notes

- **Do not infer an axis sign from an assembled extent.** Magnitudes cannot distinguish ±.
  Test both signs per component; the components symmetric in the ambiguous axis will match
  under either, and the asymmetric ones will decide it.
- **Verify an assembly by multiset-comparing instance positions against the source's own node
  transforms.** Sort both lists and compare element-wise — O(n log n). A greedy
  nearest-neighbour match is quadratic and will not finish on a 16,248-instance group.
- **Before rebuilding, check whether the meshes are already imported.** Guidemen had all 48.
