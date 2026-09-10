# Crunch Num.2 dash reapplication

Date: 2026-09-10

## Intent

Reapply the Crunch Num.2 long-dash fix after the Crunch asset rollback. The 2026-09-09 server log repeatedly showed `AM_Dash` resolving to the wrong skeleton, `PlayMontageAndWait` failing, and `AuraCrunchDash` ending immediately. That presentation failure prevented the fallback movement from running in the open game session.

## Changed behavior

- Added repaired target-owned `Content/Assets/Characters/Crunch/Animations/Abilities/AM_Dash_Reapplied.uasset`, restored from the known-good LFS object. It references `Crunch_SkeletonV4` and keeps the ability’s presentation asset independent of the reverted package.
- Updated `AuraCrunchDash` to load `AM_Dash_Reapplied.AM_Dash`.
- Montage playback is presentation-only. Missing, skeleton-mismatched, cancelled or interrupted montage playback no longer ends the ability; the notify/fallback path can still start GAS root-motion movement.
- The existing charge remains 1800 world units over 0.6 seconds, with CharacterMovement collision, authority-only damage, idempotent start, watchdog timeout and cleanup.
- Added reflected contract assertions for distance, duration and the target-skeleton montage, alongside travel/collision/cancellation coverage.

## Validation

- Isolated copy at `C:/Temp/AuraProjDashValidation0910` contained the patched source and repaired asset. `Build.bat AuraEditor Win64 Development C:/Temp/AuraProjDashValidation0910/Aura.uproject -waitmutex -NoHotReloadFromIDE` passed (354 actions).
- Isolated `UnrealEditor-Cmd.exe` run with `Automation RunTests Aura.Migration.Crunch.Dash.` passed with exit code 0. Log: `C:/Temp/AuraProjDashValidation0910/Saved/Logs/CrunchDashReappliedAsset.log`.
- Runtime evidence: 60 FPS = 1800 units, 30 FPS = 1800 units, blocking wall = 565 units, cancellation = 500 units; ability end and no residual movement passed.
- `git diff --check` passed for the edited text files.
- The normal workspace build was attempted but could not link because the open Unreal Editor held project DLLs (`LNK1104`). The isolated build avoids that lock and compiled the exact patched source.

## Limits

The old `AM_Dash.uasset` remains open/locked by the current Unreal Editor and was not overwritten. The C++ path now selects the new repaired package, so the editor must be restarted or rebuilt before it loads the new CDO. The isolated run is headless and local-authority; rendered feel, exact screen fraction and client/server play remain follow-up checks.

## Illustration

[Before/after flow](2026-09-10-crunch-num2-dash-reapply.svg)
