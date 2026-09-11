# Landmark toggle keyboard fix

Intent: diagnose why pressing `L` produced no visible response after the Landmark Guide panel mounted.

Evidence: `Saved/Logs/Aura.log` contained three `[Landmark] Native guide panel mounted` entries and normal `hud_ready` WebUI commands, but no native toggle callback or landmark toggle command. That isolates the failure to keyboard delivery before the existing `APlayerController::BindKey` path.

Changed behavior: every gameplay WebUI panel now forwards an unmodified, non-repeating `KeyL` event as `hud_landmarks_toggle`; `AAuraHUD` routes that command through the authoritative player-controller toggle. Native input remains in place as the viewport-focused path. Controller routing, HUD routing, and final widget visibility now emit `[Landmark]` diagnostics.

Validation:

- `Build.bat Aura Win64 Development C:\Git\AuraProj\Aura.uproject -WaitMutex`: PASS; UHT, changed C++ files, and `Aura.exe` link completed.
- `node --check`: PASS for `hud-bottom.html`, `hud-interaction.html`, `hud-left-top.html`, and `hud-right-top.html` script blocks.
- `Aura.Landmark.Keyboard.Contract`: compiled in the Aura target; checks all four panels for `KeyL` and `hud_landmarks_toggle`.
- `git diff --check`: PASS for all changed files.
- A standalone UnrealEditor-Cmd automation attempt exited before test discovery with “The game module Aura could not be found”; live editor automation also remains unavailable until the already-open user-owned UnrealEditor releases its loaded module. The new runtime logs will distinguish native versus WebUI delivery on the next PIE run.

Diagram: [Keyboard delivery and validation](2026-09-11-landmark-toggle-keyboard-fix.svg).
