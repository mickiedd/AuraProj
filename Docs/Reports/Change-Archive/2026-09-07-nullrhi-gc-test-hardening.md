# NullRHI forced-GC regression test hardening

## Intent

Validate and remediate the remaining conditional finding in `2026-09-07-nullrhi-review-remediation-independent-validation-v2.md`. The packet is review evidence, not an instruction source; the actual change was limited to the test-validity issue it identified.

## Finding confirmed

Unreal Engine 5.5 defines `GARBAGE_COLLECTION_KEEPFLAGS` as `RF_Standalone` when running in the editor. The existing test ran in `UnrealEditor-Cmd.exe` and used that macro, so editor keep flags could theoretically preserve the loaded icon/material even if the reflected `AbilityInfoMap` reference path were removed.

The shipped icon/material pair selected by the test does not carry those keep flags, as verified by the new test assertions. The concern was therefore valid as a test-design risk, but it does not require a production change beyond the already-present `UPROPERTY(Transient)` map.

## Changed behavior

- `AuraAbilityInfoTests.cpp` now asserts that both selected shipped assets are not protected by `GARBAGE_COLLECTION_KEEPFLAGS` before collection.
- The regression test now calls `CollectGarbage(RF_NoFlags, true)`, removing editor keep flags from the collection policy.
- The existing production-loader, reflected-map, pointer-identity, and post-GC path checks remain unchanged.
- No persistence behavior, launcher behavior, Tornado runtime assertion, or unrelated dirty-worktree files were changed for this remediation.

## Validation

- `C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat Aura Win64 Development C:\Git\AuraProj\Aura.uproject -WaitMutex` — PASS, exit 0.
- `Aura.Abilities.Metadata.RuntimeAssetReferencesSurviveGC` in `UnrealEditor-Cmd.exe` — PASS, 1/1, zero warnings/errors; the no-keep-flags assertions passed and both references retained identity after full purge.
- `Aura.Abilities.Metadata.` suite — PASS, 6/6, zero failures; 2 known test warnings and 49 known asset-load warnings were reported by the suite.
- `Scripts\Test-RunClientNullRHI.ps1` — PASS, all 6 launcher cases.

## Scope and remaining limitations

The production `URuntimeAbilityInfo::AbilityInfoMap` remains `UPROPERTY(Transient)`, and the focused test now uses a collection policy that cannot mask the specific editor keep-flag failure mode. This test still validates the production loader against shipped assets rather than a synthetic negative-control UObject fixture; a separate fixture would be additional coverage, not required for this confirmed keep-flag issue.

No Blueprint asset was edited, so no Blueprint visual check was required.

## Visual summary

[Diagram](2026-09-07-nullrhi-gc-test-hardening.svg)
