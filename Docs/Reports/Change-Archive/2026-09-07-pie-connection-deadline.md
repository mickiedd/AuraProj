# PIE connection deadline repair and campaign diagnosis

Date: 2026-09-07. Manager fix validated; original campaign recovery remains pending user choice.

## Intent and verified cause

Investigate the actual PIE client and dedicated server, fix connection issues, and visually verify entry into the game level in the editor viewport.

Client request at 23:16:24 timed out at 35 seconds and fell back to 192.168.1.6:7790. The server rejected PreLogin: both valid manifests reference AuraWorld_B9154B90_G179 with expected checksum EF2B3283, but the world record computes 9EB5501B. The manager never receives authenticated world readiness. Its previous 12-second launch grace plus 32-second readiness wait exceeded the client's 35-second timeout.

## Changes

- Scripts/GameServerManager.py: share a 30-second request budget between process launch and readiness; return an explicit startup/readiness error before client transport timeout. Process-alive logs and CLI help no longer describe process survival as world readiness.
- Scripts/test_gsm_request_deadline.py: four actual TCP-handler tests cover slow startup, missing readiness, shared elapsed budget, and healthy ready response.
- No C++ or Blueprint assets changed in this job. Preexisting dirty changes remain intact.

## Validation

- `python Scripts/test_gsm_request_deadline.py`: 4 passed.
- `python Scripts/test_game_server_manager.py`: passed.
- `python Scripts/test_editor_server_routing.py`: passed.
- `git diff --check`: passed.
- Live existing DebugGame editor PIE viewport: unhealthy campaign returns explicit manager error at 30.000 seconds; no fallback travel.
- Same PIE viewport, temporary WorldPersistenceId=PIEConnectionRepair_20260907: manager returns ready in 12.015 seconds; server reports `Join succeeded: mickie`; viewport shows Aura pawn, StartupMap (Client -1), HP/MP and abilities.
- Temporary LevelConfig.json override restored byte-for-byte. All 949 preexisting save hashes unchanged; full backup retained.
- Fresh runtime `Aura.Persistence.RecordAudit` on G139: valid=1, map=RoleBattleCivilianTest, checksum=5B6CFC56. DebugGame commandlet with RiderLink disabled passed. First Development commandlet was blocked by missing RiderLink RD module; not a game connection failure.

## Recovery status and evidence

Both manifests retain 43 player-profile references. A proposed replacement for inactive manifest B uses generation 187, the verified G139 world record, and preserves the latest manifest A's player-reference bytes. It is prepared outside SaveGames and has NOT been published. Applying it would roll world/NPC/merchant state back to September 6 at 23:02 local time; the user's recovery choice is pending. Original campaign recovery is not complete.

Evidence: [report directory](../../../Saved/Reports/PIEConnectionRepair-20260907/), including paired client/server logs, save audit, proposed-recovery.json, SaveGames-backup, regression results, and screenshots.

- [PIE Login](../../../Saved/Reports/PIEConnectionRepair-20260907/pie-login.png)
- [PIE Loading](../../../Saved/Reports/PIEConnectionRepair-20260907/pie-loading.png)
- [PIE error](../../../Saved/Reports/PIEConnectionRepair-20260907/pie-explicit-error.png)
- [PIE gameplay — separate campaign](../../../Saved/Reports/PIEConnectionRepair-20260907/pie-gameplay.png)

Current handoff skill produces a local review packet; it does not perform independent review. No external upload or independent reviewer was used.

[Archived diagram](2026-09-07-pie-connection-deadline.svg)
