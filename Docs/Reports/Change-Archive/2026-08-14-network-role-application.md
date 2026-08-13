# Apply the accepted network role to the character

## Intent

Fix the login flow so a role accepted by the dedicated server is used when the character is initialized.

## Changed behavior

- `AAuraCharacter::LoadProgress()` now reads `PendingAcceptedRoleId` for network connections.
- The accepted role is revalidated before setup; invalid state still rejects login.
- The role is copied to `AAuraPlayerState`, applied to character visuals/loadout, and used for role-specific default attributes and abilities.
- The accepted role remains on `PlayerState` so pawn replacement and respawn keep the connection-scoped identity.
- Standalone save-slot behavior remains unchanged.

## Validation

- `Aura Win64 Development` build passed.
- `AuraEditor Win64 Development` build passed.
- `Aura.RoleBattle.Day5` report: 12 succeeded, 0 failed, 0 not run.
- New `Aura.RoleBattle.Day5.NetworkCharacterConsumesAcceptedRole` test passed.
- `git diff --check` passed.

## Visual summary

[View the change diagram](./2026-08-14-network-role-application.svg)
