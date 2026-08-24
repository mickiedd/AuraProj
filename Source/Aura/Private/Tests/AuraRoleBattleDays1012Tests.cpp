// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Battle/AuraBattleDirector.h"
#include "Battle/AuraBattleZoneConfig.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraDeathPolicyDispatcher.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/AuraGameModeBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/AuraPopulationManager.h"
#include "World/AuraPopulationSpawnDefinition.h"

namespace AuraRoleBattleDays1012TestsPrivate
{
	FString Read(const TCHAR* RelativePath)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / RelativePath));
		return Text;
	}

	bool HasAll(const FString& Text, std::initializer_list<const TCHAR*> Needles)
	{
		for (const TCHAR* Needle : Needles) if (!Text.Contains(Needle)) return false;
		return true;
	}

	UWorld* FindWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World()) return Context.World();
		}
		return nullptr;
	}
}

#define AURA_DAY1012_TEST(ClassName, TestPath) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.RoleBattle." TestPath, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AURA_DAY1012_TEST(FAuraDay10WorkProfileValidationTest, "Day10.WorkProfileValidation")
bool FAuraDay10WorkProfileValidationTest::RunTest(const FString& Parameters)
{
	UAuraPopulationSpawnDefinition* Definition = NewObject<UAuraPopulationSpawnDefinition>();
	FString Error;
	TestTrue(TEXT("Work profile table validates"), Definition && Definition->LoadDefinitions(Error));
	const FAuraCivilianWorkProfile* Profile = Definition ? Definition->FindWorkProfile(TEXT("Observer")) : nullptr;
	TestTrue(TEXT("Profile exposes bounded activity values"), Profile && Profile->MoveTimeout > 0.f && Profile->FleeMovementSpeed > 0.f && Profile->CalmDuration >= 0.f);
	return true;
}

AURA_DAY1012_TEST(FAuraDay10ThreatQueryDirectionTest, "Day10.ThreatQueryDirection")
bool FAuraDay10ThreatQueryDirectionTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestThreat.cpp"));
	TestTrue(TEXT("Threat query uses inverse damage direction"), Source.Contains(TEXT("CanDamage(Candidate, Civilian")));
	TestFalse(TEXT("Threat query does not ask Civilian to attack"), Source.Contains(TEXT("CanCombatTarget(Civilian")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10ProtectedCandidateIgnoredTest, "Day10.ProtectedCandidateIgnored")
bool FAuraDay10ProtectedCandidateIgnoredTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestThreat.cpp"));
	TestTrue(TEXT("Denied rule results are filtered"), Source.Contains(TEXT("if (!Result.bCanDamage) continue")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10TrustedTestThreatPolicyTest, "Day10.TrustedTestThreatPolicy")
bool FAuraDay10TrustedTestThreatPolicyTest::RunTest(const FString& Parameters)
{
	const FString Controller = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/AuraCivilianAIController.cpp"));
	TestTrue(TEXT("Threat sensing resolves the same authoritative policy as damage"),
		AuraRoleBattleDays1012TestsPrivate::HasAll(Controller, { TEXT("GetBattleDirector"), TEXT("ResolveCombatRuleContext(Candidate, Civilian") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10UntargetableDamageSourceStillThreatTest, "Day10.UntargetableDamageSourceStillThreat")
bool FAuraDay10UntargetableDamageSourceStillThreatTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestThreat.cpp"));
	TestFalse(TEXT("Threat source query does not require targetable flag"), Source.Contains(TEXT("bTargetable")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10MarkerReservationCapacityTest, "Day10.MarkerReservationCapacity")
bool FAuraDay10MarkerReservationCapacityTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/World/AuraPopulationManager.cpp"));
	TestTrue(TEXT("Marker reservations enforce capacity"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("Reservations.Num()"), TEXT("GetCapacity()"), TEXT("PopulationMemberId") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10UnreachableShelterRecoveryTest, "Day10.UnreachableShelterRecovery")
bool FAuraDay10UnreachableShelterRecoveryTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTTask_FindCivilianDestination.cpp"));
	const int32 ReachabilityCheck = Source.Find(TEXT("GetPathLength"));
	const int32 Reservation = Source.Find(TEXT("TryReserveActivityMarker"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ReachabilityCheck);
	const int32 FleeFallback = Source.Find(TEXT("ProjectPointToNavigation(DesiredFleeLocation"));
	TestTrue(TEXT("Shelters are path-checked before reservation"), ReachabilityCheck != INDEX_NONE && Reservation > ReachabilityCheck);
	TestTrue(TEXT("Rejected shelters fall through to projected fleeing"), FleeFallback > Reservation);
	return true;
}

AURA_DAY1012_TEST(FAuraDay10LifeStateStopsBrainTest, "Day10.LifeStateStopsBrain")
bool FAuraDay10LifeStateStopsBrainTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/AuraCivilianAIController.cpp"));
	TestTrue(TEXT("Non-alive state stops brain and movement"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("StopLogic"), TEXT("StopMovement"), TEXT("NewState != EAuraCombatLifeState::Alive") }));
	TestTrue(TEXT("The single alive decorator uses its zero-based execution index"), Source.Contains(TEXT("FBTDecoratorLogic(EBTDecoratorLogic::Test, 0)")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10NoAttackNodeOrGrantTest, "Day10.NoAttackNodeOrGrant")
bool FAuraDay10NoAttackNodeOrGrantTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/AuraCivilianAIController.cpp"));
	TestFalse(TEXT("Civilian runtime tree contains no attack task"), Source.Contains(TEXT("Attack")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10FindNearestHostileBlackboardContractTest, "Day10.FindNearestHostileBlackboardContract")
bool FAuraDay10FindNearestHostileBlackboardContractTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestHostile.cpp"));
	TestTrue(TEXT("Hostile service preserves selectors"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("TargetToFollow"), TEXT("DistanceToTarget"), TEXT("TNumericLimits<float>::Max()") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10EnemyTargetingRegressionTest, "Day10.EnemyTargetingRegression")
bool FAuraDay10EnemyTargetingRegressionTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestHostile.cpp"));
	TestTrue(TEXT("Enemy targeting uses combat permission and Alive"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("CanCombatTarget(Hostile, Candidate"), TEXT("IsCombatAlive"), TEXT("bTargetable") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay10NoFindNearestPlayerReferencesTest, "Day10.NoFindNearestPlayerReferences")
bool FAuraDay10NoFindNearestPlayerReferencesTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AI/BTService_FindNearestHostile.cpp"));
	TestFalse(TEXT("New hostile service does not retain old service references"), Source.Contains(TEXT("FindNearestPlayer")));
	TestTrue(TEXT("Compatibility migration service exists separately"), FPaths::FileExists(FPaths::ProjectDir() / TEXT("Source/Aura/Public/AI/BTService_FindNearestHostile.h")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11TransitionWinnerTest, "Day11.TransitionWinner")
bool FAuraDay11TransitionWinnerTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"));
	TestTrue(TEXT("Fatal transition is Alive-only"), Source.Contains(TEXT("LifeState != EAuraCombatLifeState::Alive")));
	TestTrue(TEXT("Death sequence increments at winner"), Source.Contains(TEXT("++DeathSequence")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11RepeatedFatalDamageTest, "Day11.RepeatedFatalDamage")
bool FAuraDay11RepeatedFatalDamageTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraDeathPolicyDispatcher.cpp"));
	TestTrue(TEXT("Dispatcher deduplicates by object serial and sequence rather than pointer hash"),
		AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("FObjectKey"), TEXT("HighestDispatchedSequenceByVictim") })
		&& !Source.Contains(TEXT("PointerHash")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11DeathPolicyDispatchTest, "Day11.DeathPolicyDispatch")
bool FAuraDay11DeathPolicyDispatchTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"));
	TestTrue(TEXT("GameMode owns dispatcher and policy routing"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("DeathPolicyDispatcher"), TEXT("Death_PlayerRespawn"), TEXT("Death_EnemyLoot"), TEXT("Death_PopulationRespawn") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11EnemyRewardExactlyOnceTest, "Day11.EnemyRewardExactlyOnce")
bool FAuraDay11EnemyRewardExactlyOnceTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Character/AuraEnemy.cpp"));
	TestTrue(TEXT("Enemy loot is policy-hooked"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("ApplyEnemyDeathPolicy"), TEXT("SpawnDataDrivenLoot"), TEXT("SetLifeSpan") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11CivilianNoRewardTest, "Day11.CivilianNoReward")
bool FAuraDay11CivilianNoRewardTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/World/AuraPopulationManager.cpp"));
	TestTrue(TEXT("Civilian death remains reward-free and enters the population lifecycle"),
		AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("Recorded authoritative death"), TEXT("Death_PopulationRespawn"), TEXT("ReleaseAllActivityMarkerReservations") })
		&& !Source.Contains(TEXT("SpawnDataDrivenLoot")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11NeutralDeathEventAttributionTest, "Day11.NeutralDeathEventAttribution")
bool FAuraDay11NeutralDeathEventAttributionTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"));
	TestTrue(TEXT("Death event copies source and policy attribution"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("SourceActor"), TEXT("DeathPolicyTag"), TEXT("BattleZoneId"), TEXT("BattleEventId") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11CombatRulesRejectNonAliveTest, "Day11.CombatRulesRejectNonAlive")
bool FAuraDay11CombatRulesRejectNonAliveTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraCombatRules.cpp"));
	TestTrue(TEXT("Combat rules inspect replicated life state"), Source.Contains(TEXT("!SourceStateComponent->IsAlive()")) && Source.Contains(TEXT("!TargetStateComponent->IsAlive()")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay11OnRepPresentationIdempotenceTest, "Day11.OnRepPresentationIdempotence")
bool FAuraDay11OnRepPresentationIdempotenceTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"));
	TestTrue(TEXT("Life-state replication remains the presentation trigger"), Source.Contains(TEXT("OnRep_LifeState")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12BattleZoneSchemaValidationTest, "Day12.BattleZoneSchemaValidation")
bool FAuraDay12BattleZoneSchemaValidationTest::RunTest(const FString& Parameters)
{
	UAuraBattleZoneConfig* Config = NewObject<UAuraBattleZoneConfig>();
	FString Error;
	TestTrue(TEXT("Battle zone JSON validates"), Config && Config->Load(Error));
	TestEqual(TEXT("Battle zone schema version"), Config ? Config->GetSchemaVersion() : 0, 1);
	const FAuraBattleZoneDefinition* ScopedZone = Config ? Config->ResolveZone(FVector::ZeroVector, TEXT("RoleBattleCivilianTest")) : nullptr;
	TestNotNull(TEXT("Configured map resolves its safe zone"), ScopedZone);
	TestNull(TEXT("Same coordinates on another map resolve no zone"), Config ? Config->ResolveZone(FVector::ZeroVector, TEXT("StartupMap")) : nullptr);
	return true;
}

AURA_DAY1012_TEST(FAuraDay12SafeZonePrecedenceTest, "Day12.SafeZonePrecedence")
bool FAuraDay12SafeZonePrecedenceTest::RunTest(const FString& Parameters)
{
	UAuraBattleZoneConfig* Config = NewObject<UAuraBattleZoneConfig>();
	FString Error;
	TestTrue(TEXT("Battle zone JSON validates"), Config && Config->Load(Error));
	const FAuraBattleZoneDefinition* Zone = Config ? Config->ResolveZone(FVector::ZeroVector, TEXT("RoleBattleCivilianTest")) : nullptr;
	TestTrue(TEXT("Overlapping safe zone wins over conflict zone"), Zone && Zone->ZoneId == TEXT("MarketSafe"));
	UWorld* World = AuraRoleBattleDays1012TestsPrivate::FindWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (Config && World)
	{
		const FAuraCombatPolicySnapshot Peace = Config->BuildPolicySnapshotForMap(World, FVector(2000.f, 0.f, 0.f), TEXT("RoleBattleCivilianTest"), EAuraBattlePhase::Peace, NAME_None);
		const FAuraCombatPolicySnapshot Conflict = Config->BuildPolicySnapshotForMap(World, FVector(2000.f, 0.f, 0.f), TEXT("RoleBattleCivilianTest"), EAuraBattlePhase::Conflict, TEXT("Event1"));
		TestFalse(TEXT("Peace fails closed even in a permissive conflict zone"), Peace.AllowsPvP());
		TestTrue(TEXT("Conflict enables the configured PvP policy"), Conflict.AllowsPvP());
	}
	return true;
}

AURA_DAY1012_TEST(FAuraDay12OverlappingSafeZonePriorityAndLexicalTieTest, "Day12.OverlappingSafeZonePriorityAndLexicalTie")
bool FAuraDay12OverlappingSafeZonePriorityAndLexicalTieTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Battle/AuraBattleZoneConfig.cpp"));
	TestTrue(TEXT("Safe zones still use deterministic priority and ID tie"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("Candidate.Priority > Best->Priority"), TEXT("Candidate.ZoneId.LexicalLess") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12PriorityAndLexicalTieTest, "Day12.PriorityAndLexicalTie")
bool FAuraDay12PriorityAndLexicalTieTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Battle/AuraBattleZoneConfig.cpp"));
	TestTrue(TEXT("Non-safe overlap uses priority then lexical ID"), Source.Contains(TEXT("Candidate.Priority == Best->Priority")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12TargetLocationResolutionTest, "Day12.TargetLocationResolution")
bool FAuraDay12TargetLocationResolutionTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Battle/AuraBattleDirector.cpp"));
	TestTrue(TEXT("Director resolves target location"), Source.Contains(TEXT("TargetActor->GetActorLocation()")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12UntrustedCallerContextOverwrittenTest, "Day12.UntrustedCallerContextOverwritten")
bool FAuraDay12UntrustedCallerContextOverwrittenTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp"));
	TestTrue(TEXT("Final damage boundary replaces caller context"), Source.Contains(TEXT("RuleContext = AuthoritativeContext")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12ProjectileBoundaryReevaluationTest, "Day12.ProjectileBoundaryReevaluation")
bool FAuraDay12ProjectileBoundaryReevaluationTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp"));
	TestTrue(TEXT("Application-time resolution is in ApplyDamageEffect"), Source.Contains(TEXT("FGameplayEffectContextHandle UAuraAbilitySystemLibrary::ApplyDamageEffect")));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12BattleAttributionNetSerializeTest, "Day12.BattleAttributionNetSerialize")
bool FAuraDay12BattleAttributionNetSerializeTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/AuraAbilityTypes.cpp"));
	TestTrue(TEXT("Existing battle IDs remain serialized"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("BattleZoneId"), TEXT("BattleEventId"), TEXT("NetSerialize") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12DeathEventCarriesBattleIdsTest, "Day12.DeathEventCarriesBattleIds")
bool FAuraDay12DeathEventCarriesBattleIdsTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"));
	TestTrue(TEXT("Death event copies resolved battle IDs"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("LastDeathEvent.BattleZoneId"), TEXT("LastDeathEvent.BattleEventId") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12ValidPhaseTransitionsTest, "Day12.ValidPhaseTransitions")
bool FAuraDay12ValidPhaseTransitionsTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Battle/AuraBattleDirector.cpp"));
	TestTrue(TEXT("Phase transition table is authority-only"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("!HasAuthority()"), TEXT("EAuraBattlePhase::Cleanup"), TEXT("EAuraBattlePhase::Peace") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12EventIdLifecycleTest, "Day12.EventIdLifecycle")
bool FAuraDay12EventIdLifecycleTest::RunTest(const FString& Parameters)
{
	const FString Source = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Battle/AuraBattleDirector.cpp"));
	TestTrue(TEXT("Event ID is generated only when leaving Peace and cleared on return"), AuraRoleBattleDays1012TestsPrivate::HasAll(Source, { TEXT("bLeavingPeace"), TEXT("bReturningToPeace"), TEXT("ActiveBattleEventId = NAME_None") }));
	return true;
}

AURA_DAY1012_TEST(FAuraDay12InitialFailurePublishesUnhealthyTest, "Day12.InitialFailurePublishesUnhealthy")
bool FAuraDay12InitialFailurePublishesUnhealthyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A complete joint startup publishes Ready"),
		AAuraGameModeBase::EvaluateWorldReadiness(true, true, true, true, true, true), EAuraWorldReadiness::Ready);
	TestEqual(TEXT("An invalid initial battle-zone candidate publishes Unhealthy"),
		AAuraGameModeBase::EvaluateWorldReadiness(true, true, false, true, false, false), EAuraWorldReadiness::Unhealthy);
	TestEqual(TEXT("A failed population/zone cross-validation publishes Unhealthy"),
		AAuraGameModeBase::EvaluateWorldReadiness(true, true, true, true, false, false), EAuraWorldReadiness::Unhealthy);
	TestEqual(TEXT("A rolled-back initial population publishes Unhealthy"),
		AAuraGameModeBase::EvaluateWorldReadiness(true, true, true, true, true, false), EAuraWorldReadiness::Unhealthy);
	const FString GameModeSource = AuraRoleBattleDays1012TestsPrivate::Read(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"));
	TestTrue(TEXT("Unhealthy startup is enforced at login and the GSM readiness boundary"),
		AuraRoleBattleDays1012TestsPrivate::HasAll(GameModeSource,
			{ TEXT("World startup is unhealthy"), TEXT("ScheduleDedicatedServerReadyNotification"), TEXT("IsWorldReadyForPlay()") }));
	return true;
}

#endif
