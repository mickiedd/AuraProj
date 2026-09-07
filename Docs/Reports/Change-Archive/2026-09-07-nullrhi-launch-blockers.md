# NullRHI launch blockers and verified fix

## Intent

Run `Launch NullRHI Client (Headless)`, identify why the attached Missing Aura Modules gate blocked startup, repair the follow-on runtime crash exposed by a matching-engine launch, and verify the role-aware headless path through `StartupMap`.

## Root causes

1. The inherited `UE_EDITOR_EXE` environment variable pointed to `C:\Git\UnrealEngine-5.5` (`5.5.4`), while the project’s registered `EngineAssociation=5.5` checkout was `C:\Git\UE_5.5` (`CL 40574608`). The wrong editor reported every Aura/plugin module as missing or incompatible.
2. After selecting the matching engine, the client reached the battleground but later crashed in `AAuraHUD::HandleAbilityInfoForWebUI`. `URuntimeAbilityInfo` was rooted, but its `AbilityInfoMap` was not reflected, so its JSON-resolved texture/material references were invisible to Unreal’s garbage collector and could become stale.
3. The rebuild then exposed a separate test portability issue: `AuraCrunchTornadoTests.cpp` called the editor-only `UAnimSequenceBase::GetDataModel()` API from a runtime target.

## Changed behavior

- `RunClientNullRHI.bat` now resolves the project-associated registered UE 5.5 editor before using an inherited `UE_EDITOR_EXE` fallback. An explicit `-editor-exe` remains authoritative, and `-role <roleId>` continues to forward the selected role.
- `URuntimeAbilityInfo::AbilityInfoMap` is now `UPROPERTY(Transient)`, making the asset references visible to GC for the lifetime of the rooted runtime registry.
- `AuraAbilityInfoTests.cpp` asserts that the map is reflected and transient.
- `AuraCrunchTornadoTests.cpp` uses the runtime-safe `GetNumberOfSampledKeys()` query.

## Validation

- `Aura Win64 Development` using `C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat` — PASS, exit 0.
- `AuraEditor Win64 Development` using the same engine — PASS, exit 0; `UnrealEditor-Aura.dll` linked after closing only the project editor/server and stale crash-reporter processes that held the DLL.
- `UnrealEditor-Cmd.exe -NullRHI` automation — `Aura.Abilities.Metadata.RuntimeBoundary` 1/1 and `Aura.RoleBattle.NullRhi.RoleLoginFlowContract` 1/1; 0 failures and 0 warnings in separate reports under `Saved/Reports/NullRHIAfterFix` and `Saved/Reports/NullRHIRoleAfterFix`.
- `cmd /c .\RunClientNullRHI.bat -h` — PASS; help and launch output show `C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe` and role forwarding.
- Live command: `RunClientNullRHI.bat RoleBattleCivilianTest -role Aura -stress -player NullRHI_GCFix_0907 -host 127.0.0.1 -nolog`, against a fresh `WorldPersistenceId=NullRHI_GCFix_0907` server — `WorldReadiness=Ready`, client welcomed into `StartupMap`, `AutoRunMap` started, 460 `BehaviorUTest` lines recorded, client alive beyond 120 seconds after arrival, and zero fatal/assert/ensure/module/readiness-block signatures.
- `git diff --check` — PASS.

The existing `AuraCampaignMain` persistence namespace still contains invalid manifest references and intentionally fails closed; it was not deleted or overwritten. The successful live run used an isolated namespace for validation. No Blueprint assets were edited.

## Visual summary

[Open the launch-blocker diagram](2026-09-07-nullrhi-launch-blockers.svg).
