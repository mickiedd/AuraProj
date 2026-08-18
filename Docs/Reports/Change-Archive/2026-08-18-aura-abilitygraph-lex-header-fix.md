# AuraAbilityGraph UE 5.5 LexFromString header fix

## Intent

Restore the AuraAbilityGraph module build after Unreal Engine 5.5 stopped resolving `Misc/LexFromString.h`.

## Changed behavior

- Updated `AbilityDefinition.cpp` to include the UE 5.5 Core header at `String/LexFromString.h`.
- Kept the XML numeric parsing logic and `LexTryParseString` validation unchanged.
- The previous missing-header C1083 failure is removed; AuraEditor can compile and link the module again.

## Validation

Ran:

```text
C:\Git\UnrealEngine-5.5\Engine\Build\BatchFiles\Build.bat AuraEditor Win64 Development -Project=C:\Git\AuraProj\Aura.uproject -WaitMutex -FromMsBuild -architecture=x64
```

Result: exit code 0, 11 build actions completed. The build still emits existing non-blocking UE 5.5 deprecation, plugin dependency, and circular dependency warnings.

## Illustration

[View the before/after build flow](2026-08-18-aura-abilitygraph-lex-header-fix.svg)
