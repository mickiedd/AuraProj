# NullRHI independent-review remediation

## Intent

Apply the confirmed findings from `2026-09-07-nullrhi-launch-blockers-independent-validation.md` without changing the separate fail-closed persistence repair or unrelated dirty worktree changes.

## Findings addressed

1. The Tornado test no longer uses the editor-only animation data model. It now checks the runtime compressed track-to-skeleton map and validates each mapped bone index against the animation skeleton, preserving the original contract that the spin contains authored bone tracks.
2. The runtime ability metadata test now loads through `UAuraAbilitySystemLibrary::LoadAbilityInfoFromJSON()`, holds the registry strongly, forces garbage collection, retrieves the same icon/material references, and touches their paths after collection. This directly covers the original GC failure mechanism.
3. `RunClientNullRHI.bat` now supports `-dry-run`, and `Scripts/Test-RunClientNullRHI.ps1` runs a six-case matrix for explicit override, EngineAssociation precedence, stale environment fallback ordering, environment fallback, and fail-closed invalid/unresolved cases. The fixture supplies an unresolvable EngineAssociation without touching the real project file.

## Scope boundaries

- `AbilityInfoMap` remains `UPROPERTY(Transient)`; no persistence readiness guard or default persistence record was changed.
- The checkout already contained unrelated dirty Aura/game/editor files and earlier archive entries. They were preserved and are not represented as part of this remediation.
- No Blueprint asset was edited, so no Blueprint visual check was required.

## Validation

- `C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat Aura Win64 Development C:\Git\AuraProj\Aura.uproject -WaitMutex` — PASS, exit 0.
- Same `Build.bat` command for `AuraEditor Win64 Development` — PASS, exit 0; the editor target was already up to date after the remediation source set.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Test-RunClientNullRHI.ps1` — PASS; all six launcher cases passed and no editor process was spawned.
- `UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests Aura.Abilities.Metadata.RuntimeAssetReferencesSurviveGC; Quit' ...` — PASS, report `Saved/Reports/NullRHIReviewMetadataFinal/index.json`, 1/1, zero warnings/errors.
- `UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests Aura.Migration.Crunch.Tornado.DamagePresentationAndCleanup; Quit' ...` — PASS, report `Saved/Reports/NullRHIReviewTornado/index.json`, success with known asset-load warnings and zero errors.
- `UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests Aura.RoleBattle.NullRhi.RoleLoginFlowContract; Quit' ...` — PASS, report `Saved/Reports/NullRHIReviewLauncherContract/index.json`, 1/1, zero warnings/errors.
- `git diff --check` — PASS; only expected line-ending normalization warnings were emitted for the mixed dirty worktree.

## Visual summary

[Diagram](2026-09-07-nullrhi-review-remediation.svg)
