# Startup ability PreConstruct ordering

## Intent

Fix the remaining empty startup skill globes after controller replay was added, by covering and correcting the real UMG widget lifecycle rather than only the controller delegate boundary.

## Changed behavior

- `AAuraHUD::InitOverlay` now calls `TakeWidget()` before assigning the overlay controller and broadcasting initial values.
- This runs `WBP_HealthManaSpells` PreConstruct first, establishing `InputTag.LMB`, `InputTag.1`, `InputTag.2`, and `InputTag.3` before ability payloads reach the spell globes.
- The earlier post-binding ability replay remains in place, covering abilities that replicated before HUD creation.
- The regression test now instantiates the real `WBP_Overlay` and `BP_OverlayWidgetController`, verifies all eight spell-globe listeners, checks the four configured input tags, and compares the rendered icon brush for every Aura startup ability.

## Validation

- The expanded real-widget regression failed before the HUD lifecycle fix because active spell-globe input tags were unset and slots 1-3 did not render.
- `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding` passed after the fix with four broadcasts, nine delegate listeners including the test receiver, correct tags, and matching rendered icon resources.
- All 9 `Aura.Abilities` automation tests passed.
- AuraEditor Win64 DebugGame built successfully.
- Live Coding applied the HUD fix successfully to the active Win64 Development editor.
- The active PIE controller executed `ClientRefreshAbilityUI`; its log recorded four overlay broadcasts and confirmed the call succeeded.
- A subsequent PIE map load exercised the patched initialization path and again received and published all four startup abilities.
- `git diff --check` and SVG XML validation passed.

Illustration: [Startup ability PreConstruct ordering](2026-08-25-startup-ability-preconstruct-order.svg)
