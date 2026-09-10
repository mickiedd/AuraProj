# Dedicated-server log optimization reapplied

Intent: restore the dedicated-server logging reductions after the previous optimization was absent from the current checkout.

## Changes

- Civilian animation diagnostics now use a separate `LogAuraAnimationDiagnostics` category at `Verbose`, disabled by default. The early guard avoids animation inspection and bookkeeping unless explicitly enabled. Opt-in samples are capped at one per pawn per real-time second, use weak pawn keys, and prune destroyed pawns. Animation playback time no longer defeats the throttle.
- Successful `GetAbilityTagFromSpec` tracing is `Verbose`; unresolved tags remain `Warning`.
- Successful civilian wander destination tracing is `Verbose`; navigation failures remain `Warning`.
- Replicated activity markers receive a `USceneComponent` root so relevancy checks have a valid world-space transform and stop the repeated missing-root warning.

## Evidence

The prior dedicated-server analysis recorded 536,657 lines / 244,565,022 bytes in a backup, including 519,778 animation dumps and 13,543 routine wander messages. The latest analyzed server run recorded 35,287 lines, 18,759 animation dumps, 413 wander messages, and 13,502 missing-root warnings. Those historical log files are no longer present in the current checkout, but the counts are preserved in the prior validation artifact.

## Validation

- PASS: `Build.bat Aura Win64 Development Aura.uproject -WaitMutex -NoHotReloadFromIDE` (standalone target; existing GAS deprecation warnings only).
- PASS: `Build.bat AuraEditor Win64 Development Aura.uproject -WaitMutex -NoHotReloadFromIDE` (editor target; existing GAS deprecation warnings only).
- PASS: `UnrealEditor-Cmd.exe ... -NullRHI -ExecCmds="Automation RunTests Aura.ServerLogging" -TestExit="Automation Test Queue Empty"`: `AnimationDiagnosticsDefaultOff` and `MarkerTransformAndRelevancy`, 2/2 successful.
- PARTIAL (bounded launch): rebuilt standalone `Aura.exe` produced zero animation, wander, ASC lookup, and missing-root entries, but stopped before opening the server port on an unrelated corrupt engine package (`/Engine/EngineMaterials/WorldGridMaterial`, seek past end of file).
- No managed server was restarted. No global `LogNet` or `LogAura` warning/error threshold was suppressed.

## Operation

Enable animation probes with `-LogCmds="LogAuraAnimationDiagnostics Verbose"` or `Log LogAuraAnimationDiagnostics Verbose`. Routine wander and successful ASC lookup traces remain available with `Log LogAura Verbose`.

[Visual summary](2026-09-10-dedicated-server-log-optimization-reapply.svg)
