# Remote Python diagnostics and Game Server Manager startup fix - 2026-08-15

## Intent

Investigate the login/runtime report from the logs and validate the desert showcase scene through Unreal Editor Remote Python.

## Evidence and changed behavior

- The first login request waited for a request-time UBT build even though `Binaries/Win64/AuraServer.exe` already existed. The client timed out after 35.002 seconds and fell back to `192.168.1.6:7784`.
- `GameServerManager.py` now re-resolves the packaged executable when a manager process started before the binary was created, and only builds when no executable can be found.
- The UE5.5 remote level scripts now use `LevelEditorSubsystem.load_level` instead of the removed `EditorLevelLibrary.load_editor_level` binding.
- The remote spawn probe confirmed the reported BungeeMan area is valid: landscape Z is around 100, the PlayerStart is at Z=200, the body has `WeaponHandSocket` on `hand_l`, and the rifle exposes `Muzzle`.

## Validation

- Python AST parsing passed for all changed scripts.
- Focused async GSM handler test passed: an existing packaged `AuraServer.exe` is reused and the build hook is not called.
- `python Scripts/test_firebolt_graph.py` passed.
- `git diff --check` passed.
- Live `python Scripts/remote_run.py Scripts/analyze_showcase_level.py` passed against the active editor: 1,699 actors, 9 landscapes, and 36 PlayerStarts were reported.
- The Game Server Manager was restarted with the patched source. A fresh request returned in about 12 seconds, and UDP port 7784 was owned by exactly one active server process.

## Visual summary

[View the remote Python and GSM startup diagram](./2026-08-15-remote-python-gsm-startup.svg)
