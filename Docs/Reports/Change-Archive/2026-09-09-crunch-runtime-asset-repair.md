# Crunch runtime asset repair

Date: 2026-09-09

## Intent

Repair the Crunch runtime startup errors caused by invalid Git LFS placeholder files and verify that the active Aura presentation assets load under Unreal Engine 5.5.

## Changed behavior

- Regenerated the canonical Crunch V4 mesh, skeleton, locomotion, combo sequences, and combo montage as valid Unreal packages.
- Repackaged Dash, UpperCut, GroundBlast Casting, GroundBlast Targetting, and Tornado under their Aura target package names with the target Crunch skeleton.
- Removed source Crunch-plugin notify references from the translated skill montages; the native Aura gameplay-event timing fallbacks remain authoritative.
- Removed superseded, unreferenced legacy Crunch package placeholders that were being rejected by the asset registry at startup.
- Extended `AuraCreateCrunchPresentation` so future presentation regeneration repairs the five skill montages as well as the canonical combo presentation.

## Validation

- AuraEditor Mac Development build passed.
- `AuraCreateCrunchPresentation` completed with `[CrunchPresentation] PASS` and valid mesh, skeleton, locomotion, four combo sections, and twelve combo notifies.
- `AuraValidateComboMontage CrunchPresentation` passed with zero errors.
- `Aura.Migration.Crunch` automation passed all 13 tests, including `PresentationAssets`, `LocomotionGraph`, `RuntimePlayback`, skill contracts, and dash travel/collision.
- `git diff --check` passed.
- The remaining Python duplicate-name warning and imported source assets' empty engine-version warnings are pre-existing non-blocking editor warnings; no Crunch asset-load errors or critical errors remain.

## Visual record

[Change diagram](2026-09-09-crunch-runtime-asset-repair.svg)
