# BungeeMan LMB gun montage fix — 2026-09-06

## Intent

Visually inspect the BungeeMan role and repair the missing gun-holding presentation for its LMB `FireGun` skill.

## Changed behavior

- Added `AM_BungeeMan_FireGun`, authored from `Fire_Rifle_Hip` and bound to the BungeeMan skeleton.
- Updated `Content/AbilityDefinitions/FireGun.xml` so `PlayMontage` uses the BungeeMan montage instead of the Aura montage.
- Updated `ABP_Bungee` so its AnimGraph evaluates `StateMachine -> DefaultSlot -> Output`, allowing the FireGun montage to override the base pose.
- Added `AuraEnsureBungeeMontageSlot` so the graph repair is reproducible and guarded by a focused asset test.

## Validation

- AuraEditor Win64 Development build passed after adding the commandlet and regression test.
- `AuraEnsureBungeeMontageSlot` returned `[BungeeMontageSlot] PASS`.
- `Aura.RoleBattle.BungeeMan.Presentation.LmbMontageGraph` passed with exit code 0.
- `Scripts/test_role_battle_days_7_9.py` passed.
- Editor-rendered before/after previews were captured; the BungeeMan body and attached rifle are visible in the repaired presentation setup. A full live gameplay frame-by-frame capture was not available.
- Independent in-app Claude review was attempted twice, but the in-app browser accessibility state timed out before a composer could be verified; local validation is the fallback for this job.
- `git diff --check` passed. The unrelated `Scripts/test_role_battle_review_fixes.py` still expects 9 XML definitions while this checkout contains 12; that pre-existing inventory mismatch was not changed.

## Visual summary

[Open the before/after diagram](./2026-09-06-bungeeman-lmb-gun-montage-fix.svg)
