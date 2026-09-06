# Remove BungeeMan role — 2026-09-06

## Intent

Retire the BungeeMan playable role completely from the current AuraProj runtime/content contract.

## Changed behavior

- Removed BungeeMan from `RoleConfig.json`, gameplay loadouts, candidate manifest, and tutorial role lists.
- Removed all tracked BungeeMan content, `FireGun.xml`, gun projectile metadata, gun ability metadata, and the BungeeMan cook/LFS rules. An editor process currently locks ten ignored local `.uasset` remnants under `Content/BungeeMan`; they are not part of the resulting Git change and need removal after the editor releases them.
- Removed production role-specific grant/presentation assumptions and changed the Day 6 role-switch probe to use Aura/Crunch.
- Added `Scripts/test_removed_bungeeman_role.py` to assert the role, assets, and definition remain absent.
- Preserved shared firearm compatibility types so unrelated module/test compilation surfaces remain stable; no role can initialize firearm state now that the BungeeMan role is retired.

## Validation

- Direct JSON parsing passed for all edited JSON files.
- `git diff --check` passed; only Git line-ending normalization warnings were reported.
- The new regression module's registry/manifest checks pass by direct invocation; its physical-content check is blocked by the editor-locked ignored remnants. `pytest` is not installed in the local Python environment, so pytest execution was unavailable.
- Independent Google AI Studio handoff was attempted twice but the in-app browser timed out before a packet could be entered or sent; no external review result is claimed.

[View the before/after change diagram](./2026-09-06-remove-bungeeman-role.svg)
