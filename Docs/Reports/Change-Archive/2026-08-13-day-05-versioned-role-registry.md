# Day 05 versioned role registry

## Intent

Make role data a safe server prerequisite: version and validate the complete Aura/BungeeMan/Civilian catalog, retain the last known-good registry on bad reload, and bind each login to one server-validated stable role ID.

## Changed behavior

- Replaced permissive flat role JSON with an authored version-2 schema containing full identity/profile tags, explicit flags, nested equipment, and separate spawn/unlockable grants.
- Added deterministic version-1 migration and structured aggregate issues instead of fatal JSON getters, skipped fragments, or silent duplicate overwrites.
- Added asset, skeleton, socket, ability, input, category, offense, projectile, tag-combination, Civilian, and default-role validation.
- Added a distinct ambient Civilian definition using the project's Shaman mesh and compatible Anim Blueprint, with no offensive loadout or player selection.
- Moved server publication to `InitGame`; a failed reload retains the exact last-good registry, and invalid first startup leaves the service unavailable.
- Added `Role=<StableRoleId>` across loading/server travel. `PreLogin` validates it, `InitNewPlayer` stores it per connection, and spawn/restart requires that accepted state.
- Added saved/default-role validation to the existing load/login UI without adding a role picker.
- Updated the editor reload action to use the same aggregate runtime validator.

## Important guards

The candidate is all-or-nothing. A client cannot submit role assets, tags, ability lists, or progression; Civilian and unknown IDs cannot log in as players; a sentinel failure cannot erase good state; and Day 05 does not hot-swap actors or claim persistent multiplayer identity.

## Validation

- `build_test.bat` passed.
- All 13 named `Aura.RoleBattle.Day5.*` automation tests passed; log: `Saved/Logs/Day5Automation.log`.
- `RunRoleBattleDay5ConfigSmoke.ps1` passed in Listen and Dedicated modes. Reports prove invalid startup rejection, startup-before-login ordering, bad-reload retention, good republish, invalid-role rejection, two distinct connection roles, saved/default validation, config restoration, and sentinel restoration.
- Required Day 04 damage regression smoke passed in Listen and Dedicated modes.
- SVG XML parsing, archive links, JSON/report checks, PowerShell syntax, and `git diff --check` were validated at completion.

## Illustration

[View the Day 05 before/after flow](2026-08-13-day-05-versioned-role-registry.svg).
