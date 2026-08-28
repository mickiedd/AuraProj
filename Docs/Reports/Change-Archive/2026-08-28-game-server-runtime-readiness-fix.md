# Game Server Manager editor runtime and readiness fix

## Intent

Fix the current `Scifi_Desert_Level` startup failure where the Game Server Manager launched the generic Development editor for a DebugGame client, causing Unreal to report that the `Aura` game module could not be found. The first successful launch also exposed a separate readiness failure because persistence settings were read from the wrong Unreal ini bucket.

## Changed behavior

- `GameServerManager.py` now preserves the client-reported UnrealEditor executable variant, including `UnrealEditor-Win64-DebugGame.exe` and command-line variants.
- Every UnrealEditor variant receives the project file when the manager constructs the server command, and is classified as an editor launch for mode reporting.
- `AuraPersistenceSubsystem` now reads `ExpectedProviderName` and `WorldPersistenceId` from `GEngineIni`, matching the existing `Config/DefaultEngine.ini` section.
- The manager’s focused tests cover executable selection, editor launch arguments, and the matching engine-root lookup.

## Validation

- `python -m py_compile Scripts/GameServerManager.py Scripts/test_game_server_manager.py Scripts/test_editor_server_routing.py` passed.
- `python Scripts/test_game_server_manager.py` passed, including the DebugGame executable and project-file launch regression.
- `python Scripts/test_editor_server_routing.py` passed.
- `python Scripts/test_role_battle_days_18.py`, `Scripts/test_role_battle_days_19.py`, and `Scripts/test_role_battle_days_20.py` passed: 12, 14, and 14 checks respectively.
- AuraEditor Win64 Development build passed: 90/90 actions.
- Bounded live Development editor-server validation reached `IpNetDriver listening` and logged `Configured WorldPersistenceId=AuraCampaignMain` without the module/configuration rejection.
- The DebugGame DLL rebuild was attempted but could not replace the DLL while the active `UnrealEditor-Win64-DebugGame.exe` process held it open; that editor was not interrupted. Restart the active editor, then rebuild/restart the manager to consume the updated DebugGame binary.
- The installed engine distribution rejected the separate `AuraServer` target with `Server targets are not currently supported from this engine distribution`.

## Illustration

[Editor runtime and readiness flow](./2026-08-28-game-server-runtime-readiness-fix.svg)
