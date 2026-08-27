# Electrocute Num.3 retrigger timeout fix

## Intent

Allow the Num.3 Electrocute ability to recover when the authoritative server does not receive its montage gameplay event.

## Changed behavior

- Electrocute's `WaitForMontageEvent` node now has the same five-second safety timeout used by the other montage-driven player abilities.
- If `Event.Montage.Electrocute` is swallowed or never arrives on the server, the existing timeout path cancels and ends the ability instead of leaving its GAS spec active forever.
- The client can then receive the authoritative end and activate Num.3 again after the normal cooldown/input checks.
- Added a runtime XML parsing regression test that verifies the Num.3 assignment, event tag, and finite timeout.

## Validation

- AuraEditor Win64 Development build passed.
- `Aura.AbilityGraph.ElectrocuteRetriable` passed 1/1.
- `Aura.AbilityGraph` passed 2/2.
- `Aura.UI.WebSkillPanel` passed 2/2.
- `git diff --check` passed.

## Illustration

[Open the before-and-after Electrocute activation flow](2026-08-28-electrocute-retrigger-timeout-fix.svg)
