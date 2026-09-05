# Crunch Unity gameplay-tag helper fix

Date: 2026-09-05

## Intent

Restore `AuraEditor Win64 DebugGame` compilation after UnrealBuildTool placed the migrated Crunch ability sources in the same Unity translation unit.

## Changed behavior

Dash, GroundBlast, Tornado, and Uppercut previously declared an identically named `CrunchTag` function in separate anonymous namespaces. Those declarations are isolated in ordinary translation units, but collide when Unreal's Unity build includes the four `.cpp` files in one generated `Module.Aura.*.cpp` file.

The four abilities now call one inline helper in `AuraCrunchTagUtils.h`. The helper keeps the existing non-throwing gameplay-tag lookup behavior while providing a single definition that is valid in both Unity and non-Unity compilation modes. No gameplay tags, ability settings, or runtime behavior changed.

## Validation

- Clean `AuraEditor Win64 DebugGame` rebuild: passed, 150/150 actions, exit code 0.
- Forced Unity build with adaptive exclusions disabled: `Module.Aura.9.cpp` compiled, linked, and wrote target metadata, exit code 0.
- `git diff --check`: passed.
- Existing Unreal Engine deprecation warnings and the pre-existing Aura/AuraAbilityGraph circular-reference warning remain outside this focused fix.

## Independent review

The required in-app ChatGPT handoff was attempted, but the available ChatGPT page was signed out. No repository evidence was transmitted and no external review is claimed. The documented local fallback was used: the exact original Unity compile surface plus a clean full target rebuild both passed.

## Illustration

[Open the Unity-safe gameplay-tag lookup flow](2026-09-05-crunch-unity-tag-helper.svg)
