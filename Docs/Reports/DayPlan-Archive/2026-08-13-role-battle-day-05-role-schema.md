# Role/Battle Day 05 - Versioned Role Schema

Date: 2026-08-13  
Status: Implemented and verified.

## Plan intent

Turn `RoleConfig.json` from a permissive collection of presentation fields into a versioned, fully validated role-definition boundary. A bad document must produce one aggregate report, never a fatal getter, partial role, silent duplicate overwrite, or loss of the last known-good registry.

## Finished work

- Added authored schema version 2 and full registered identity/profile tags and flags for Aura, BungeeMan, and Civilian.
- Added deterministic omitted/explicit version-1 migration for the shipped Aura and BungeeMan compatibility map, including flat-to-nested equipment normalization and a deprecation warning.
- Added structured validation issues with severity, role ID, JSON path, and message, plus a candidate result with detected/published versions and publication eligibility.
- Replaced unsafe JSON getters with safe field readers that aggregate wrong types, missing fields, malformed arrays, duplicates, unsupported tag combinations, invalid/non-finite attributes, missing assets, incompatible skeletons, missing sockets, and ability-definition errors.
- Validated BungeeMan's FireGun contract through its offensive LMB/input tag, projectile definition, body attach socket, weapon tip socket, and real rifle asset.
- Added Civilian as ambient-only with Shaman presentation assets, no equipment, no startup offense, no LMB, no unlockables, and no player selection.
- Moved authoritative publication into `InitGame`; failed reloads retain the last good object, while first-start failure exposes an unavailable role service.
- Carried one stable role ID through loading/server travel. The server validates it in `PreLogin`, revalidates it in `InitNewPlayer`, stores it per connection on `AAuraPlayerState`, and gates spawning when missing.
- Added saved/default role validation in load/login UI without inventing a Day 05 role picker.

## Important guards

- Warnings may publish; any validation error rejects the entire candidate.
- Duplicate IDs never become last-entry-wins map writes.
- Client options may submit only a stable role ID, never tags, assets, ability lists, or progression.
- Civilian cannot be selected as a player role.
- A failed sentinel reload does not clear server or client last-known-good state.
- Existing live actors are not hot-swapped; Day 06 owns deterministic actor application.

## Validation retained by the project

- `build_test.bat`: passed.
- `Aura.RoleBattle.Day5.*`: 13/13 passed.
- Day 05 config smoke: Listen passed; Dedicated passed.
- Day 04 damage regression smoke: Listen passed; Dedicated passed.
- Smoke evidence proves invalid startup rejection, `InitGame` publication before `PreLogin`, last-good retention on bad reload, good atomic republish, invalid-role rejection, saved/default validation, and separate Aura/BungeeMan connection state.
- Both Day 05 smoke reports record `ConfigRestored=true` and `SentinelRestored=true`.

## Carryover

Day 06 consumes the accepted pending role once and applies validated identity/loadout to compatible actor shells. Day 07 retains the rendered presentation checkpoint. Day 18 remains responsible for authenticated stable profile identity; Day 05 connection roles do not claim disk-save ownership in multiplayer.

## Sources

- [Day 05 plan](../../Plans/Role-Battle-Implementation/Day-05-Role-Schema.md)
- Retained reports: `Saved/Reports/Day05-Listen.json` and `Saved/Reports/Day05-Dedicated.json`
- [Mind map](2026-08-13-role-battle-day-05-role-schema.svg)
