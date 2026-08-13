# Login role selection

## Intent

Add an explicit Role Selection dropdown to the Login Web UI so players choose which configured player role they use before entering a game level.

## Changed behavior

- Login `login_state` now publishes configured player-selectable roles and `selectedRoleId`.
- `login.html` renders the Role selection dropdown before the server-level dropdown.
- The page sends `login_select_role`, and includes `roleId` in level-selection and connect commands.
- `ALoginPlayerController` validates role IDs against `RoleConfig.json` before accepting them.
- The selected role continues through the existing GameInstance → Loading → `?Role=` travel path; dedicated-server validation remains authoritative.
- The existing automatic login path remains compatible and can still use `-AutoLoginRole=`.

## Validation

- Role configuration contract found the selectable roles `Aura` and `BungeeMan`.
- `Aura Win64 Development` build passed.
- `AuraEditor Win64 Development` build passed.
- Focused `AuraWebUI` automation passed:
  - `AuraWebUI.Plugin.WebSocketLoopback`
  - `AuraWebUI.Plugin.BridgeProtocol`
  - `AuraWebUI.Plugin.ContentContract`
- `git diff --check` passed.

## Visual summary

[View the change diagram](./2026-08-14-login-role-selection.svg)
