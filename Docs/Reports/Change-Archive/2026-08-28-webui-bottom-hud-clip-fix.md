# WebUI bottom HUD clipping fix

## Intent

Fix the bottom gameplay HUD being partially cut off at the lower edge of its native WebBrowser host.

## Changed behavior

- Increased the normal bottom UMG browser band from 100 px to 116 px while preserving the 24 px bottom screen margin.
- Added a 104 px minimum height and centered flex alignment to the HTML skill strip, keeping tiles, names, and the bottom border inside the browser viewport.
- Updated static and runtime HUD contracts to protect the corrected native geometry and HTML sizing relationship.

## Validation

- Standalone `Aura Win64 Development` build passed and linked successfully; modified `AuraHUD.cpp`, `AuraAbilityInfoTests.cpp`, and `AuraWebUIAutomationTests.cpp` compiled.
- Bottom HUD content and native layout contracts passed; `git diff --check` passed.
- The editor target compiled the modified units but its final link was blocked by two running UnrealEditor processes holding RiderLink DLLs open (LNK1104).

## Illustration

[Open the bottom HUD clipping fix diagram](2026-08-28-webui-bottom-hud-clip-fix.svg)
