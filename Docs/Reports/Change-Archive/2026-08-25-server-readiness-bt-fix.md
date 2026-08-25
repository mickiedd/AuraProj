# Server readiness and Behavior Tree blackboard fix

## Intent

Restore managed-server readiness and client login after the Sci-Fi Desert population addition, and eliminate the native enemy Behavior Tree service's repeated null-owner runtime ensures.

## Changed behavior

- Registered the `DesertVillages` battle zone for `Scifi_Desert_Level` with broad map bounds and a fail-closed policy that grants no PvP or Civilian-damage permission.
- Kept strict population-to-zone cross-validation, so bad references still block readiness instead of being silently ignored.
- Changed `UBTService_FindNearestHostile` to write through the `UBehaviorTreeComponent` supplied to `TickNode` instead of Blueprint-only `UBTFunctionLibrary` helpers.
- Added generic population/zone reference coverage, Sci-Fi-specific zone-policy coverage, native blackboard-access coverage, and runtime guards for unhealthy readiness and owner-component ensures.

## Validation

- Day 10-12 static contracts and Sci-Fi Desert source/data contracts passed.
- Python compilation, JSON parsing, and `git diff --check` passed.
- `AuraEditor` Win64 Development compiled successfully.
- Focused native `FindNearestHostileBlackboardContract` automation passed.
- Managed `Scifi_Desert_Level` reached Ready with 32 live civilians and received GSM acknowledgement on attempt 1; a headless client was welcomed and joined.
- Managed `RoleBattleCivilianTest` reached Ready with 3 live civilians and received GSM acknowledgement on attempt 1; a headless client was welcomed and joined.
- Repeated hostile-service runtime ticks produced zero `OwnerComp`/`BTComponent` ensures, script errors, fatal errors, assertions, or unhandled exceptions.

Illustration: [Server readiness and BT blackboard repair](2026-08-25-server-readiness-bt-fix.svg)
