# Player-name text display fix

Intent: diagnose the three replacement glyphs visible above the player in the supplied gameplay capture and preserve the resolved player name through the Login -> Loading -> dedicated-server travel path.

Evidence: `Saved/Logs/Aura.log` records `player='mickie'` at `RequestLoginMenuConnect`, then records `player='?䟵ਫ'` at the GSM success callback, `OnCrossServerTravelReady`, and the final `?PlayerName=` travel URL. The callback was clearing its self-referential `TFunction` before using its captured player-name and fallback strings. Clearing the function destroyed those captures while the callback was still executing, producing use-after-destruction text that rendered as replacement glyphs.

Changed behavior: `LoginPlayerController.cpp` now defers handler release with a game-thread task until the current callback invocation has returned. Terminal success, fallback, failure, GameInstance teardown, and cancelled retry paths all release through the deferred helper; retry behavior and the server-authoritative travel contract are otherwise unchanged. A focused contract assertion prevents the handler from being cleared in place again.

Validation:

- `python Scripts/test_game_server_manager.py`: PASS; includes the player-name callback lifetime contract.
- `python -m py_compile Scripts/test_game_server_manager.py`: PASS.
- `Build.bat AuraEditor Win64 Development C:\Git\AuraProj\Aura.uproject -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoLink`: PASS; UHT and `LoginPlayerController.cpp` compilation completed. Final DLL linking was not attempted in this validation pass because the open editor owns `UnrealEditor-Aura.dll`.
- `git diff --check`: PASS for the focused changes.
- The supplied screenshot and pre-fix log are evidence of the symptom and cause, not implementation instructions. A post-rebuild gameplay capture is still required to visually confirm the corrected name in the running editor.

Diagram: [Player-name lifetime and display path](2026-09-11-text-display-player-name-fix.svg).
