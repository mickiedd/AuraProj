# Role parser build compatibility fix — 2026-09-06

## Intent

Restore the AuraEditor DebugGame build after the role-config migration changed the legacy parser signature.

## Changed behavior

- Removed the stale fourth argument from the `interactionProfile` `LegacyString` call.
- Replaced deprecated `UGameplayAbility::AbilityTags` access with `GetAssetTags()`.
- Preserved the existing version-1 Aura fallback behavior and all unrelated working-tree changes.

## Validation

- `C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat AuraEditor Win64 DebugGame -Project=C:\Git\AuraProj\Aura.uproject -WaitMutex -FromMsBuild -architecture=x64` passed with exit code 0.
- The build compiled `AuraAbilitySystemLibrary.cpp` and linked all six actions successfully.
- The original log path `C:\Git\UE\_5.5` was unavailable; validation used the installed equivalent `C:\Git\UE_5.5`.
- Independent external handoff review is pending action-time browser confirmation; no external review result is claimed yet.

[View the before/after change diagram](./2026-09-06-role-parser-build-fix.svg)
