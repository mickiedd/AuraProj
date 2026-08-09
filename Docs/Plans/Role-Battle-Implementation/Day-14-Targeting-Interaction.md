# Day 14 - Add Targeting and Interaction

## Goal

Give players orthogonal target relationship, target kind, life state, and interaction-option data; keep LMB as attack/ability input; and add a distinct Interact input whose Server RPC lives on a player-owned component. Client highlights/prompts are previews, while the server re-resolves and validates the target and option at execution time.

## Prerequisite gate

- Day 03/12 `AuraCombatRules` returns structured relationship/permission results with server-resolved zone context.
- Day 11 combat state provides replicated Alive/Dying/Dead/Respawning and late-join reconciliation.
- Day 10 shelter markers and Day 09 Civilian work/optional merchant IDs exist.
- Day 13 removes dead civilians from active AI registries and cancels interaction eligibility.

## New code files

- `Source/Aura/Public/Combat/AuraTargetingTypes.h`
- `Source/Aura/Public/Combat/AuraTargetableInterface.h`
- `Source/Aura/Private/Combat/AuraTargetableInterface.cpp`
- `Source/Aura/Public/Interaction/AuraInteractionTypes.h`
- `Source/Aura/Public/Interaction/AuraInteractionPolicy.h`
- `Source/Aura/Private/Interaction/AuraInteractionPolicy.cpp`
- `Source/Aura/Public/Interaction/AuraInteractionComponent.h`
- `Source/Aura/Private/Interaction/AuraInteractionComponent.cpp`
- `Source/Aura/Public/UI/WidgetController/TargetInteractionWidgetController.h`
- `Source/Aura/Private/UI/WidgetController/TargetInteractionWidgetController.cpp`
- `RunRoleBattleDay14NetworkSmoke.ps1`

## New content assets

- `Content/Blueprints/Input/InputActions/IA_Interact.uasset`
- `Content/Blueprints/UI/Interaction/WBP_TargetPrompt.uasset`

## Files and assets to modify

- `Source/Aura/Public/AuraGameplayTags.h`
- `Source/Aura/Private/AuraGameplayTags.cpp`
- `Source/Aura/Public/Input/AuraInputConfig.h`
- `Source/Aura/Private/Input/AuraInputConfig.cpp`
- `Source/Aura/Public/Input/AuraInputComponent.h`
- `Source/Aura/Public/Character/AuraCharacterBase.h`
- `Source/Aura/Private/Character/AuraCharacterBase.cpp`
- `Source/Aura/Public/Character/AuraCharacter.h`
- `Source/Aura/Private/Character/AuraCharacter.cpp`
- `Source/Aura/Public/Character/AuraEnemy.h`
- `Source/Aura/Private/Character/AuraEnemy.cpp`
- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Source/Aura/Public/World/AuraCivilianShelterMarker.h`
- `Source/Aura/Private/World/AuraCivilianShelterMarker.cpp`
- `Source/Aura/Public/Player/AuraPlayerController.h`
- `Source/Aura/Private/Player/AuraPlayerController.cpp`
- `Source/Aura/Public/UI/HUD/AuraHUD.h`
- `Source/Aura/Private/UI/HUD/AuraHUD.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Blueprints/Input/IMC_AuraContext.uasset`
- `Content/Blueprints/Input/DA_AuraInputConfig.uasset`
- `Content/Blueprints/UI/Overlay/WBP_Overlay.uasset`
- `Content/Maps/RoleBattleCivilianTest.umap`

Keep `EnemyInterface` for enemy-only combat-target behavior during migration, but remove it as the PlayerController’s universal test for whether an actor is attackable or interactable.

## Orthogonal target descriptor

`FAuraTargetDescriptor` exposes separate fields; do not collapse them into one enum:

- `RelationshipTag`: one of `Target.Relationship.Hostile`, `Friendly`, `Neutral`, or `Protected`. This is computed for the observing player through `AuraCombatRules`.
- `KindTag`: `Target.Kind.Player`, `Enemy`, `Civilian`, `Shelter`, or `World`.
- `LifeTag`: `Target.Life.Alive`, `Dying`, `Dead`, or `Respawning`, mapped from `UAuraCombatStateComponent`.
- `InteractionOptions`: an array of `FAuraInteractionOption`, each with option tag, display text, enabled state, and disabled reason.
- `bLocallyAttackAllowed` and structured attack rejection reason, computed from `CanDamage(LocalPlayer, Target, PreviewContext)`.
- Read-only Health/MaxHealth and target display name where allowed.

Register native tags:

- `Target.Relationship.Hostile`
- `Target.Relationship.Friendly`
- `Target.Relationship.Neutral`
- `Target.Relationship.Protected`
- `Target.Kind.Player`
- `Target.Kind.Enemy`
- `Target.Kind.Civilian`
- `Target.Kind.Shelter`
- `Target.Kind.World`
- `Target.Life.Alive`
- `Target.Life.Dying`
- `Target.Life.Dead`
- `Target.Life.Respawning`
- `Interaction.Talk`
- `Interaction.Observe`
- `Interaction.Trade`
- `Interaction.UseShelter`
- `InputTag.Interact`

A Civilian with a merchant definition remains `Target.Kind.Civilian` and exposes a Trade option; Merchant is not a mutually exclusive target kind. A dead Enemy remains `Target.Kind.Enemy` with `Target.Life.Dead`. Do not add Downed in this slice.

## Targetable-interface contract

`IAuraTargetableInterface` exposes intrinsic target data only:

- Kind tag.
- Display name.
- Combat identity/state component access.
- Health/MaxHealth access.
- Server-recomputed interaction options for a requesting actor.
- Cosmetic highlight/unhighlight hooks or a bridge to the existing `HighlightInterface`.

Relationship and `bLocallyAttackAllowed` are not supplied by the target; the PlayerController computes them using the observer and current rules.

Implement the shared state/health access on `AAuraCharacterBase` and override kind/options in Player, Enemy, and Civilian headers. Implement the interface on `AAuraCivilianShelterMarker`. Dead/Dying actors return no enabled interactions.

Civilian options:

- Talk and Observe while Alive and allowed by its interaction profile.
- Trade appears when `MerchantDefinitionId` is nonempty, but remains disabled with `FeatureUnavailable` until Days 15–17 validate and implement commerce.

Shelter exposes UseShelter only when enabled, in range, and not capacity-blocked.

## Authoritative interaction-policy contract

`FAuraInteractionOption` is the client-safe prompt view (tag, text, enabled state, disabled reason). Separately, `FAuraResolvedInteractionPolicy` is rebuilt on the server for the requesting player and contains the option tag, internal handler ID, `MaxRangeCm`, `bRequiresLineOfSight`, allowed target kinds/life states/phases, and any zone/profile restriction. The client never sends this policy or handler ID.

For the first slice, `AuraInteractionPolicy` owns one immutable native table:

- Talk: Civilian, Alive, 250 cm, line of sight, Peace or Alert.
- Observe: Civilian, Alive, 600 cm, line of sight, any director phase.
- UseShelter: Shelter, enabled/available, 200 cm, line of sight, Alert/Conflict/Cleanup, with current capacity and zone membership rechecked.
- Trade: Civilian with a non-empty `MerchantDefinitionId`, Alive, 250 cm, line of sight, Peace or Alert; its handler remains disabled until Day 17 activates commerce.

The resolver composes that table with the requester's validated `interactionProfile`, the target's current intrinsic options/member state, and the authoritative Day 12 phase/target-zone state. Combat relationship or `CanDamage` never implicitly authorizes interaction. Safe-zone combat protection also does not grant an interaction; only the resolved interaction policy does.

## Player-owned interaction RPC

`UAuraInteractionComponent` is created as a replicated default subobject of `AAuraPlayerController`. The owning PlayerController provides the client connection, so its Server RPC is valid. Do not attach the request RPC component to a Civilian, Enemy, or shelter target.

Client entry point:

```text
RequestInteraction(TargetActor, InteractionOptionTag, ClientRequestId)
    -> ServerRequestInteraction(TargetActor, InteractionOptionTag, ClientRequestId)
    -> ClientInteractionResult(ClientRequestId, ResultCode)
```

The client sends only the replicated target actor reference, option tag, and bounded request ID. It never sends an authoritative player actor, distance, line-of-sight result, relationship, role, price, item, enabled Boolean, zone, or permission result.

On the server:

1. Derive the requesting controller/pawn from the component owner and require a valid possessed Alive player.
2. Resolve the replicated target actor reference in the server world and require `IAuraTargetableInterface`.
3. Re-read target identity, kind, life state, member state, current transform, and current intrinsic options; ignore the cached client descriptor.
4. Resolve `FAuraResolvedInteractionPolicy` from the immutable native table plus current requester profile, target, and director state. Require the option present/enabled and actor still relevant/valid.
5. Measure authoritative pawn-to-target distance against the resolved `MaxRangeCm`.
6. When the resolved policy requires it, trace authoritative line of sight on the configured visibility channel, ignoring only the requester and documented cosmetic components.
7. Enforce the resolved target-kind, life, phase, zone, capacity, requester-profile, and any explicit interaction-faction restrictions; never substitute the combat relationship result as authorization.
8. Apply per-client rate limiting and reject stale/replayed request IDs.
9. Execute Talk/Observe/UseShelter through a server handler and return a safe result code. Day 17 extends the same route for Trade.

A target that dies, is destroyed, changes zone, fills shelter capacity, or moves out of range between prompt and RPC must be rejected with no state change.

## Input and attack separation

- Add `IA_Interact` to `IMC_AuraContext` using the chosen Interact key and map it to `InputTag.Interact` in `DA_AuraInputConfig`.
- Bind Interact pressed in `AAuraPlayerController` to the focused target’s selected enabled interaction option.
- LMB remains ability/attack input and never invokes Talk, Observe, Trade, or UseShelter.
- Replace LMB’s `EnemyInterface` classification with the orthogonal descriptor/`AuraCombatRules` result while preserving existing ability activation semantics.
- Whether an attack prompt is shown is determined by `CanDamage(LocalPlayer, Target, CurrentPreviewContext)`. Never use the target’s `bCanAttack`; that flag describes whether the target can initiate attacks.
- If an LMB projectile is fired toward a protected Civilian, the Day 12 final server boundary still rejects damage even if the client preview was stale.

## Cosmetic focus, highlight, and prompt

Cursor tracing remains local and cosmetic:

1. Trace the existing cursor channel.
2. If the hit actor implements `IAuraTargetableInterface`, build a local descriptor.
3. Compute relationship and attack preview through `AuraCombatRules`.
4. Apply kind/relationship/life-specific highlight and update `WBP_TargetPrompt` through `UTargetInteractionWidgetController`.
5. Show all enabled/disabled interaction options with a safe reason and the distinct Interact key.
6. Clear focus immediately when the actor becomes invalid/non-targetable or life state leaves Alive.

The prompt must label its result as a preview. Showing a prompt or highlight never means the server accepted an interaction or attack. Server validation occurs only when the input request arrives.

## Implementation steps

1. Add the orthogonal types/tags and targetable interface.
2. Implement exact interfaces in base, Player, Enemy, Civilian, and shelter headers/sources.
3. Add the PlayerController-owned interaction component and ownership-safe RPC/result path.
4. Add the distinct Interact action/tag/config/data-asset binding.
5. Migrate cursor focus and LMB classification off universal `EnemyInterface` checks.
6. Compute attack affordance with `CanDamage(LocalPlayer, Target)`, never target `bCanAttack`.
7. Add local prompt/highlight UI and clear it on life/validity changes.
8. Add server target re-resolution and authoritative option-policy resolution for handler, range, LOS, kind, life, phase, zone, capacity, profile, rate-limit, and replay validation.
9. Add Talk, Observe, and UseShelter first-slice handlers; expose disabled Trade until commerce exists.
10. Add structured local-preview and authoritative-result logs without leaking hidden server data.
11. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day14.TargetDescriptorOrthogonality`
- `Aura.RoleBattle.Day14.MerchantRemainsCivilianKind`
- `Aura.RoleBattle.Day14.AttackAffordanceUsesCanDamage`
- `Aura.RoleBattle.Day14.DeadTargetHasNoInteraction`
- `Aura.RoleBattle.Day14.PlayerOwnedRPCComponent`
- `Aura.RoleBattle.Day14.ServerRevalidatesTarget`
- `Aura.RoleBattle.Day14.RangeAndLineOfSight`
- `Aura.RoleBattle.Day14.AuthoritativeOptionPolicy`
- `Aura.RoleBattle.Day14.CombatRelationshipDoesNotAuthorizeInteraction`
- `Aura.RoleBattle.Day14.RequestReplayAndRateLimit`
- `Aura.RoleBattle.Day14.InteractAndLMBSeparated`

## Listen-server and dedicated-server smoke

`RunRoleBattleDay14NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. Each mode uses a remote owning client and:

1. Verifies Enemy, ordinary Civilian, merchant-bound Civilian population member, shelter, and dead actor produce composable relationship/kind/life/interaction descriptors.
2. Confirms the interaction component is owned by that client’s PlayerController and its Server RPC executes exactly once.
3. Uses Interact to Talk/Observe and proves LMB is not activated.
4. Uses LMB on an allowed hostile target and proves no interaction request is emitted.
5. Verifies a protected Civilian has no attack affordance even though the player can attack generally.
6. Attempts forged/out-of-range/occluded/dead/destroyed/stale-option/replayed interactions and proves the server changes no state.
7. Moves or kills a target after the prompt but before server execution and verifies authoritative rejection.
8. Verifies disabled Trade appears on a Civilian population member with `MerchantDefinitionId` without treating it as a separate kind or changing the shared role.
9. Connects a late client after a Civilian death and verifies Dead life state, no prompt, and no interaction.

The runner enforces bounded startup, request/result, assertion, late-join, and teardown timeouts; returns nonzero on any assertion, child-process, crash, timeout, unexpected RPC execution, or missing-artifact failure; and stops only processes it created. It writes `Saved/Logs/Day14-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day14-{Listen|Dedicated}.json` with revision, commands, exit codes, request IDs/result codes, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day14; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day14Automation.log'

& '.\RunRoleBattleDay14NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay14NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

Target relationship, kind, life, and interactions are independent and composable. The client-owned PlayerController component is the only interaction RPC endpoint; the server re-resolves and validates every target/option. Interact and LMB are distinct, attack affordance uses `CanDamage(LocalPlayer, Target)`, and existing plus late clients cannot interact with invalid/dead actors or bypass range, LOS, zone, replay, or ownership checks. All build, native, listen, and dedicated gates pass.
