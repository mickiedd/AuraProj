# Crunch HUD replay and packaged Loading-map fix

Date: 2026-09-04

## Intent

Close two observed migration blockers: the packaged Login-to-Loading travel failure and the Web UI HUD remaining on its startup placeholders when the browser handshake completed after role/pawn replication.

## Changed behavior

- The packaged Game archive is built with `Login`, `Loading`, and `StartupMap` as explicit cook inputs, so the staged UFS manifest contains both `Aura/Content/Maps/Loading.umap` and `Aura/Content/Maps/Loading.uexp`.
- `AAuraHUD::HandleWebUIConnectionChanged` now replays the complete authoritative Web UI snapshot on the first browser connection and clears stale role/battle payload caches.
- `AAuraHUD::SendRoleStateToWebUI` now uses the applied pawn role when valid and falls back to the replicated `AAuraPlayerState` role during the short replication window.
- `FAuraWebSkillPanelHUDContractTest` asserts the late-connection replay and PlayerState fallback source contract.

## Validation

- `Build.bat Aura Win64 Development ... -NoHotReloadFromIDE` passed with exit code 0.
- Fresh `RunUAT BuildCookRun` for `CrunchMigration-20260904-game` completed with `BUILD SUCCESSFUL` and exit code 0.
- `Saved/StagedBuilds/CrunchMigration-20260904-game/Windows/Manifest_UFSFiles_Win64.txt` lists `Loading.umap` and `Loading.uexp` (lines 1676–1677) alongside Login and StartupMap.
- `Aura.UI.WebSkillPanel.HUDContract` completed with `Result={Success}` and exit code 0.
- `Aura.Migration.Crunch` discovered seven tests; all seven completed with `Result={Success}` and exit code 0.
- `Scripts/Tests/test_crunch_migration_runner.py` passed; `validate_crunch_network_timeline.py` passed for the latest visible/offscreen reports.
- Source client visual startup had already reached the Role Battle HUD with Crunch and five skill slots in the preceding GSM run. A fresh rebuilt executable reached Login and rendered, but Windows Firewall presented a permission prompt; no Windows security setting was changed.

## Archived illustration

[Open the change-flow illustration](2026-09-04-crunch-hud-replay-and-loading-map.svg)
