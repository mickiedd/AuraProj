# Role/Battle Day 06 - Authoritative Role Application

Date: 2026-08-14  
Status: Implemented and verified.

## Plan intent

Apply one validated role during server spawn, publish its replicated runtime identity and presentation, and let the persistent PlayerState-owned ASC grant the role loadout exactly once across pawn replacement.

## Finished work

- Added a server-only `ApplyRoleAtSpawn` transaction that validates the published role, actor-shell compatibility, ASC state, identity, assets, sockets, and complete grant set before committing.
- Made `FAuraAppliedRoleState` the replicated role snapshot consumed by clients for presentation; clients cannot author role identity, grants, attributes, or combat state.
- Added persistent role-grant bookkeeping with stable provenance, idempotent same-role reapplication, and explicit legacy-save reconciliation.
- Kept equipment explicit in role data rather than inferring it from an LMB ability, preserving the future Civilian empty-loadout contract.
- Preserved the previous applied snapshot when staging or validation fails, preventing partial identity, presentation, or loadout publication.

## Important guard

Pawn replacement rebinds the persistent ASC but does not duplicate role abilities, passive activation, cooldown state, or equipment. A different live role and client mutation attempt fail closed.

## Validation retained by the project

- AuraEditor Win64 Development build passed.
- `Aura.RoleBattle.Day6`: `12/12` passed; Day 05 regression: `14/14` passed.
- Day 06 Listen and Dedicated smokes passed with two clients, two respawns per role, nine assertions per mode, and no duplicate-grant or mutation failures.
- Day 04–05 smoke regressions passed and `git diff --check` passed.

## Carryover

Day 07 owns the final Aura/BungeeMan role, rendered presentation, and packaged regression gate. Civilian actor and population behavior remain Day 08–09 work.

## Sources

- [Day 06 plan](../../Plans/Role-Battle-Implementation/Day-06-Role-Application.md)
- [Day 06 implementation record](../Change-Archive/2026-08-14-day-06-authoritative-role-application.md)
- [Mind map](2026-08-14-role-battle-day-06-role-application.svg)
