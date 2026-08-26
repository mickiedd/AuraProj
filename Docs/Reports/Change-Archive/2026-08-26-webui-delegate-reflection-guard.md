# Web UI delegate reflection guard

## Intent

Remove the client ensure that occurred while `Pawn.OnRep_PlayerState` initialized the partial in-game Web UI HUD.

## Diagnosis and changed behavior

- The `OnRep_PlayerState` stack entry was the replication lifecycle that created the HUD; it was not the underlying fault.
- `UWebUIBridgeSubsystem::OnCommand` and `OnConnectionChanged` are dynamic multicast delegates. `AAuraHUD::HandleWebUICommand` and `HandleWebUIConnectionChanged` were not reflected `UFUNCTION`s, so `AddDynamic` could not bind them and emitted the `IsBound()` ensure.
- Both handlers are now marked `UFUNCTION()`, allowing Unreal's reflected delegate binder to resolve them safely.
- The HUD contract test now checks both handlers through `AAuraHUD::StaticClass()->FindFunctionByName`, preventing a future source-only declaration regression.

## Validation

- AuraEditor Win64 DebugGame build passed.
- `Aura.UI.WebSkillPanel.HUDContract` passed, including both reflection checks.
- `AuraWebUI` automation suite passed 4/4.
- `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding` passed.
- `Aura.Abilities` automation suite passed 9/9.
- The original client log was rechecked: it contained one ensure for each missing reflected handler at the same `OnRep_PlayerState` lifecycle point. No new ensure or binding failure appeared in the focused and broader automation logs.
- The already-running Development editor still has its pre-fix module loaded; restart it or use Live Coding (`Ctrl+Alt+F11`) before judging an existing session.

## Illustration

[Open the delegate binding diagnosis and guard flow](2026-08-26-webui-delegate-reflection-guard.svg)
