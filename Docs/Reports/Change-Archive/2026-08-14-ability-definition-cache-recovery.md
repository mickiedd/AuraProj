# Ability definition cache recovery - 2026-08-14

## Intent

Fix the network ability failure where a client predicted an XML-driven ability with a missing transient definition, ending the activation before montage/target data while the server had already consumed mana.

## Changed behavior

- `UAuraDataAbility::GetDefinition()` still prefers the replicated spec's live `SourceObject` when available.
- When the transient source is absent and the registry has no entry, the client now reloads `RoleConfig.json` through `GetRoleInfo()` and retries the replicated ability tag.
- The loaded role definitions remain rooted by the client cache, so Login-to-gameplay map travel no longer leaves the predicted graph without a `RootNode`.
- Added a regression fixture that simulates a null `SourceObject` and validates FireBolt definition recovery.

## Validation

- AuraEditor Live Coding build compiled `DataAbility.cpp`, `TestDataAbility.cpp`, `AuraAbilityInfoTests.cpp`, and generated module sources successfully.
- `python Scripts/test_firebolt_graph.py` passed.
- `git diff --check` passed.
- Full runtime automation was not replayed because an UnrealEditor process was already active; the build used the engine's Live Coding path and the focused structural test passed.

## Visual summary

[View the ability definition cache recovery diagram](./2026-08-14-ability-definition-cache-recovery.svg)
