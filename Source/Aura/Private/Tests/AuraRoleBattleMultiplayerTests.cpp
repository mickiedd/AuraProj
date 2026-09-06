// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Combat/AuraCombatRules.h"
#include "Game/AuraGameModeBase.h"
#include "Interaction/AuraInteractionComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AuraRoleBattleNetworkProbe.h"

namespace AuraRoleBattleMultiplayerTests
{
	bool SourceContains(const TCHAR* RelativePath, std::initializer_list<const TCHAR*> Tokens)
	{
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *(FPaths::ProjectDir() / RelativePath))) return false;
		for (const TCHAR* Token : Tokens) if (!Source.Contains(Token)) return false;
		return true;
	}

	bool FileContains(const TCHAR* RelativePath, const TCHAR* Token)
	{
		FString Source;
		return FFileHelper::LoadFileToString(Source, *(FPaths::ProjectDir() / RelativePath)) && Source.Contains(Token);
	}

	bool AuthorityMutationContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp"), {
			TEXT("ServerUpgradeAttribute_Implementation"), TEXT("IsOwnerActorAuthoritative"),
			TEXT("Attributes_Primary_Strength"), TEXT("GetAttributePoints"),
			TEXT("ServerSpendSpellPoint_Implementation"), TEXT("GetSpellPoints"),
			TEXT("ServerRequestActivateAbility_Implementation"), TEXT("FindAbilitySpecFromHandle"),
			TEXT("ServerEquipAbility_Implementation")});
	}

	bool DamageProducerContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Combat/AuraCombatRules.cpp"), {
			TEXT("CanDamage"), TEXT("CanReceiveDamage"), TEXT("TrustedWorldContext")})
			&& SourceContains(TEXT("Source/Aura/Private/Actor/AuraProjectile.cpp"), { TEXT("HasAuthority()"), TEXT("ApplyDamageEffect") });
	}

	bool RetiredRoleAndFirearmBoundaryContract()
	{
		return !FileContains(TEXT("Content/Config/RoleConfig.json"), TEXT("BungeeMan"))
			&& !FileContains(TEXT("Content/Config/RoleConfig.json"), TEXT("FireGun.xml"))
			&& !FileContains(TEXT("Content/Config/ProjectileDefinitions.json"), TEXT("fireGunBullet"))
			&& SourceContains(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp"), { TEXT("bApplicable"), TEXT("NotApplicable") });
	}

	bool CommerceSecurityContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), {
			TEXT("PlayerController->HasAuthority"), TEXT("SessionNonce"), TEXT("RequestGap"),
			TEXT("ReplayCache"), TEXT("while (Session.ReplayOrder.Num() > 256)"),
			TEXT("Offer.RequiredRoleId"), TEXT("LineOfSightClear"), TEXT("CapturePersistenceState")});
	}

	bool PrivacyAndLifecycleContract()
	{
		return SourceContains(TEXT("Source/Aura/Private/Player/AuraPlayerState.cpp"), {
			TEXT("COND_OwnerOnly"), TEXT("ApplyPersistentProfile")})
			&& SourceContains(TEXT("Source/Aura/Private/World/AuraPopulationManager.cpp"), {
				TEXT("PopulationMemberId"), TEXT("RestoreDormantSlot"), TEXT("bPersistenceSnapshotConfigured")})
			&& SourceContains(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), { TEXT("TryEnterDying"), TEXT("HasAuthority()") });
	}

	bool TopologyContract()
	{
		return FileContains(TEXT("Source/AuraServer.Target.cs"), TEXT("TargetType.Server"))
			&& SourceContains(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), {
				TEXT("RoleBattleDay19NetworkProbe"), TEXT("RunRoleBattleDay19NetworkProbe")});
	}

	bool DevelopmentProbeContract()
	{
		return SourceContains(TEXT("Source/Aura/Public/Tests/AuraRoleBattleNetworkProbe.h"), {
			TEXT("ReplayCacheLimit = 256"), TEXT("HasRequiredFixture")})
			&& SourceContains(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), {
				TEXT("#if !UE_BUILD_SHIPPING"), TEXT("NetworkEmulation=recorded")});
	}
}

#define AURA_DAY19_TEST(ClassName, PrettyName, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_DAY19_TEST(FAuraDay19ServerAuthorityTest, "Aura.RoleBattle.Day19.Security.ServerAuthorityBoundary", AuraRoleBattleMultiplayerTests::AuthorityMutationContract())
AURA_DAY19_TEST(FAuraDay19RoleSelectionServerOwnedTest, "Aura.RoleBattle.Day19.Security.RoleSelectionServerOwned", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), { TEXT("Only the server may apply a role at spawn"), TEXT("HasAuthority") }))
AURA_DAY19_TEST(FAuraDay19AbilityActivationTest, "Aura.RoleBattle.Day19.Security.AbilityActivationRequiresEquippedSlottedSpec", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp"), { TEXT("FindAbilitySpecFromHandle"), TEXT("Abilities_Status_Equipped"), TEXT("AbilityHasAnySlot") }))
AURA_DAY19_TEST(FAuraDay19DamageRulesTest, "Aura.RoleBattle.Day19.Security.DamageRulesRemainAuthoritative", AuraRoleBattleMultiplayerTests::DamageProducerContract())
AURA_DAY19_TEST(FAuraDay19RetiredRoleBoundaryTest, "Aura.RoleBattle.Day19.Cleanup.RetiredRoleAndFirearmBoundary", AuraRoleBattleMultiplayerTests::RetiredRoleAndFirearmBoundaryContract())
AURA_DAY19_TEST(FAuraDay19FriendlyFireTest, "Aura.RoleBattle.Day19.Security.FriendlyFireDenied", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Combat/AuraCombatRules.cpp"), { TEXT("Friendly"), TEXT("bAllowFriendlyFire") }))
AURA_DAY19_TEST(FAuraDay19ProtectedCivilianTest, "Aura.RoleBattle.Day19.Security.ProtectedCivilianDenied", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Combat/AuraCombatRules.cpp"), { TEXT("bTargetProtected"), TEXT("Civilian") }))
AURA_DAY19_TEST(FAuraDay19CommerceAuthorityTest, "Aura.RoleBattle.Day19.Security.CommerceAuthority", AuraRoleBattleMultiplayerTests::CommerceSecurityContract())
AURA_DAY19_TEST(FAuraDay19ForgedPayloadTest, "Aura.RoleBattle.Day19.Security.ForgedCommercePayloadRejected", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), { TEXT("OfferId"), TEXT("Offer.BuyPrice"), TEXT("Offer.ItemId"), TEXT("RuntimeOffer") }))
AURA_DAY19_TEST(FAuraDay19ReplayBoundedTest, "Aura.RoleBattle.Day19.Security.ReplayCacheBounded", FAuraRoleBattleNetworkProbe::ReplayCacheLimit == 256 && AuraRoleBattleMultiplayerTests::CommerceSecurityContract())
AURA_DAY19_TEST(FAuraDay19OwnerPrivacyTest, "Aura.RoleBattle.Day19.Replication.OwnerOnlyEconomy", AuraRoleBattleMultiplayerTests::PrivacyAndLifecycleContract())
AURA_DAY19_TEST(FAuraDay19LateJoinTest, "Aura.RoleBattle.Day19.Replication.LateJoinUsesReplicatedState", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), { TEXT("OnRep_AppliedRoleState"), TEXT("OnRep") }) && AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Economy/AuraMerchantComponent.cpp"), { TEXT("OnRep_Presentation"), TEXT("Replicated") }))
AURA_DAY19_TEST(FAuraDay19ReconnectNonceTest, "Aura.RoleBattle.Day19.Lifecycle.ReconnectRotatesSessionNonce", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), { TEXT("RotateSession"), TEXT("ReplayCache.Reset"), TEXT("Nonce = FGuid::NewGuid") }))
AURA_DAY19_TEST(FAuraDay19ExactlyOnceDeathTest, "Aura.RoleBattle.Day19.Lifecycle.ExactlyOnceDeath", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"), { TEXT("TryEnterDying"), TEXT("DeathSequence"), TEXT("TryEnterDead") }))
AURA_DAY19_TEST(FAuraDay19PopulationStableIdTest, "Aura.RoleBattle.Day19.Lifecycle.PopulationStableIds", AuraRoleBattleMultiplayerTests::PrivacyAndLifecycleContract())
AURA_DAY19_TEST(FAuraDay19MerchantClosureTest, "Aura.RoleBattle.Day19.Lifecycle.MerchantDeathClosure", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), { TEXT("HandleMerchantLifeStateChanged"), TEXT("SetAuthorityUnavailable") }))
AURA_DAY19_TEST(FAuraDay19InteractionOwnershipTest, "Aura.RoleBattle.Day19.Security.PlayerOwnedInteractionRoute", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Interaction/AuraInteractionComponent.cpp"), { TEXT("Cast<APlayerController>(GetOwner())"), TEXT("ServerRequestInteraction") }))
AURA_DAY19_TEST(FAuraDay19PersistenceIsolationTest, "Aura.RoleBattle.Day19.Persistence.WorldAndProfileIsolation", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp"), { TEXT("WorldPersistenceId"), TEXT("IdentityKey"), TEXT("BuildSlotName") }))
AURA_DAY19_TEST(FAuraDay19NetworkTopologyTest, "Aura.RoleBattle.Day19.Network.ListenAndDedicatedRequired", AuraRoleBattleMultiplayerTests::TopologyContract())
AURA_DAY19_TEST(FAuraDay19NetworkProbeTest, "Aura.RoleBattle.Day19.Network.DevelopmentProbeOnly", AuraRoleBattleMultiplayerTests::DevelopmentProbeContract())
AURA_DAY19_TEST(FAuraDay19NetworkEmulationTest, "Aura.RoleBattle.Day19.Network.EmulationProfileRecorded", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), { TEXT("NetworkEmulation=recorded"), TEXT("RoleBattleDay19PerformanceProbe") }))
AURA_DAY19_TEST(FAuraDay19PerformanceBoundedTest, "Aura.RoleBattle.Day19.Performance.BoundedFixture", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Tests/AuraRoleBattleNetworkProbe.cpp"), { TEXT("ConfiguredPopulationSlots"), TEXT("ConfiguredEnemyRows"), TEXT("ReplayCacheLimit") }) && FAuraRoleBattleNetworkProbe::ReplayCacheLimit == 256)
AURA_DAY19_TEST(FAuraDay19NoShippingMutationTest, "Aura.RoleBattle.Day19.Security.NoShippingMutationPath", AuraRoleBattleMultiplayerTests::SourceContains(TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"), { TEXT("#if !UE_BUILD_SHIPPING"), TEXT("RoleBattleDay19MultiplayerProbeClientA") }))

#undef AURA_DAY19_TEST

#endif
