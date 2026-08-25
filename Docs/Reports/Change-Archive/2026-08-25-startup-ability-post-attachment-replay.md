# Startup ability post-attachment replay

## Intent

Prevent startup skill icons from remaining empty until the player activates a skill.

## Changed behavior

- `AAuraHUD::InitOverlay` still builds the UMG tree before assigning the overlay controller, ensuring spell-globe input tags exist.
- The initial controller replay now runs after `AddToViewport`, because viewport attachment may invoke `PreConstruct` again and `WBP_SpellGlobe` clears its icon brush there.
- The startup sequence is now: create, build/PreConstruct, bind controller, attach to viewport, then broadcast initial values.
- The real-widget regression explicitly invokes the final spell-globe PreConstruct clear before replay, requires all four Aura startup brushes to be restored, and guards the exact HUD source ordering.

## Validation

- `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding` passed with the post-PreConstruct clear included.
- All 9 `Aura.Abilities` automation tests passed.
- AuraEditor Win64 DebugGame built successfully.
- Live Coding applied the ordering change successfully to the active Win64 Development editor.
- `git diff --check` and SVG XML validation passed.

Illustration: [Startup ability post-attachment replay](2026-08-25-startup-ability-post-attachment-replay.svg)
