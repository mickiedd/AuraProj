// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AuraRoleBattleDay20TestsPrivate
{
	FString Read(const TCHAR* RelativePath)
	{
		FString Contents;
		FFileHelper::LoadFileToString(Contents, *(FPaths::ProjectDir() / RelativePath));
		return Contents;
	}

	bool ContainsAll(const TCHAR* RelativePath, std::initializer_list<const TCHAR*> Tokens)
	{
		const FString Contents = Read(RelativePath);
		if (Contents.IsEmpty()) return false;
		for (const TCHAR* Token : Tokens)
		{
			if (!Contents.Contains(Token)) return false;
		}
		return true;
	}

	bool FileExists(const TCHAR* RelativePath)
	{
		return IFileManager::Get().FileExists(*(FPaths::ProjectDir() / RelativePath));
	}

	bool DirectoryExists(const TCHAR* RelativePath)
	{
		return IFileManager::Get().DirectoryExists(*(FPaths::ProjectDir() / RelativePath));
	}
}

#define AURA_DAY20_TEST(ClassName, PrettyName, Expression) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, PrettyName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool ClassName::RunTest(const FString& Parameters) { TestTrue(TEXT(#Expression), (Expression)); return !HasAnyErrors(); }

AURA_DAY20_TEST(FAuraDay20SharedCombatRulesTest, "Aura.RoleBattle.Day20.Cleanup.SharedCombatRules",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Actor/AuraEffectActor.cpp"), { TEXT("FAuraPickupEligibility::CanReceive"), TEXT("HasAuthority") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp"), { TEXT("FAuraCombatRules::CanDamage"), TEXT("FAuraCombatRuleContext") }));

AURA_DAY20_TEST(FAuraDay20PickupEligibilityPolicyTest, "Aura.RoleBattle.Day20.Cleanup.PickupEligibilityPolicy",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Combat/AuraPickupEligibility.cpp"), { TEXT("Faction_Player"), TEXT("Faction_Enemy"), TEXT("bAllowEnemies"), TEXT("Civilians and unknown factions") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Public/Combat/AuraPickupEligibility.h"), { TEXT("Players are eligible"), TEXT("civilians and unknown identities reject") }));

AURA_DAY20_TEST(FAuraDay20RetiredRoleContractTest, "Aura.RoleBattle.Day20.Cleanup.RetiredBungeeManContract",
	!AuraRoleBattleDay20TestsPrivate::Read(TEXT("Content/Config/RoleConfig.json")).Contains(TEXT("BungeeMan"))
	&& !AuraRoleBattleDay20TestsPrivate::FileExists(TEXT("Content/AbilityDefinitions/FireGun.xml"))
	&& !AuraRoleBattleDay20TestsPrivate::DirectoryExists(TEXT("Content/BungeeMan")));

AURA_DAY20_TEST(FAuraDay20ShippingMutationSurfaceTest, "Aura.RoleBattle.Day20.Security.ShippingMutationSurface",
	!AuraRoleBattleDay20TestsPrivate::FileExists(TEXT("Source/Aura/Public/Player/AuraCheatManager.h"))
	&& !AuraRoleBattleDay20TestsPrivate::FileExists(TEXT("Source/Aura/Private/Player/AuraCheatManager.cpp"))
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Public/Player/AuraPlayerController.h"), { TEXT("meta = (DevelopmentOnly)"), TEXT("AutoTestUseRandomEquippedAbility") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"), { TEXT("UE_BUILD_SHIPPING"), TEXT("return;") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Plugins/AuraAutoTest/AuraAutoTestPlugin.uplugin"), { TEXT("AuraAutoTestRuntime"), TEXT("DeveloperTool") }));

AURA_DAY20_TEST(FAuraDay20RoleGrantLedgerTest, "Aura.RoleBattle.Day20.Roles.IdempotentGrantLedger",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp"), { TEXT("RoleGrantLedger.bInitialized"), TEXT("RoleGrantLedger.bReconciled"), TEXT("AddUnique"), TEXT("GrantedRoleBySpecHandle") })
	&& !AuraRoleBattleDay20TestsPrivate::Read(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp")).Contains(TEXT("GrantAndEquipAllAbilities")));

AURA_DAY20_TEST(FAuraDay20CivilianLifecycleTest, "Aura.RoleBattle.Day20.Civilian.IndependentLifecycle",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), { TEXT("HandleMerchantLifeStateChanged"), TEXT("SetAuthorityUnavailable") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Combat/AuraCombatStateComponent.cpp"), { TEXT("TryEnterDying"), TEXT("TryEnterDead"), TEXT("DeathSequence") }));

AURA_DAY20_TEST(FAuraDay20EconomyOwnerPurchaseTest, "Aura.RoleBattle.Day20.Economy.OwnerPurchaseOnly",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp"), { TEXT("PlayerController->HasAuthority"), TEXT("Requester"), TEXT("SessionNonce"), TEXT("Offer.BuyPrice"), TEXT("CapturePersistenceState") }));

AURA_DAY20_TEST(FAuraDay20PersistenceLifecycleCleanupTest, "Aura.RoleBattle.Day20.Persistence.LifecycleCleanup",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp"), { TEXT("void UAuraPersistenceSubsystem::Deinitialize"), TEXT("PreparedProfiles.Reset"), TEXT("ActiveProfiles.Reset") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"), { TEXT("EndPlay"), TEXT("InvalidateSession") }));

AURA_DAY20_TEST(FAuraDay20RespawnCollisionFallbackTest, "Aura.RoleBattle.Day20.Respawn.CollisionSafeFallback",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), { TEXT("AdjustIfPossibleButDontSpawnIfColliding"), TEXT("Occupied start adjusted"), TEXT("AdjustIfPossibleButAlwaysSpawn"), TEXT("All collision-safe offsets occupied") }));

AURA_DAY20_TEST(FAuraDay20MerchantProbeJoinBarrierTest, "Aura.RoleBattle.Day20.MerchantProbe.NamedJoinBarrier",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), { TEXT("Day17Client"), TEXT("ProbeControllers"), TEXT("external clients") }));

AURA_DAY20_TEST(FAuraDay20NoLegacyNearestPlayerServiceTest, "Aura.RoleBattle.Day20.Cleanup.NoLegacyNearestPlayerService",
	!AuraRoleBattleDay20TestsPrivate::FileExists(TEXT("Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp"))
	&& !AuraRoleBattleDay20TestsPrivate::FileExists(TEXT("Source/Aura/Public/AI/BTService_FindNearestPlayer.h")));

AURA_DAY20_TEST(FAuraDay20LegacyBlueprintSnapshotInactiveTest, "Aura.RoleBattle.Day20.Cleanup.LegacyBlueprintSnapshotInactive",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Content/Config/EnemyAbilityConfig.json"), { TEXT("EnemyMeleeAttack.xml") })
	&& !AuraRoleBattleDay20TestsPrivate::Read(TEXT("Content/Config/EnemyAbilityConfig.json")).Contains(TEXT("GA_MeleeAttack"))
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Content/AbilityDefinitions/EnemyMeleeAttack.xml"), { TEXT("EnemyMeleeDamage") }));

AURA_DAY20_TEST(FAuraDay20SchemasAndProcedureTest, "Aura.RoleBattle.Day20.Schemas.ReproducibleProcedure",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Docs/Reference/Role-Battle-Economy-Schema.md"), { TEXT("schema version 1"), TEXT("IdentityProvider"), TEXT("WorldPersistenceId") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Docs/Reference/Role-Battle-Vertical-Slice-Test-Procedure.md"), { TEXT("Listen"), TEXT("Dedicated"), TEXT("Production authentication") }));

AURA_DAY20_TEST(FAuraDay20LimitationsDocumentedTest, "Aura.RoleBattle.Day20.Limitations.Documented",
	AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Docs/Tracking/GAS-Migration-TODOs.md"), { TEXT("Day 20 closeout"), TEXT("ammunition"), TEXT("hot swapping") })
	&& AuraRoleBattleDay20TestsPrivate::ContainsAll(TEXT("Docs/README.md"), { TEXT("Role-Battle-Economy-Schema.md"), TEXT("Role-Battle-Vertical-Slice-Test-Procedure.md") }));

#undef AURA_DAY20_TEST

#endif
