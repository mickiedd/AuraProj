# Civilian fourth-role contract

Date: 2026-09-05

## Intent

Make the existing Civilian implementation unambiguously durable as the fourth
configured role. The repository already had the Civilian runtime slice and four
RoleConfig entries; the missing protection was an executable assertion of the
exact stable catalog and the ambient/non-playable boundary.

## Changed behavior

- Added `Aura.RoleBattle.Day5.ExplicitFourRoleCatalog` to require exactly
  `Aura`, `Crunch`, `BungeeMan`, and `Civilian`.
- The same guard verifies Civilian remains `Entity.AmbientNPC`,
  `Control.CivilianAI`, `Faction.Civilian`, `Combat.Civilian`, non-player-
  selectable, and non-attacking.
- Updated the fast Day 7-9 contract script to require the new native guard.
- Added the implementation-grade plan at
  `Docs/Plans/Civilian-Role-Implementation-Plan-2026-09-05.md`.
- No existing Crunch migration changes were reset or rewritten, and no player
  login/persistence behavior was changed.

## Validation

- `python Scripts/test_role_battle_days_7_9.py` — PASS, 26 named tests and 5 topology runners present.
- `python Scripts/test_civilian_ai.py` — PASS.
- `python Scripts/test_scifi_desert_civilian_population.py` — PASS.
- `build_test.bat` — PASS, AuraEditor Win64 Development.
- `RunRoleBattleDays789Automation.ps1 -Day 8 -Mode Dedicated` — PASS.
- `git diff --check` — PASS, with only existing LF-to-CRLF warnings.
- The direct Day 5 commandlet attempt was stopped after its quit handshake failed to terminate cleanly; the new test is compiled and statically required, but no Day 5 native result is claimed.
- The requested in-app handoff was attempted through the available Claude handoff skill. The in-app tab opened, but bounded browser-state reads timed out before a composer became available, so no private repository packet was sent and no external review result is claimed.

## Illustration

[Civilian fourth-role contract diagram](2026-09-05-civilian-role-4th-contract.svg)
