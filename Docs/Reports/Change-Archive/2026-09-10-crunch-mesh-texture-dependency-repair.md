# Crunch mesh and texture dependency repair

Date: 2026-09-10

## Intent

Investigate the Crunch mesh/texture failure reported by the Unreal logs and repair it if the active Crunch presentation asset was pointing at unavailable packages.

## Confirmed failure

- `Saved/Logs/Aura-backup-2026.09.09-19.25.20.log:1712-1720` records `SM_CrunchV4` importing material classes, five Crunch material instances, `Crunch_Extents`, and `Crunch_ShadowCyl` from `/Game/ParagonCrunch/...` while those packages were absent from the AuraProj content mount.
- `Saved/Logs/Aura-backup-2026.09.09-19.25.20.log:1748-1782` records the corresponding unavailable-package errors and expected missing `.uasset` paths.

## Changed behavior

- Added `Scripts/RepairCrunchAssetDependencies.ps1`, which builds a recursive source dependency closure from the Crunch mesh/material seeds, copies it under `Content/Assets/Crunch`, and remaps `/Game/ParagonCrunch/` references to `/Game/Assets/Crunch/` without changing package byte lengths.
- Copied 82 source `.uasset` packages (517,233,873 bytes / 493.3 MiB), including 43 texture packages and 9 material packages.
- Repaired the four Crunch mesh packages, four Crunch skeleton packages, and `Anim_Tornado_Aura.uasset`; all repaired target packages now contain zero legacy `/Game/ParagonCrunch/` references.
- The active role configuration continues to use `/Game/Assets/Characters/Crunch/Meshes/SM_CrunchV4`.

## Validation

- Repair script: `packages=82; copiedMiB=493.3; repairedTargets=9`.
- Bundle scan: `82` packages, `0` files containing the old prefix.
- `RunCrunchMigration.ps1 -Stage Fast -Scenario Assets -RunId crunch-mesh-texture-repair-20260910`: PASS; source export preflight exit code `0`.
- `AuraValidateComboMontage -CrunchPresentation`: PASS; target montage loaded with `Crunch_SkeletonV4`, 4 sections, and 12 notifies.
- Direct Unreal/Python load probe: `SM_CrunchV4`, `Crunch_SkeletonV4`, and `Anim_Tornado_Aura` loaded `3/3`; the mesh resolved seven target-owned materials.
- `git diff --check`: no whitespace errors. Existing unrelated working-tree line-ending notices are preserved.

## Limits

The current Unreal log still reports old `/Game/ParagonCrunch/...` references in legacy `AM_Dash` and `AM_UpperCut` packages. Those animation packages are outside this mesh/texture dependency repair; no new mesh or texture load error was observed after the fix.

## Illustration

[Before/after dependency flow](2026-09-10-crunch-mesh-texture-dependency-repair.svg)
