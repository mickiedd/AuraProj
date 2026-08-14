# Ability definition cache recovery regression coverage - 2026-08-14

## Intent

Ensure the cache-recovery fix protects the actual activation boundary seen in the log, not only a direct definition lookup.

## Changed behavior

- The test fixture now creates a real transient `UWorld` and a GAS owner so component and ability-task setup follows engine rules.
- It simulates the replicated client shape: the dynamic FireBolt tag is present while `FGameplayAbilitySpec::SourceObject` is absent.
- After clearing process-lifetime caches, the test verifies the XML definition and graph root are rebuilt, calls `ActivateAbility`, and requires the graph to remain running with its target-data wait task alive.
- Test activation bookkeeping and teardown now model an active GAS spec, so cleanup validates the pending task is released without a false inactive-ability warning.

## Validation

- AuraEditor Development full link passed.
- `Aura.Abilities.DefinitionCache.RecoveryAfterTravel` passed with exit code 0.
- `Aura.Abilities.Metadata` passed all 5 tests with exit code 0.
- `python Scripts/test_firebolt_graph.py` passed.
- `git diff --check` passed.

## Visual summary

[View the definition cache recovery regression diagram](./2026-08-14-ability-definition-cache-regression-test.svg)
