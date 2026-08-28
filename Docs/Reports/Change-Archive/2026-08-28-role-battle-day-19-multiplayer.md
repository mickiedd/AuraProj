# Role/Battle Day 19 - Multiplayer Hardening

Date: 2026-08-28

## Intent

Harden the authoritative multiplayer boundary across Listen and Dedicated topologies: server-owned role and ability mutations, combat and FireGun attribution, protected-civilian and friendly-fire rules, replay-safe commerce, owner-only economy state, late-join and reconnect behavior, stable population lifecycle, and bounded runtime state under network emulation.

## Changed behavior

- Ability upgrades, spell-point spending, ability activation, and equip requests now reject non-authoritative callers, invalid avatars, forged handles, unavailable points, role-owned specs, and ineligible ability states.
- The Day 19 server probe records authority, world, population, economy, FireGun, controller, privacy, replay-cache, and configured-fixture facts. The client probe exercises late join, ordered commerce, exact replay, request gaps, and stale session nonces.
- Commerce validation remains authoritative and now excludes peer player pawns from the merchant line-of-sight trace. Dev-only fixture players are positioned and movement-disabled so contention measures transaction rules rather than movement drift.
- Replay state is bounded to 256 entries, session rotation invalidates stale nonces, owner-only economy replication remains enforced, and civilian/merchant death closes interaction state without duplicate lifecycle effects.
- The Dedicated runner now fails fast when the cooked `StartupMap.umap` is missing and reads the packaged server's sandbox-resolved log path. `Config/DefaultGame.ini` explicitly cooks `/Game/Maps/StartupMap` for the WindowsServer target.
- The checked-in Day 19 vertical-slice contract covers the required security, replication, lifecycle, commerce, FireGun, emulation, and performance assertions. Mutation probes remain behind `#if !UE_BUILD_SHIPPING`.

## Validation

- `build_test.bat`: AuraEditor Win64 Development passed.
- `BuildDedicatedServer.bat`: AuraServer Win64 Development passed.
- Focused native automation: 25/25 Day 19 tests passed.
- Full `Aura` native automation: 223/223 tests passed with zero failure signals.
- All repository Python contract suites: 14/14 passed; the Day 19 suite passed 14/14 checks.
- WindowsServer cook for `/Game/Maps/StartupMap`: exit code 0, 0 cook errors (355 existing warnings).
- `RunRoleBattleDay19Multiplayer.ps1 -Mode Listen -Port 20019`: passed the Day 18 prerequisite, authority/security matrix, late join, exact-once contention, replay-gap/nonce rejection, owner privacy, crash scan, and the 30-second warmup plus 60-second sample window.
- `RunRoleBattleDay19Multiplayer.ps1 -Mode Dedicated -Port 20029`: passed the same matrix against the cooked `AuraServer.exe`.
- Both live modes used the recorded emulation profile of 100 ms lag, 20 ms jitter, and 2% packet loss. External process samples were 12 per mode; observed limits were Listen CPU p95/p99 0.45%/0.45%, memory growth 4.51 MiB, maximum outbound client bandwidth 9 KiB/s, and Dedicated 0.12%/0.12%, 5.24 MiB, and 4 KiB/s. Thresholds were 33.3%/50%, 128 MiB, and 512 KiB/s.
- `git diff --check`: passed; only the repository's existing LF-to-CRLF warnings were reported.

The live matrix intentionally uses the existing configured `StartupMap.umap`; no fake binary `Content/Maps/Tests/RoleBattleDay19.umap` was created. The configured fixture contains 37 population slots and 6 monster rows, with 3 civilians, 2 enemies, and 1 active merchant spawned in the live map. Those configured counts are recorded by the probe, while the actual actor counts remain visible in the performance samples. Production persistence and identity security still require the configured authenticated Online Subsystem; the fixture identities are explicitly non-shipping development inputs.

## Evidence

- Focused log: `Saved/Logs/Day19FocusedFinal.log`
- Full-suite log: `Saved/Logs/AuraNativeFinal.log`
- Listen report: `Saved/Reports/Day19-Listen.json`
- Dedicated report: `Saved/Reports/Day19-Dedicated.json`
- Dedicated server log: `Saved/Cooked/WindowsServer/Aura/Saved/Logs/Day19-Dedicated-Server.log`
- Vertical-slice contract: `Content/AutoTests/RoleBattleDay19VerticalSlice.xml`

## Completion gate

Day 19 multiplayer hardening is implemented and verified in both required topologies. All focused, full, static, and live tests passed before the work advanced to Day 20.

## Illustration

[Role/Battle Day 19 multiplayer hardening matrix](2026-08-28-role-battle-day-19-multiplayer.svg)
