# Playable Candidate scope decision — 2026-08-30

The Days 21–40 candidate is frozen against `playable-candidate-v1`.

The canonical world is `/Game/Maps/StartupMap`, selected by the `RoleBattleCivilianTest` alias in `Content/Config/LevelConfig.json`. Civilian actors and markers remain runtime-created. Aura and BungeeMan are the only supported roles. The four mandatory lanes are packaged Aura/listen, BungeeMan/listen, Aura/dedicated, and BungeeMan/dedicated.

FireGun remains the XML data-ability path. It is semi-automatic, server-cadenced, BungeeMan-only, and uses owner-only magazine/reserve state. Aura reports firearm state as `NotApplicable`. The player/world persistence, reward, merchant restock, diagnostics privacy, and GSM rules are the machine-readable contract in [playable-candidate-scope.json](../Plans/Playable-Candidate-Implementation/playable-candidate-scope.json).

The production provider/account gate is explicitly `BLOCKED` because it requires external provisioning. It does not affect local/LAN validation.

Validation command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ValidatePlayableCandidateScope.ps1
```

The validator rejects unknown lanes, missing journey steps, duplicate artifacts, path traversal, unresolved decision text, and contract drift.
