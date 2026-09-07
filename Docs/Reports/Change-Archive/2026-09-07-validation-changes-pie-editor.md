# Validation Changes: PIE editor visual check

Date: 2026-09-07

## Intent

Repeat the client-to-dedicated-server check inside Unreal Editor Play In Editor mode and capture the visible Login, Loading, failure, and successful gameplay states.

## Results

- A production-namespace PIE run showed the expected fail-closed behavior: Login -> Loading at 30% -> Login with `World startup is unhealthy: No valid manifest referenced a valid world record; refusing to publish partial world state.`
- A second PIE run used an isolated temporary `WorldPersistenceId=ValidationChangesPIE_20260907`. The manager launched the dedicated server, returned `127.0.0.1:7790` in 8.6 seconds, and the PIE client received `Welcomed by server` and loaded `StartupMap`.
- The gameplay viewport visibly showed the possessed Aura pawn named `mickie`, combat identity HUD, health/mana bars, spell bar, and gameplay controls.
- The temporary `LevelConfig.json` change was restored byte-for-byte; no production campaign save was changed.

## Evidence

- [PIE Login](../../../Saved/Reports/ValidationChanges-20260907/pie-success-login.png)
- [PIE Loading](../../../Saved/Reports/ValidationChanges-20260907/pie-success-loading.png)
- [PIE gameplay](../../../Saved/Reports/ValidationChanges-20260907/pie-success-gameplay.png)
- [PIE production failure](../../../Saved/Reports/ValidationChanges-20260907/pie-error-clean.png)
- [PIE client log](../../../Saved/Logs/Aura.log)
- [isolated manager log](../../../Saved/Reports/ValidationChanges-20260907/pie-manager-isolated.stderr.log)

[Diagram](2026-09-07-validation-changes-pie-editor.svg)
