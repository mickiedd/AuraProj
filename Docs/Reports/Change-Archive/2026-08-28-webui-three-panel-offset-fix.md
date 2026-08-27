# WebUI three-panel HUD offset fix

## Intent

Restore the right-top and bottom gameplay WebUI panels after the three-panel split exposed invalid UMG viewport dimensions.

## Changed behavior

- The right-top browser keeps its right margin through a positive fixed width (`366`) because its horizontal anchors are not stretched.
- The bottom browser uses positive fixed height (`100`) and a positive right margin (`24`) so Slate receives a drawable bottom band.
- The expanded bottom interaction layout now also supplies a positive height (`396`).
- The HUD contract checks the corrected offsets and rejects non-positive runtime dimensions.

## Validation

- AuraEditor Win64 Development build passed.
- `Aura.UI.WebSkillPanel.HUDContract` and `RuntimeMountAndFallback` passed 2/2.
- `AuraWebUI` plugin tests passed 4/4.
- `git diff --check` passed.

## Illustration

[Open the invalid-to-visible viewport offset diagram](2026-08-28-webui-three-panel-offset-fix.svg)
