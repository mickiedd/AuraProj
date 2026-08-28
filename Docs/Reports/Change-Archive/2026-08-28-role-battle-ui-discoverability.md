# Role/Battle UI discoverability

Date: 2026-08-28

## Intent

Review the completed Day 01–18 Role/Battle implementation and expose every player-relevant change through the existing WebUI-only gameplay HUD.

## Changed behavior

- Added a replicated battle population summary for active civilians, configured capacity, pending refills, and accepted population deaths.
- Added left-top role/life/battle/persistence presentation, including the population summary.
- Added bottom target metadata for Civilian activity, merchant availability, and combat-protected versus combat-ready state.
- Added owner-only wallet/inventory and merchant presentation wiring, including authoritative purchase result feedback.
- Added focused WebUI contract assertions for the new event and display surfaces.
- Added the day-by-day mapping and follow-up UI increment plan in [Role-Battle-UI-Incremental-Plan.md](../../Plans/Role-Battle-UI-Incremental-Plan.md).

The browser remains a presentation and command surface. Role, price, stock, inventory, range, and combat permissions continue to come from native authoritative state and validation.

## Validation

- AuraEditor Win64 Development build passed after replacing Unreal 5.5-incompatible weak-pointer boolean checks with `IsValid()`.
- `AuraWebUI.Plugin.ContentContract` passed.
- `Aura.UI.WebSkillPanel.HUDContract` passed.
- All three HUD JavaScript blocks parsed successfully with Node.js.
- `git diff --check` passed.

## Illustration

[Role/Battle UI discoverability flow](2026-08-28-role-battle-ui-discoverability.svg)
