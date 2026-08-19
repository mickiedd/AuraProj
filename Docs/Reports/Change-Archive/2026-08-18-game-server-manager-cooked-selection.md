# Game Server Manager cooked-server selection

## Intent

Fix the Game Server Manager failure where a Development `Binaries/Win64/AuraServer.exe` was launched against `/Game/Maps/StartupMap` without cooked content. Unreal correctly exited with code 0 after reporting that the map could not be found.

## Changed behavior

- `GameServerManager.py` now prefers the current staged Windows Server launcher and then the newest archived Windows Server package.
- Candidates are accepted only when a cooked map or Pak/IoStore payload is present beside the launcher.
- An uncooked `Binaries/AuraServer` is no longer selected automatically. When no cooked server exists, the manager uses the discovered `UnrealEditor.exe -server` fallback; otherwise it emits an actionable staging/configuration error.
- Explicit `--server-exe` and `AURA_SERVER_EXE` overrides remain supported.

## Validation

- `Scripts/test_game_server_manager.py` passed.
- `python -m py_compile Scripts/GameServerManager.py Scripts/test_game_server_manager.py` passed.
- Live GSM request passed after restart: staged server loaded `StartupMap`, finalized three `MarketCivilians`, and returned `host=192.168.1.6`, `port=7790`.
- The original failure was reproduced and confirmed as `Failed to load package '/Game/Maps/StartupMap'` followed by `RequestExit(0)` from the uncooked server binary.

## Illustration

[Cooked-server selection flow](./2026-08-18-game-server-manager-cooked-selection.svg)
