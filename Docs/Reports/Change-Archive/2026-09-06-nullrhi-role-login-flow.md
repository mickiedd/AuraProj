# NullRHI role-aware Login flow

## Intent

Repair the editor option `Launch NullRHI Client (Headless)` so a headless test client can choose a current player-selectable role and carry that choice through the normal Login, Loading, and server-authoritative spawn flow.

## Root cause

The normal Login Web UI already exposed `RoleConfig.json` roles and the runtime travel code already carried `Role=<StableRoleId>`. The editor NullRHI submenu, however, only selected a level; `RunClientNullRHI.bat` emitted only `-AutoLoginLevel`; and `TryAutoLoginFromCommandLine()` replayed the connect sequence without a role argument.

## Changed behavior

- `AuraEditorModule.cpp` rebuilds a RoleConfig-backed combo list whenever the NullRHI submenu opens, filters it through `ValidatePlayerRoleSelection`, preserves the prior choice when possible (otherwise using `defaultRole`), and captures the stable role ID with each level launch action.
- `RunClientNullRHI.bat` accepts `-role <roleId>` and emits `-AutoLoginRole=<roleId>`. Omitting `-role` remains backward-compatible and uses the runtime save/default-role fallback.
- `LoginPlayerController.cpp` validates an explicit `-AutoLoginRole`, resolves the selected/default role, and forwards it through both menu-selection and connect calls. The existing `GameInstance` and Loading URL handoff then reaches server-side role revalidation unchanged.
- Added `Aura.RoleBattle.NullRhi.RoleLoginFlowContract` to guard the editor combo, batch argument, and runtime forwarding anchors.
- Added the missing `InputCore` private dependency required by the Slate combo’s keyboard handling.

## Validation

- `C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat AuraEditor Win64 Development C:\Git\AuraProj\Aura.uproject -WaitMutex` — passed; final rebuild linked `UnrealEditor-AuraEditor.dll`.
- `UnrealEditor-Cmd.exe ... -NullRHI ... Automation RunTests Aura.RoleBattle.NullRhi.RoleLoginFlowContract` — passed, 1/1.
- `cmd.exe /c "RunClientNullRHI.bat -h"` — passed; help shows `-role`, `-AutoLoginRole`, and the complete Login → Loading → battleground flow.
- Source-contract checks for the combo, validator, batch forwarding, and command-line runtime forwarding — passed.
- `git diff --check` — passed.

Live GSM/dedicated-server multi-process execution was not run because this focused change did not start or own those external processes. The required in-app AI Studio review was attempted twice, but the composer/state never became readable within the bounded browser attempts; no repository content was transmitted and no external review is claimed. Local build and automation validation were used as the fallback.

## Visual summary

[Open the flow diagram](2026-09-06-nullrhi-role-login-flow.svg).
