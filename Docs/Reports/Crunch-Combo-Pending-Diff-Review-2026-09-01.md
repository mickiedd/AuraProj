# Crunch Combo Pending Diff Review — 2026-09-01

## Purpose

Independent review of the **uncommitted working-tree changes** in `C:\Git\AuraProj`, produced by reading every pending diff against Unreal Engine 5.5 headers. This report exists so another agent can pick up the remediation without re-deriving the analysis.

The changeset builds a native four-section "Crunch" melee combo (`UAuraMeleeAttack`) and then restores Aura's LMB to the data-driven FireBolt ability. Five defects were found; none currently break a build, but two undermine the test gate and one is a latent correctness risk that will surface the moment the combo ships.

## Evidence reviewed

- Repository: `C:\Git\AuraProj`, branch `main` @ `d4f80e6` (`Gameplay expansion: Days 41–60 plans and tooling`), up to date with `origin/main`, no stashes.
- Unreal Engine **5.5** (`Aura.uproject` `EngineAssociation`), installed at `C:\Git\UE_5.5`. Every engine API claim below was verified against headers in that tree, not from memory.
- Pending surface: **17 modified files (+1,200 / −5), 24 untracked files**.
- Prior context: `Docs/Reports/Change-Archive/2026-08-31-crunch-combo-foundation.md` through `2026-09-01-aura-firebolt-lmb-regression-test.md`, and the index in `.claude/memory/visual-change-archive.md`.

### Changeset shape

| Area | Files | Character |
| --- | --- | --- |
| Combo ability | `Source/Aura/Private/AbilitySystem/Abilities/AuraMeleeAttack.cpp` (+536), `.../AuraMeleeAttack.h` (+120) | New native four-section combo, montage-driven |
| Dev network probe | `Source/Aura/Private/Player/AuraPlayerController.cpp` (+348), `.../AuraPlayerController.h` (+25) | Command-line-gated cross-process test harness |
| Gameplay tags | `AuraGameplayTags.h/.cpp`, `Config/DefaultGameplayTags.ini` | New combo tags + one duplicated cue tag |
| ASC fixes | `Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp` (+15/−2) | Prediction-key and spec-dirty correctness fixes |
| Tests | `AuraRoleBattleTests.cpp` (+80), `AuraBaselineAssetTests.cpp` (+29), new `AuraCrunchComboRuntimeTests.cpp` | Regression + contract coverage |
| Editor tooling | `AuraEditor.Build.cs` (+3), new `Source/AuraEditor/Private/Commands/*.cpp` | Montage fixture commandlets |
| Docs | 11 new `Change-Archive/*.md` + `.svg` pairs, `visual-change-archive.md` (+10) | Archive entries |

**Scope note for the next agent:** `UAuraMeleeAttack` is **not wired into any role**. It appears in no `RoleConfig.json` entry and no `Content/AbilityDefinitions/*.xml`. `Content/Config/RoleConfig.json:54` still binds Aura's LMB to `/Game/AbilityDefinitions/FireBolt.xml`. The ability is reachable only through the `-CrunchComboNetworkProbe` flag and automation tests. Confirm whether ~540 lines of shipping dead code is the intended resting state before doing more work on it.

---

## Findings

### F1 — `GameplayCue.MeleeImpact` is declared in two sources (Medium)

**Locations**

- `Config/DefaultGameplayTags.ini:30` — `+GameplayTagList=(Tag="GameplayCue.MeleeImpact",DevComment="")` (pre-existing)
- `Source/Aura/Private/AuraGameplayTags.cpp:554-557` — `AddNativeGameplayTag(FName("GameplayCue.MeleeImpact"), ...)`
- `Source/Aura/Public/AuraGameplayTags.h:177` — `FGameplayTag GameplayCue_MeleeImpact;` (pre-existing member, newly populated)

**Conflict with project convention.** Gameplay-cue tags in this project are single-source. `GameplayCue.ArcaneShards`, `GameplayCue.ShockBurst`, `GameplayCue.ShockLoop` are config-only (`DefaultGameplayTags.ini:29,31,32`); `GameplayCue.FireBlast` is native-only. This change makes `MeleeImpact` the first tag owned by both.

**Why it fails silently.** In UE 5.5, `UGameplayTagsManager::AddNativeGameplayTag` (`Engine/Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp`) does **not** validate. It calls `AddTagTableRow` and then returns `FGameplayTag(TagName)` — a bare wrapper over the `FName`. Consequences:

- No error, no warning. The tag resolves from whichever source wins registration order.
- `FGameplayTag::IsValid()` returns `true` regardless of whether the tag is registered anywhere. So the assertion added at `AuraCrunchComboRuntimeTests.cpp:31` (`TestTrue("Melee impact gameplay cue tag is registered", ...IsValid())`) is **vacuous** — it passes even if the tag is deleted from both sources.

**Fix.** Pick one source and delete the other. Recommended: **keep the config entry** (line 30) and remove the native `AddNativeGameplayTag` block at `AuraGameplayTags.cpp:554-557`, because `GameplayCue` tags are designer-facing and the ini is already the home for the other three. If you instead keep the native registration, delete ini line 30 — but note line 9 carries a redirect `GameplayCue.MeleeImpat` → `GameplayCue.MeleeImpact`, so removing the config entry also requires handling that redirect.

**Also fix the vacuous assertion.** Replace `AuraCrunchComboRuntimeTests.cpp:31` with a real registration check, e.g. compare against `UGameplayTagsManager::Get().RequestGameplayTag(TEXT("GameplayCue.MeleeImpact"), false)` and assert `IsValid()` on the *requested* tag, which does validate.

---

### F2 — Contract test greps source text instead of testing behavior (Medium)

**Location:** `Source/Aura/Private/Tests/AuraCrunchComboRuntimeTests.cpp`, `FAuraCrunchComboImpactCueContractTest`, lines 39-54.

The test loads `AuraMeleeAttack.cpp` with `FFileHelper::LoadFileToString` and asserts on substrings:

```cpp
TestTrue(TEXT("Impact cue is dispatched through the target ASC"),
    Implementation.Contains(TEXT("TargetASC->ExecuteGameplayCue(FAuraGameplayTags::Get().GameplayCue_MeleeImpact")));
TestTrue(TEXT("Impact cue supplies a target location"),
    Implementation.Contains(TEXT("CueParams.Location = Target->GetActorLocation()")));
```

**Why it is wrong.** These assertions cannot fail when the behavior breaks, and they *will* fail on any harmless reformat, rename, or refactor — including the F1 fix above, which touches `GameplayCue_MeleeImpact` resolution. A test that reads its own implementation as text provides negative value: it costs maintenance and certifies nothing.

**Fix.** Delete `FAuraCrunchComboImpactCueContractTest` (lines 23-55). The behavioral coverage already exists in `FAuraCrunchComboRuntimePlaybackTest` in the same file, which spawns a real world, activates the ability, ticks the montage, and asserts health deltas on hostile/friendly/dead/outside-radius targets. If you want an explicit cue assertion, capture the cue via a `UGameplayCueInterface` test listener or assert on the `ExecuteGameplayCue` call through the target ASC's cue history — not on file text.

Keep the `AuraBaselineAssetTests.cpp` `FAuraCrunchComboNativeContractTest` (added at lines 42-66): that one asserts real reflection state on the CDO and is legitimate.

---

### F3 — `Content/Config/RoleConfig.json` is a phantom modification (Low, but misleading)

**Symptoms.** `git status` reports ` M Content/Config/RoleConfig.json`, but `git diff` is empty and `git diff --stat` shows nothing.

**Diagnosis.** The file is byte-different but semantically identical:

| | Size | CR count | Blob hash after clean filter |
| --- | --- | --- | --- |
| `HEAD` blob | 4,952 B | 0 | `e4c917b5ca878f37e98c906e05b6849b4e5c5d44` |
| Working copy | 5,092 B | 140 | `e4c917b5ca878f37e98c906e05b6849b4e5c5d44` |

`git hash-object` on the working copy returns the **same** blob as the index, proving the only difference is line endings. Cause: `core.autocrlf=true` combined with `* text=auto` in `.gitattributes`; the file was rewritten with CRLF at 2026-09-01 22:56 (the FireBolt LMB restore). Git flags it dirty on the raw size/stat mismatch, then finds no content delta after normalization.

**Impact.** None on content. `git add` on this file is a **no-op** — it produces the same blob. But it pollutes `git status` indefinitely and will confuse the next agent into thinking an LMB config change is pending.

**Fix.** Either leave it (harmless) or clear it with:

```bash
git add Content/Config/RoleConfig.json   # normalizes and clears the stat entry
```

Do **not** "restore" it with `git checkout` — that rewrites the file with LF and the next editor save reintroduces CRLF. The durable fix is a `.gitattributes` rule pinning the file, e.g. `Content/Config/*.json text eol=lf`, if the churn becomes annoying.

---

### F4 — The CI gate exercises a code path players never hit (Medium, highest-priority design risk)

**Location:** `Source/Aura/Private/AbilitySystem/Abilities/AuraMeleeAttack.cpp:72-73`, with the guards at the top of `OnMontageEvent`, `OnInputWindowOpened`, and `OnInputWindowClosed`.

```cpp
bAuthorityFallbackTimelineActive = ActorInfo && ActorInfo->IsNetAuthority()
    && GetWorld() && GetWorld()->GetNetMode() != NM_Standalone;
```

When this is true, all three event handlers early-return on real montage notifies, and three synthetic timers drive the combo at 35% / 55% / 75% of each section's length (`ArmAuthorityFallbackForSection`).

**The problem.** `NM_Standalone` is the *only* excluded mode, so **listen servers take the fallback too**. That produces a two-path design:

| Net mode | `IsNetAuthority` | Fallback active | Timing source |
| --- | --- | --- | --- |
| Standalone / PIE | yes | **no** | Authored montage notifies |
| Remote client | no | no | Authored montage notifies (presentation only) |
| Listen server | yes | **yes** | Synthetic timers, notifies discarded |
| Dedicated server | yes | **yes** | Synthetic timers, notifies discarded |

`RunCrunchComboNetworkSmoke.ps1` launches **exactly the listen-server topology** (`Aura.uproject /Game/Maps/StartupMap?listen -server ...` plus a client). So the entire automated network proof exercises synthetic timing, while the authored notifies — the ones a real single-player or listen-server host actually receives — are only covered by the editor automation test.

The gate is further weakened by the `NearClose` scenario, which accepts either outcome:

```powershell
$ServerAccepted = Test-Pattern $ServerLog '...NEARCLOSE_ACCEPTED...'
$ServerRejected = Test-Pattern $ServerLog '...NEARCLOSE_REJECTED...'
if (-not $ServerAccepted -and -not $ServerRejected) { throw '...' }
```

**Root cause to confirm before fixing.** The fallback is well-motivated for *dedicated* servers: they do not tick skeletal meshes, so gameplay-notify-driven montages never fire. That is a real engine constraint. It is **not** obviously true for listen servers, which do render and do tick meshes. The archive entry `2026-09-01-crunch-combo-listen-runtime-gate.md` describes running "without server mesh-tick forcing" — which suggests the listen fallback may be compensating for the **nullrhi harness** rather than for engine behavior.

**Recommended fix (in order of preference).**

1. **Narrow the condition to dedicated servers only** — this preserves the real motivation and puts listen servers on the authored path real players get:

   ```cpp
   bAuthorityFallbackTimelineActive = ActorInfo && ActorInfo->IsNetAuthority()
       && GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer;
   ```

2. **If the harness cannot tick meshes, fix the harness, not the gameplay code.** The editor automation test already demonstrates the technique (`AuraCrunchComboRuntimeTests.cpp` calls `TickAnimation` + `ConditionallyDispatchQueuedAnimEvents` manually). Forcing mesh ticks in the smoke harness is preferable to shipping divergent timing.

**Expect fallout.** Changing this will change listen-server probe results; the `NearClose` scenario exists precisely because the listen path was converging unpredictably. Budget a re-baseline of the smoke assertions, and re-run all three scenarios (`Full`, `Cancel`, `NearClose`) with and without `NetworkLagMs`.

---

### F5 — `EndAbility` fabricates a Close event (Low-Medium)

**Location:** `Source/Aura/Private/AbilitySystem/Abilities/AuraMeleeAttack.cpp:136` (inside `EndAbility`).

```cpp
#if !UE_BUILD_SHIPPING
if (bComboWindowOpen)
{
    ++TestCloseEventCount;
}
#endif
```

The real close counter is incremented at line 267 (inside `OnInputWindowClosed`). This synthetic increment means the smoke script's `Close=4 Cleanup=1` assertion can pass with **three real close notifies plus one ability teardown** — the exact convergence failure the test was written to detect.

**Impact.** It is dev-only and does not affect shipped behavior, but it weakens the diagnostic it feeds. The nearby `NEARCLOSE_*` log lines and the `RunCrunchComboNetworkSmoke.ps1` `Cleanup=1` assertions all read this counter.

**Fix.** Track the synthetic count separately (e.g. `TestImplicitCloseCount`) and log both, so `Close=4` means four authored closes. If the implicit close is genuinely expected on authority teardown, keep it but make it visible in the log line rather than folded into the authored count.

---

## Verified clean

These were asserted against UE 5.5 headers at `C:\Git\UE_5.5` specifically because they looked risky. **No action needed** — recorded so the next agent does not redo the work.

| Claim | Verified at | Result |
| --- | --- | --- |
| `UGameplayAbility::MontageJumpToSection` / `MontageSetNextSectionName` exist | `Plugins/Runtime/GameplayAbilities/.../Public/Abilities/GameplayAbility.h:824, 828` | Present; both gate on `IsAnimatingAbility(this)`, which holds because the montage task's `Activate()` starts playback synchronously inside `ReadyForActivation()` |
| `HasAuthorityOrPredictionKey(ActorInfo, ActivationInfo)` signature | same header, `:255` | Matches the two-argument call in `ActivateAbility` |
| `UAbilitySystemComponent::MarkAbilitySpecDirty` is public | `.../Public/AbilitySystemComponent.h:1215` | Public; the new call in `ClearAbilitiesOfSlot` compiles and correctly marks the fast-array entry dirty |
| `StartupInputTag` is reachable from `UAuraAssetManager` | `Source/Aura/Public/AbilitySystem/Abilities/AuraGameplayAbility.h:19` | Declared `public`, so the CDO poke in `AuraAssetManager.cpp:28` is legal |
| `WaitInputPress` task leak in `OnInputPressed` | `.../Private/Abilities/Tasks/AbilityTask_WaitInputPress.cpp` (`OnPressCallback`) | **Not a bug.** The task calls `EndTask()` immediately after `OnPress.Broadcast(...)`, and the handler nulls the pointer *before* re-arming, so no double-fire or orphan task |
| `GetLivePlayersWithinRadius` / `ApplyDamageEffect` signatures | `Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h:305, 321` | Match the call sites in `ApplyComboDamage` |
| Shipping build safety of the new probe | `AuraPlayerController.h:311-317`, `AuraPlayerController.cpp:1013, 1126` | Declaration and definition both inside `#if !UE_BUILD_SHIPPING`. **Shipping compiles.** The endplay timer clear at `:1003` is deliberately unguarded, matching the existing Day17/18/19 pattern |
| New `.uasset` LFS eligibility | `.gitattributes` | `*.uasset filter=lfs diff=lfs merge=lfs -text` already covers `AM_CrunchCombo_Prototype_RuntimeV2.uasset` |
| Smoke script regex escaping | `RunCrunchComboNetworkSmoke.ps1` | All `[CrunchComboNetworkProbe]` literals in `-Pattern` arguments are backslash-escaped, so they do not degrade into character classes |
| Smoke script teardown safety | same file, `finally` block | Fixture deletion is guarded by a resolved-path prefix check under `Saved\` |

### Two minor latent issues

- `AuraMeleeAttack.cpp:87` — `ActivateAbility` arms every task inside `if (HasAuthorityOrPredictionKey(...))` with **no `K2_EndAbility()` in the else branch**. For `LocalPredicted` a client always holds a prediction key, so this should never fire, but there is no fallback if it does.
- `ActivateAbility` does not call `Super::ActivateAbility`. Harmless today (`UAuraGameplayAbility` does not override it and `UGameplayAbility::ActivateAbility` is empty), but it becomes a real bug the moment a base class gains setup logic.

---

## Remediation order

| # | Task | Files | Risk |
| --- | --- | --- | --- |
| 1 | Remove the duplicate native `GameplayCue.MeleeImpact` registration; keep the ini entry | `AuraGameplayTags.cpp:554-557` | Low |
| 2 | Replace the vacuous `IsValid()` assertion with a real tag-request check | `AuraCrunchComboRuntimeTests.cpp:31` | Low |
| 3 | Delete the source-text grep test | `AuraCrunchComboRuntimeTests.cpp:24-55` | Low — verify `Aura.Migration.CrunchCombo.RuntimePlayback` still passes |
| 4 | Split the synthetic close counter out of `TestCloseEventCount` | `AuraMeleeAttack.cpp:136`, `AuraPlayerController.cpp` log lines | Low-Med |
| 5 | Clear the phantom `RoleConfig.json` entry (`git add` is a no-op) | `Content/Config/RoleConfig.json` | None |
| 6 | **Decide** F4: narrow the fallback to `NM_DedicatedServer`, or fix mesh ticking in the smoke harness | `AuraMeleeAttack.cpp:72-73`, `RunCrunchComboNetworkSmoke.ps1` | **Med-High — expect assertions to need re-baselining** |

Task 6 requires a design decision, not just a code edit. Resolve it before committing the rest, because it determines what the other five fixes must converge to.

---

## Verification commands

Run from `C:\Git\AuraProj`. Engine root is `C:\Git\UE_5.5`; the smoke script reads `$env:UE_ENGINE_ROOT` and falls back to the `-EngineRoot` parameter.

```bash
# 1. Editor build (must be clean before anything else)
C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat AuraEditor Win64 Development C:\Git\AuraProj\Aura.uproject -waitmutex

# 2. Crunch combo automation tests (after F2/F3 edits)
C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Git\AuraProj\Aura.uproject ^
  -ExecCmds="Automation RunTests Aura.Migration.CrunchCombo; Quit" -unattended -nop4 -nullrhi

# 3. FireBolt LMB regression (guards the restore)
C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Git\AuraProj\Aura.uproject ^
  -ExecCmds="Automation RunTests Aura.Abilities.FireBolt; Quit" -unattended -nop4 -nullrhi

# 4. Full RoleBattle suite — baseline is 234/234 per the change archive
C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Git\AuraProj\Aura.uproject ^
  -ExecCmds="Automation RunTests Aura.RoleBattle; Quit" -unattended -nop4 -nullrhi

# 5. Network smoke, all three scenarios (re-run after any F4 change)
powershell -File .\RunCrunchComboNetworkSmoke.ps1 -EngineRoot C:\Git\UE_5.5 -Scenario Full
powershell -File .\RunCrunchComboNetworkSmoke.ps1 -EngineRoot C:\Git\UE_5.5 -Scenario Cancel
powershell -File .\RunCrunchComboNetworkSmoke.ps1 -EngineRoot C:\Git\UE_5.5 -Scenario NearClose
```

Reports land in `Saved\Reports\CrunchCombo-Listen.json`; logs in `Saved\Logs\CrunchCombo-Listen-{Server,Client}.log`.

### Baselines to hold

- `Aura.RoleBattle` — **234/234**
- `Aura.Migration.CrunchCombo` — **2/2** (`NativeContract`, `RuntimePlayback`; `ImpactCueContract` should be gone after F3)
- `RunCrunchComboNetworkSmoke.ps1` — all three scenarios `PASS`, zero `Fatal error:` / `Assertion failed:` signatures in either log

---

## Open questions for the owner

1. **Is the combo meant to ship?** Nothing grants `UAuraMeleeAttack`. If yes, wire it through `RoleConfig.json` / an ability definition; if no, decide whether to keep 540 lines of unreachable code.
2. **Do listen servers really need the timer fallback,** or was it added to work around the nullrhi harness not ticking skeletal meshes? See F4.
3. **Should the `NearClose` scenario accept both outcomes?** Today it cannot fail on the server-side acceptance question, which is the one thing it was written to answer.
