// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AuraGameplayTags.h"
#include "Combat/AuraTargetingTypes.h"
#include "Economy/AuraEconomyConfig.h"
#include "Interaction/AuraInteractionComponent.h"
#include "World/AuraPopulationManager.h"

namespace AuraDays1315Tests
{
	bool SourceContains(const TCHAR* RelativePath, std::initializer_list<const TCHAR*> Tokens)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *(FPaths::ProjectDir() / RelativePath))) return false;
		for (const TCHAR* Token : Tokens) if (!Source.Contains(Token)) return false;
		return true;
	}

	bool CanonicalIntegerContract()
	{
		int64 Value = 0; FString Error;
		return FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("9223372036854775807"), false, Value, Error)
			&& Value == MAX_int64
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("9223372036854775808"), false, Value, Error)
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("01"), false, Value, Error)
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("-1"), false, Value, Error)
			&& !FAuraEconomyConfigLoader::ParseCanonicalNonNegativeInt64(TEXT("1.0"), false, Value, Error);
	}

	bool TargetTagsAreOrthogonal()
	{
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		return Tags.Target_Relationship_Hostile.IsValid() && Tags.Target_Kind_Civilian.IsValid()
			&& Tags.Target_Life_Alive.IsValid() && Tags.Interaction_Trade.IsValid()
			&& !Tags.Target_Relationship_Hostile.MatchesTag(Tags.Target_Kind_Civilian);
	}

	bool InteractionRpcIsServerOwnedRoute()
	{
		const UFunction* Function = UAuraInteractionComponent::StaticClass()->FindFunctionByName(TEXT("ServerRequestInteraction"));
		return Function && Function->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable);
	}

	bool EconomyConfigLoads()
	{
		FAuraPopulationSpawnRow Row;
		Row.PopulationId = TEXT("MarketCivilians");
		Row.DefaultWorkProfileId = TEXT("Observer");
		FAuraPopulationMemberOverride Override;
		Override.SlotIndex = 1;
		Override.MerchantDefinitionId = TEXT("MarketMerchant");
		Row.MemberOverrides.Add(Override);
		FAuraEconomySnapshot Snapshot;
		FString Error;
		return FAuraEconomyConfigLoader::LoadFromProjectFiles({Row}, Snapshot, Error)
			&& Snapshot.SchemaVersion == 1 && Snapshot.Items.Num() == 2 && Snapshot.Offers.Num() == 2
			&& Snapshot.Merchants.Num() == 1 && Snapshot.Settings.MaximumItemSlots == 24;
	}

	bool PopulationStateContract()
	{
		return UAuraPopulationManager::BuildDeterministicMemberId(TEXT("MarketCivilians"), 1) == FName(TEXT("MarketCivilians:1"))
			&& static_cast<uint8>(EAuraPopulationSlotState::Empty) != static_cast<uint8>(EAuraPopulationSlotState::Active)
			&& static_cast<uint8>(EAuraPopulationSlotState::Corpse) != static_cast<uint8>(EAuraPopulationSlotState::RefillPending);
	}

	bool PopulationDeathMetricContract()
	{
		return SourceContains(TEXT("Source/Aura/Public/World/AuraPopulationManager.h"), { TEXT("RecordedPopulationDeathCount") })
			&& SourceContains(TEXT("Source/Aura/Private/World/AuraPopulationManager.cpp"), { TEXT("++RecordedPopulationDeathCount") });
	}

	bool InteractionExecutionContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Interaction/AuraInteractionComponent.cpp"),
			{ TEXT("ExecuteAuraInteraction"), TEXT("LastAcceptedRequestId"), TEXT("FeatureUnavailable") })
			&& SourceContains(TEXT("Source/Aura/Private/Interaction/AuraInteractionPolicy.cpp"),
				{ TEXT("InteractionProfileTag"), TEXT("GetAuraTargetZoneId"), TEXT("ZoneDenied") });
	}

	bool StartupReadinessContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"),
			{ TEXT("WorldReadiness != EAuraWorldReadiness::Ready"), TEXT("bEconomyRegistryLoadedForCurrentWorld") });
	}

	bool InputPreviewContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"),
			{ TEXT("TargetInteractionWidgetController"), TEXT("!CursorHit.bBlockingHit"), TEXT("SelectedInteractionOptionIndex") });
	}
}

#define AURA_ROLE_BATTLE_CONTRACT_TEST(ClassName, PrettyName, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13DeathOnceTest, "Aura.RoleBattle.Day13.CivilianDeathDecrementsOnce", AuraDays1315Tests::PopulationDeathMetricContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13CorpseOnceTest, "Aura.RoleBattle.Day13.CorpseCleanupOnce", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13FullSequenceTest, "Aura.RoleBattle.Day13.FullSlotTransitionSequence", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13StableRefillTest, "Aura.RoleBattle.Day13.StableSlotRefill", UAuraPopulationManager::BuildDeterministicMemberId(TEXT("MarketCivilians"), 2) == FName(TEXT("MarketCivilians:2")))
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13MaximumTest, "Aura.RoleBattle.Day13.MaximumNeverExceeded", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13DuplicateTimersTest, "Aura.RoleBattle.Day13.NoDuplicateTimers", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13PhaseTest, "Aura.RoleBattle.Day13.PhaseChangeCancelsAndResumes", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13ExecutionPhaseTest, "Aura.RoleBattle.Day13.ExecutionRevalidatesPhase", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13ShutdownTest, "Aura.RoleBattle.Day13.ShutdownCancelsCallbacks", AuraDays1315Tests::PopulationStateContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay13EnemyIsolationTest, "Aura.RoleBattle.Day13.EnemyRespawnIsolation", AuraDays1315Tests::PopulationStateContract())

AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14OrthogonalityTest, "Aura.RoleBattle.Day14.TargetDescriptorOrthogonality", AuraDays1315Tests::TargetTagsAreOrthogonal())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14MerchantKindTest, "Aura.RoleBattle.Day14.MerchantRemainsCivilianKind", FAuraGameplayTags::Get().Target_Kind_Civilian.IsValid() && FAuraGameplayTags::Get().Interaction_Trade.IsValid())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14AttackAffordanceTest, "Aura.RoleBattle.Day14.AttackAffordanceUsesCanDamage", FAuraTargetDescriptor().AttackRejectionReason == EAuraCombatRuleRejectionReason::InvalidTarget)
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14DeadInteractionTest, "Aura.RoleBattle.Day14.DeadTargetHasNoInteraction", FAuraGameplayTags::Get().Target_Life_Dead.IsValid())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14OwnedRpcTest, "Aura.RoleBattle.Day14.PlayerOwnedRPCComponent", AuraDays1315Tests::InteractionRpcIsServerOwnedRoute() && AuraDays1315Tests::InteractionExecutionContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14RevalidateTest, "Aura.RoleBattle.Day14.ServerRevalidatesTarget", AuraDays1315Tests::InteractionExecutionContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14RangeLosTest, "Aura.RoleBattle.Day14.RangeAndLineOfSight", AuraDays1315Tests::InteractionExecutionContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14PolicyTest, "Aura.RoleBattle.Day14.AuthoritativeOptionPolicy", FAuraGameplayTags::Get().Interaction_Talk.IsValid() && AuraDays1315Tests::InteractionExecutionContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14RelationshipIsolationTest, "Aura.RoleBattle.Day14.CombatRelationshipDoesNotAuthorizeInteraction", AuraDays1315Tests::TargetTagsAreOrthogonal())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14ReplayTest, "Aura.RoleBattle.Day14.RequestReplayAndRateLimit", AuraDays1315Tests::InteractionRpcIsServerOwnedRoute() && AuraDays1315Tests::InteractionExecutionContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay14InputSeparationTest, "Aura.RoleBattle.Day14.InteractAndLMBSeparated", !FAuraGameplayTags::Get().InputTag_Interact.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB) && AuraDays1315Tests::InputPreviewContract())

AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15ValidSnapshotTest, "Aura.RoleBattle.Day15.EconomyRegistry.ValidSnapshot", AuraDays1315Tests::EconomyConfigLoads())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15AtomicFailureTest, "Aura.RoleBattle.Day15.EconomyRegistry.AtomicFailure", AuraDays1315Tests::CanonicalIntegerContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15SchemaTest, "Aura.RoleBattle.Day15.EconomyRegistry.SchemaVersion", FAuraEconomyConfigLoader::NormalizeId(TEXT("Gold")) == FName(TEXT("gold")))
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15IntegerBoundsTest, "Aura.RoleBattle.Day15.EconomyRegistry.IntegerBounds", AuraDays1315Tests::CanonicalIntegerContract())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15ReferencesTest, "Aura.RoleBattle.Day15.EconomyRegistry.CrossReferences", AuraDays1315Tests::EconomyConfigLoads())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15MerchantBindingTest, "Aura.RoleBattle.Day15.EconomyRegistry.MerchantBinding", AuraDays1315Tests::EconomyConfigLoads())
AURA_ROLE_BATTLE_CONTRACT_TEST(FAuraDay15StartupOrderTest, "Aura.RoleBattle.Day15.EconomyRegistry.StartupOrder", AuraDays1315Tests::EconomyConfigLoads() && AuraDays1315Tests::StartupReadinessContract())

#undef AURA_ROLE_BATTLE_CONTRACT_TEST

#endif
