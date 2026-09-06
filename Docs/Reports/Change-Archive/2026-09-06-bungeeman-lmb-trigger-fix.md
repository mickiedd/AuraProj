# BungeeMan LMB trigger fix — 2026-09-06

## Intent

Trace the BungeeMan role's LMB `FireGun` skill from input through the ability graph, verify the visual presentation, and repair the point where the ability stopped.

## Diagnosis

The fresh `Saved/Logs/Aura-backup-2026.09.06-06.13.24.log` trace showed that the click was routed correctly:

- `InputTag.LMB` reached the PlayerController and the Ability System Component.
- The ASC found `Abilities.Gun.Fire` and received target data.
- The graph then stopped at `WaitForMontageEvent` because `Event.Montage.FireGun` was not authored on `AM_BungeeMan_FireGun`.

The generic montage-notify name was insufficient. The graph checks for the project-specific `AN_MontageEvent` notify and its `EventTag` gameplay-tag property.

## Fix

- Authored `AM_BungeeMan_FireGun` on the BungeeMan skeleton and linked it from `Content/AbilityDefinitions/FireGun.xml`.
- Added the `AN_MontageEvent` branching-point notify with `EventTag=Event.Montage.FireGun` at the montage midpoint.
- Kept the reproducible editor commandlet responsible for repairing the notify and `ABP_Bungee`'s `StateMachine -> DefaultSlot -> Output` path.
- Added a focused asset regression test covering the montage tag, FireGun XML reference, and AnimGraph slot wiring.

## Validation

- AuraEditor Win64 Development build: exit 0, target up to date.
- `Saved/Logs/BungeeMontageNotifyFinal.log`: commandlet PASS, including `Notify=Event.Montage.FireGun`, with 0 errors.
- `Saved/Logs/BungeeLmbAutomationPostCommandlet.log`: `Aura.RoleBattle.BungeeMan.Presentation.LmbMontageGraph` Success, exit 0.
- `Scripts/test_role_battle_days_7_9.py`: PASS.
- Editor-rendered preview shows BungeeMan in the rifle firing pose with the weapon attached.
- `git diff --check`: passed.
- The broader `Aura.Abilities.PlayerSkillAuthoritySafety` test remains blocked by three unrelated malformed Captain XML files (`CaptainLineCharge.xml`, `CaptainReinforce.xml`, and `CaptainSweep.xml`); those files were not changed.
- A full post-fix live click capture was not available in the current headless/editor environment, so runtime success is supported by the pre-fix log diagnosis plus the repaired asset, commandlet, focused automation, and visual preview.
- Independent in-app Claude review was attempted twice, but the browser accessibility state timed out before the composer could be verified; local validation is the documented fallback.

## Visual summary

[Open the before/after diagram](./2026-09-06-bungeeman-lmb-trigger-fix.svg)
