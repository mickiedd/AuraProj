# Scifi Desert civilian population: stale empty snapshot fix

## Intent

Restore the expected civilians in every Scifi Desert village when the world persistence snapshot contains slots that were never successfully initialized.

## Root cause

The failing server log `Saved/Logs/GameServerManager/Scifi_Desert_Level-backup-2026.08.28-15.54.08.log` restored all 32 `DesertVillageCivilians` slots as `Empty`. The population finalizer treated the presence of any saved slot as an authoritative restore, called `RestoreDormantSlot` for those Empty slots, and then reported `liveMembers=0 success=1`. The world became Ready even though no civilian had spawned.

## Changed behavior

`UAuraPopulationManager::FinalizeInitialPopulation` now treats a persisted `Empty` slot as a missing initial member and includes it in the normal initial spawn reconciliation. The manager logs the number of reconciled empty slots and fills the configured `initialCount` of 32. Deliberate lifecycle states (`Suppressed`, `Corpse`, and `RefillPending`) remain authoritative and are not respawned by this guard.

## Validation

- Scifi Desert civilian source/data regression checks passed.
- AuraEditor Win64 Development build passed, including `AuraPopulationManager.cpp`.
- Full `Aura.RoleBattle.Day` native automation passed: 213/213 successful tests, exit code 0.
- Dedicated `AuraServer` rebuild passed, and a fresh isolated server smoke test spawned slots 0–31 and reached `liveMembers=32` followed by `WorldReadiness State=Ready`.
- `git diff --check` passed.

## Illustration

[Before/after population reconciliation flow](2026-08-29-desert-civilian-empty-snapshot.svg)
