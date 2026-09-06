# Crunch Num.2 dash distance

Date: 2026-09-06

## Intent and evidence

Give Crunch a substantial charge. Saved/Logs/Aura.log confirms InputTag.2 activates AuraCrunchDash. Old code used walking input for 0.35 seconds with a 1600 units/s speed cap: at most 560 units before acceleration losses. Montage blend-out could also end movement. Old logs do not measure actual distance.

## Behavior

Default distance is 1800 units over 0.6 seconds, using a GAS constant-force override root-motion task and CharacterMovement collision. Normal montage completion no longer ends the charge; interruption and cancellation still do. Montage playback is skipped without an animation instance. Animation root-motion mode is saved, ignored during the charge, and restored on cleanup. Walk speed and braking settings are untouched. The start notify and 0.248-second fallback are idempotent; a duration-derived watchdog bounds execution. Existing authority-only damage and per-target hit ledger remain. Start/stop logs report requested and actual travel.

## Validation

- `Build.bat AuraEditor Win64 Development C:/Git/AuraProj/Aura.uproject -waitmutex -NoHotReloadFromIDE` passed using C:/Git/UE_5.5.
- UnrealEditor-Cmd.exe with `-unattended -nullrhi -nosound "-ExecCmds=Automation RunTests Aura.Migration.Crunch.;Quit" "-TestExit=Automation Test Queue Empty"` passed; exit code 0. Evidence: Saved/Logs/CrunchDashFinal.log and Saved/Reports/CrunchDash/index.json.
- New test `Aura.Migration.Crunch.Dash.TravelCollisionAndCancel` exercises real ASC activation and CharacterMovement on a box floor. Measured 1800 units at 30 and 60 FPS, 565 units against a wall, and 500 units when cancelled. Ability end, removal of its root-motion source, and no residual travel are asserted.
- Early fixture runs exposed missing-animation cancellation and the synthetic frame timer guard; corrected before the passing run.
- Scoped diff whitespace check passed.

## Review and limits

The in-app-claude-handoff opened Google AI Studio but timed out. State inspection found the tab; reacquiring it also timed out. No packet was typed or sent. External review is unavailable; used the documented local fallback with engine source review, compilation and runtime tests.

The existing DebugGame editor/server remain open with old binaries. Development contains this change; DebugGame needs rebuilding after that editor closes. No Blueprint assets were edited. Tests are headless and authority-local: exact half-screen projection, rendered animation feel, victim damage and client/server play were not verified. 1800 units is initial world-space tuning, not a guarantee at every zoom.

[Before/after diagram](2026-09-06-crunch-num2-dash-distance.svg)
