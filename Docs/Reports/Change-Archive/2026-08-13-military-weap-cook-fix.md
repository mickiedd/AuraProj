# MilitaryWeapDark server cook fix

## Intent

Fix the Loading-to-game-level rejection caused by the authoritative role service failing to load `BungeeMan`'s weapon mesh from `MilitaryWeapDark`.

## Changed behavior

- Added `/Game/MilitaryWeapDark` to `DirectoriesToAlwaysCook` in [DefaultGame.ini](../../../Config/DefaultGame.ini:123).
- Performed a full Windows Server cook. The cooked output contains `Assault_Rifle_B.uasset/.uexp`, its physics asset, and skeleton dependencies.
- Staged fresh `Aura-WindowsServer.pak`, `.ucas`, and `.utoc` containers.
- The first staging attempt exposed an incomplete `FabLauncher` descriptor in `C:\Git\UnrealEngine-5.5`; the successful packaging rerun used the complete `C:\Git\UE_5.5` engine installation without recooking.

## Validation

- Full cook reached 3,303 of 3,304 packages before staging.
- `BuildCookRun -skipcook -stage -pak -iostore -server ...` completed with `BUILD SUCCESSFUL` and exit code 0.
- Verified the cooked role asset and dependencies under `Saved/Cooked/WindowsServer/Aura/Content/MilitaryWeapDark/Weapons`.
- Verified fresh Windows Server IoStore outputs under `Saved/StagedBuilds/WindowsServer/WindowsServer/Aura/Content/Paks`.

## Visual summary

[View the change diagram](./2026-08-13-military-weap-cook-fix.svg)
