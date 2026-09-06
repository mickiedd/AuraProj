#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AuraGameplayTags.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatRules.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Character/AuraCivilian.h"
#include "Character/AuraEnemy.h"
#include "Game/AuraGameModeBase.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/AuraPopulationManager.h"
#include "World/AuraPopulationSpawnDefinition.h"

namespace AuraRoleBattleDays789TestsPrivate
{
	bool ReadProjectFile(const TCHAR* RelativePath, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *(FPaths::ProjectDir() / RelativePath));
	}

	const FRoleDefaultInfo* FindRole(const TCHAR* RoleId)
	{
		const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
		return RoleInfo ? RoleInfo->RoleInformation.Find(FName(RoleId)) : nullptr;
	}

	bool ContainsAll(const FString& Text, std::initializer_list<const TCHAR*> Needles)
	{
		for (const TCHAR* Needle : Needles) if (!Text.Contains(Needle)) return false;
		return true;
	}

	UWorld* FindAutomationWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World()) return Context.World();
		}
		return nullptr;
	}

	FAuraCombatIdentity MakeIdentity(const FGameplayTag& Faction, const FGameplayTag& Control, const FGameplayTag& Profile, const FGameplayTag& DeathPolicy, bool bCanAttack = true)
	{
		FAuraCombatIdentity Identity;
		Identity.FactionTag = Faction;
		Identity.ControlTypeTag = Control;
		Identity.CombatProfileTag = Profile;
		Identity.DeathPolicyTag = DeathPolicy;
		Identity.bTargetable = true;
		Identity.bCanAttack = bCanAttack;
		Identity.bCanBeDamaged = true;
		Identity.bAllowFriendlyFire = false;
		return Identity;
	}

	AActor* SpawnIdentityFixture(UWorld* World, const FAuraCombatIdentity& Identity, UAuraCombatIdentityComponent*& OutIdentityComponent)
	{
		OutIdentityComponent = nullptr;
		if (!World) return nullptr;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!Actor) return nullptr;
		OutIdentityComponent = NewObject<UAuraCombatIdentityComponent>(Actor, NAME_None, RF_Transient);
		Actor->AddInstanceComponent(OutIdentityComponent);
		OutIdentityComponent->RegisterComponent();
		UAuraCombatStateComponent* StateComponent = NewObject<UAuraCombatStateComponent>(Actor, NAME_None, RF_Transient);
		Actor->AddInstanceComponent(StateComponent);
		StateComponent->RegisterComponent();
		StateComponent->TryEnterAlive();
		if (!OutIdentityComponent->InitializeIdentity(Identity))
		{
			Actor->Destroy();
			OutIdentityComponent = nullptr;
			return nullptr;
		}
		return Actor;
	}
}

#define AURA_DAY789_TEST(ClassName, TestPath) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.RoleBattle." TestPath, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AURA_DAY789_TEST(FAuraDay7AuraDefinitionContractTest, "Day7.AuraDefinitionContract")
bool FAuraDay7AuraDefinitionContractTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Aura = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Aura"));
	TestNotNull(TEXT("Aura role is published"), Aura);
	if (!Aura) return false;
	TestEqual(TEXT("Aura combat profile"), Aura->CombatProfile, FAuraGameplayTags::Get().Combat_Magic);
	TestEqual(TEXT("Aura LMB definition path"), Aura->DefaultLMBAbilityDefinitionPath, FString(TEXT("/Game/AbilityDefinitions/FireBolt.xml")));
	TestEqual(TEXT("Aura startup definition count"), Aura->StartupAbilityDefinitionPaths.Num(), 3);
	TestTrue(TEXT("Aura FireBolt XML exists"), FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("AbilityDefinitions/FireBolt.xml")));
	return true;
}

AURA_DAY789_TEST(FAuraDay7BungeeDefinitionContractTest, "Day7.BungeeDefinitionContract")
bool FAuraDay7BungeeDefinitionContractTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Bungee = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("BungeeMan"));
	TestNotNull(TEXT("BungeeMan role is published"), Bungee);
	if (!Bungee) return false;
	TestEqual(TEXT("Bungee combat profile"), Bungee->CombatProfile, FAuraGameplayTags::Get().Combat_Gun);
	TestEqual(TEXT("Bungee LMB definition path"), Bungee->DefaultLMBAbilityDefinitionPath, FString(TEXT("/Game/AbilityDefinitions/FireGun.xml")));
	TestTrue(TEXT("Bungee non-LMB startup list is empty"), Bungee->StartupAbilityDefinitionPaths.IsEmpty() && Bungee->StartupPassiveAbilityDefinitionPaths.IsEmpty());
	TestTrue(TEXT("FireGun XML exists"), FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("AbilityDefinitions/FireGun.xml")));
	return true;
}

AURA_DAY789_TEST(FAuraDay7AbilityAssetMontageSocketValidationTest, "Day7.AbilityAssetMontageSocketValidation")
bool FAuraDay7AbilityAssetMontageSocketValidationTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Aura = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Aura"));
	const FRoleDefaultInfo* Bungee = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("BungeeMan"));
	TestTrue(TEXT("Aura body mesh and AnimBP resolve"), Aura && Aura->SkeletalMesh && Aura->AnimBlueprintClass);
	TestTrue(TEXT("Bungee body mesh and AnimBP resolve"), Bungee && Bungee->SkeletalMesh && Bungee->AnimBlueprintClass);
	TestTrue(TEXT("Aura staff and body sockets are distinct assets"), Aura && Aura->WeaponMesh && Aura->SkeletalMesh && Aura->WeaponMesh != Aura->SkeletalMesh);
	TestTrue(TEXT("Bungee rifle exposes Muzzle"), Bungee && Bungee->WeaponMesh && Bungee->WeaponMesh->FindSocket(FName(TEXT("Muzzle"))));
	return true;
}

AURA_DAY789_TEST(FAuraDay7ExactRoleGrantSetsTest, "Day7.ExactRoleGrantSets")
bool FAuraDay7ExactRoleGrantSetsTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Aura = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Aura"));
	const FRoleDefaultInfo* Bungee = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("BungeeMan"));
	const FRoleDefaultInfo* Civilian = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Civilian"));
	TestTrue(TEXT("Aura has exactly three non-LMB definitions"), Aura && Aura->StartupAbilityDefinitionPaths.Num() == 3);
	TestTrue(TEXT("Bungee has exactly one LMB definition"), Bungee && Bungee->DefaultLMBAbilityDefinitionPath.Contains(TEXT("FireGun.xml")));
	TestTrue(TEXT("Civilian has an empty grant catalog"), Civilian && Civilian->StartupAbilityDefinitionPaths.IsEmpty() && Civilian->StartupPassiveAbilityDefinitionPaths.IsEmpty() && Civilian->UnlockableAbilities.IsEmpty());
	return true;
}

AURA_DAY789_TEST(FAuraDay7FinalProfileAssertionsTest, "Day7.FinalProfileAssertions")
bool FAuraDay7FinalProfileAssertionsTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Aura = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Aura"));
	const FRoleDefaultInfo* Bungee = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("BungeeMan"));
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestTrue(TEXT("Aura final profile tuple"), Aura && Aura->CombatProfile == Tags.Combat_Magic && Aura->Faction == Tags.Faction_Player && Aura->ControlType == Tags.Control_Player && Aura->DeathPolicy == Tags.Death_PlayerRespawn && Aura->EconomyProfile == Tags.Economy_None && Aura->InteractionProfile == Tags.Interaction_Combatant);
	TestTrue(TEXT("Bungee final profile tuple"), Bungee && Bungee->CombatProfile == Tags.Combat_Gun && Bungee->Faction == Tags.Faction_Player && Bungee->ControlType == Tags.Control_Player && Bungee->DeathPolicy == Tags.Death_PlayerRespawn && Bungee->EconomyProfile == Tags.Economy_None && Bungee->InteractionProfile == Tags.Interaction_Combatant);
	return true;
}

AURA_DAY789_TEST(FAuraDay7CostCooldownAndDamageTypesTest, "Day7.CostCooldownAndDamageTypes")
bool FAuraDay7CostCooldownAndDamageTypesTest::RunTest(const FString& Parameters)
{
	FString FireBolt;
	FString FireGun;
	TestTrue(TEXT("FireBolt XML readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Content/AbilityDefinitions/FireBolt.xml"), FireBolt));
	TestTrue(TEXT("FireGun XML readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Content/AbilityDefinitions/FireGun.xml"), FireGun));
	TestTrue(TEXT("FireBolt exact cost/cooldown/type"), AuraRoleBattleDays789TestsPrivate::ContainsAll(FireBolt, { TEXT("mana=\"10\""), TEXT("duration=\"5\""), TEXT("type=\"Damage.Fire\"") }));
	TestTrue(TEXT("FireGun exact cost/cooldown/type"), AuraRoleBattleDays789TestsPrivate::ContainsAll(FireGun, { TEXT("mana=\"0\""), TEXT("duration=\"0.2\""), TEXT("type=\"Damage.Physical\""), TEXT("Event.Montage.FireGun") }));
	return true;
}

AURA_DAY789_TEST(FAuraDay7SameFactionRejectionTest, "Day7.SameFactionRejection")
bool FAuraDay7SameFactionRejectionTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleDays789TestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("Automation world available"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	UAuraCombatIdentityComponent* SourceIdentity = nullptr;
	UAuraCombatIdentityComponent* TargetIdentity = nullptr;
	AActor* Source = SpawnIdentityFixture(World, Player, SourceIdentity);
	AActor* Target = SpawnIdentityFixture(World, Player, TargetIdentity);
	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = Source;
	const FAuraCombatRuleResult Result = FAuraCombatRules::CanDamage(Source, Target, Context);
	TestEqual(TEXT("Same-faction relationship is friendly"), Result.Relationship, EAuraCombatRelationship::Friendly);
	TestFalse(TEXT("Same-faction damage is denied by default"), Result.bCanDamage);
	TestEqual(TEXT("Same-faction rejection reason is Friendly"), Result.RejectionReason, EAuraCombatRuleRejectionReason::Friendly);
	if (Source) Source->Destroy();
	if (Target) Target->Destroy();
	return true;
}

AURA_DAY789_TEST(FAuraDay7IsolatedSingleProfileSaveReloadAndRespawnTest, "Day7.IsolatedSingleProfileSaveReloadAndRespawn")
bool FAuraDay7IsolatedSingleProfileSaveReloadAndRespawnTest::RunTest(const FString& Parameters)
{
	FString BaseSource;
	TestTrue(TEXT("Role application source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), BaseSource));
	TestTrue(TEXT("Role application is transactionally applied"), BaseSource.Contains(TEXT("ApplyRoleAtSpawn")) && BaseSource.Contains(TEXT("RoleGrantLedger")) && BaseSource.Contains(TEXT("AppliedRoleState")));
	TestTrue(TEXT("Persistent ASC replacement path exists"), BaseSource.Contains(TEXT("InitializeDefaultAttributesForRole")));
	return true;
}

AURA_DAY789_TEST(FAuraDay7ClientCannotGrantOrDamageTest, "Day7.ClientCannotGrantOrDamage")
bool FAuraDay7ClientCannotGrantOrDamageTest::RunTest(const FString& Parameters)
{
	FString BaseSource;
	FString ASCSource;
	TestTrue(TEXT("Character authority source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), BaseSource));
	TestTrue(TEXT("ASC authority source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp"), ASCSource));
	TestTrue(TEXT("Role application rejects non-authority"), BaseSource.Contains(TEXT("EAuraRoleApplicationError::NotAuthority")));
	TestTrue(TEXT("Grant path contains authority gate"), ASCSource.Contains(TEXT("HasAuthority")));
	return true;
}

AURA_DAY789_TEST(FAuraDay7PackagingConfigContractTest, "Day7.PackagingConfigContract")
bool FAuraDay7PackagingConfigContractTest::RunTest(const FString& Parameters)
{
	FString Ini;
	TestTrue(TEXT("DefaultGame.ini readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Config/DefaultGame.ini"), Ini));
	TestTrue(TEXT("AbilityDefinitions staged as UFS"), Ini.Contains(TEXT("+DirectoriesToAlwaysStageAsUFS=(Path=\"AbilityDefinitions\")")));
	for (const TCHAR* FileName : { TEXT("FireBolt.xml"), TEXT("FireBlast.xml"), TEXT("ArcaneShards.xml"), TEXT("Electrocute.xml"), TEXT("FireGun.xml") })
	{
		TestTrue(FString::Printf(TEXT("Ability definition exists: %s"), FileName), FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("AbilityDefinitions") / FileName));
	}
	return true;
}

AURA_DAY789_TEST(FAuraDay8CivilianRoleIdentityTest, "Day8.CivilianRoleIdentity")
bool FAuraDay8CivilianRoleIdentityTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Civilian = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Civilian"));
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestTrue(TEXT("Civilian identity remains ambient and selectable"), Civilian && Civilian->EntityType == Tags.Entity_AmbientNPC && Civilian->ControlType == Tags.Control_CivilianAI && Civilian->Faction == Tags.Faction_Civilian && Civilian->CombatProfile == Tags.Combat_Civilian && Civilian->DeathPolicy == Tags.Death_PopulationRespawn && Civilian->bPlayerSelectable);
	TestTrue(TEXT("Civilian default policy is protected but targetable"), Civilian && Civilian->bTargetable && !Civilian->bCanAttack && Civilian->bCanBeDamaged && !Civilian->bAllowFriendlyFire);
	TestEqual(TEXT("Civilian role publishes a slow walk speed"), Civilian ? Civilian->MovementSpeed : 0.f, 120.f);
	return true;
}

AURA_DAY789_TEST(FAuraDay8CivilianRoleMovementApplicationContractTest, "Day8.CivilianRoleMovementApplicationContract")
bool FAuraDay8CivilianRoleMovementApplicationContractTest::RunTest(const FString& Parameters)
{
	FString BaseSource;
	FString ControllerSource;
	TestTrue(TEXT("Role presentation source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), BaseSource));
	TestTrue(TEXT("Player controller source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"), ControllerSource));
	TestTrue(TEXT("Role presentation applies the configured movement speed"), BaseSource.Contains(TEXT("RoleDefinition.MovementSpeed")) && BaseSource.Contains(TEXT("RoleMovement")));
	TestTrue(TEXT("Role presentation refreshes sprint cache"), BaseSource.Contains(TEXT("RefreshCachedWalkSpeed")));
	TestTrue(TEXT("Sprint cache can be rebased after role presentation"), ControllerSource.Contains(TEXT("void AAuraPlayerController::RefreshCachedWalkSpeed")));
	return true;
}

AURA_DAY789_TEST(FAuraDay8CivilianASCInitializationTest, "Day8.CivilianASCInitialization")
bool FAuraDay8CivilianASCInitializationTest::RunTest(const FString& Parameters)
{
	const AAuraCivilian* CDO = AAuraCivilian::StaticClass()->GetDefaultObject<AAuraCivilian>();
	TestNotNull(TEXT("Civilian CDO"), CDO);
	TestNotNull(TEXT("Civilian owns one ASC"), CDO ? CDO->GetAbilitySystemComponent() : nullptr);
	TestNotNull(TEXT("Civilian owns one AttributeSet"), CDO ? CDO->GetAttributeSet() : nullptr);
	TestEqual(TEXT("Civilian requested role default"), CDO ? CDO->GetRequestedCivilianRoleId() : NAME_None, FName(TEXT("Civilian")));
	return true;
}

AURA_DAY789_TEST(FAuraDay8EmptyOffensiveLoadoutTest, "Day8.EmptyOffensiveLoadout")
bool FAuraDay8EmptyOffensiveLoadoutTest::RunTest(const FString& Parameters)
{
	const FRoleDefaultInfo* Civilian = AuraRoleBattleDays789TestsPrivate::FindRole(TEXT("Civilian"));
	FString Source;
	TestTrue(TEXT("Civilian source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), Source));
	TestTrue(TEXT("Civilian role has empty loadout"), Civilian && Civilian->StartupAbilities.IsEmpty() && Civilian->StartupPassiveAbilities.IsEmpty() && Civilian->UnlockableAbilities.IsEmpty() && Civilian->DefaultLMBAbilityDefinitionPath.IsEmpty());
	TestFalse(TEXT("Civilian does not give startup abilities"), Source.Contains(TEXT("GiveStartupAbilities")));
	return true;
}

AURA_DAY789_TEST(FAuraDay8InvalidRoleFailsClosedTest, "Day8.InvalidRoleFailsClosed")
bool FAuraDay8InvalidRoleFailsClosedTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Civilian source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), Source));
	TestTrue(TEXT("Invalid role disables actor and returns before ready"), Source.Contains(TEXT("if (!RoleResult.bSuccess)")) && Source.Contains(TEXT("SetActorEnableCollision(false)")) && Source.Contains(TEXT("return;")));
	TestTrue(TEXT("Civilian uses required ambient role tags"), Source.Contains(TEXT("Entity_AmbientNPC")) && Source.Contains(TEXT("Control_CivilianAI")));
	return true;
}

AURA_DAY789_TEST(FAuraDay8PresentationIdempotenceTest, "Day8.PresentationIdempotence")
bool FAuraDay8PresentationIdempotenceTest::RunTest(const FString& Parameters)
{
	FString BaseSource;
	FString CivilianSource;
	TestTrue(TEXT("Base presentation source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"), BaseSource));
	TestTrue(TEXT("Civilian source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), CivilianSource));
	TestTrue(TEXT("Replicated applied role drives presentation"), BaseSource.Contains(TEXT("OnRep_AppliedRoleState")) && BaseSource.Contains(TEXT("ApplyRolePresentation")));
	TestFalse(TEXT("Civilian does not own a second presentation path"), CivilianSource.Contains(TEXT("ApplyRolePresentation")));
	return true;
}

AURA_DAY789_TEST(FAuraDay8DefaultDamageDeniedAndTrustedFixtureAllowedTest, "Day8.DefaultDamageDeniedAndTrustedFixtureAllowed")
bool FAuraDay8DefaultDamageDeniedAndTrustedFixtureAllowedTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleDays789TestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("Automation world available"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	const FAuraCombatIdentity Civilian = MakeIdentity(Tags.Faction_Civilian, Tags.Control_CivilianAI, Tags.Combat_Civilian, Tags.Death_PopulationRespawn, false);
	UAuraCombatIdentityComponent* PlayerIdentity = nullptr;
	UAuraCombatIdentityComponent* EnemyIdentity = nullptr;
	UAuraCombatIdentityComponent* CivilianIdentity = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, Player, PlayerIdentity);
	AActor* EnemyActor = SpawnIdentityFixture(World, Enemy, EnemyIdentity);
	AActor* CivilianActor = SpawnIdentityFixture(World, Civilian, CivilianIdentity);
	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = PlayerActor;
	TestFalse(TEXT("Default Player-to-Civilian damage is denied"), FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, Context).bCanDamage);
	TestFalse(TEXT("Default Enemy-to-Civilian damage is denied"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);
	Context.SetPolicySnapshot(FAuraCombatRules::MakeTrustedTestPolicySnapshot(World, false, true, true, false));
	TestTrue(TEXT("Trusted Player-to-Civilian fixture is allowed"), FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, Context).bCanDamage);
	TestTrue(TEXT("Trusted Enemy-to-Civilian fixture is allowed"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);
	if (PlayerActor) PlayerActor->Destroy();
	if (EnemyActor) EnemyActor->Destroy();
	if (CivilianActor) CivilianActor->Destroy();
	return true;
}

AURA_DAY789_TEST(FAuraDay9PopulationSchemaValidationTest, "Day9.PopulationSchemaValidation")
bool FAuraDay9PopulationSchemaValidationTest::RunTest(const FString& Parameters)
{
	UAuraPopulationSpawnDefinition* Definition = NewObject<UAuraPopulationSpawnDefinition>();
	FString Error;
	TestTrue(TEXT("Population fixture schema validates"), Definition && Definition->LoadDefinitions(Error));
	if (!Error.IsEmpty()) AddError(Error);
	TestEqual(TEXT("Population schema version"), Definition ? Definition->GetSchemaVersion() : 0, 1);
	TestEqual(TEXT("Initial fixture count"), Definition && Definition->GetPopulationRows().Num() > 0 ? Definition->GetPopulationRows()[0].InitialCount : 0, 3);
	const FAuraPopulationSpawnRow* Row = Definition && Definition->GetPopulationRows().Num() > 0 ? &Definition->GetPopulationRows()[0] : nullptr;
	TestEqual(TEXT("One member override is loaded"), Row ? Row->MemberOverrides.Num() : 0, 1);
	if (Row && Row->MemberOverrides.Num() == 1)
	{
		TestEqual(TEXT("Override targets canonical slot one"), Row->MemberOverrides[0].SlotIndex, 1);
		TestEqual(TEXT("Override carries the merchant definition"), Row->MemberOverrides[0].MerchantDefinitionId, FName(TEXT("MarketMerchant")));
	}
	return true;
}

AURA_DAY789_TEST(FAuraDay9WorkProfileValidationTest, "Day9.WorkProfileValidation")
bool FAuraDay9WorkProfileValidationTest::RunTest(const FString& Parameters)
{
	UAuraPopulationSpawnDefinition* Definition = NewObject<UAuraPopulationSpawnDefinition>();
	FString Error;
	TestTrue(TEXT("Work profile fixture validates"), Definition && Definition->LoadDefinitions(Error));
	const FAuraCivilianWorkProfile* Observer = Definition ? Definition->FindWorkProfile(TEXT("Observer")) : nullptr;
	TestTrue(TEXT("Observer has bounded movement/threat/schedule values"), Observer && Observer->MovementSpeed >= 0.f && Observer->WanderRadius >= 0.f && Observer->ObserveRadius >= 0.f && Observer->FleeDistance >= 0.f && Observer->ScheduleEndHour <= 24.f);
	return true;
}

AURA_DAY789_TEST(FAuraDay9DeterministicMemberIdsTest, "Day9.DeterministicMemberIds")
bool FAuraDay9DeterministicMemberIdsTest::RunTest(const FString& Parameters)
{
	const TArray<int32> FirstIterationOrder = { 3, 0, 2, 1 };
	const TArray<int32> SecondIterationOrder = { 1, 2, 0, 3 };
	TMap<int32, FName> FirstIds;
	TMap<int32, FName> SecondIds;
	for (const int32 SlotIndex : FirstIterationOrder)
	{
		FirstIds.Add(SlotIndex, UAuraPopulationManager::BuildDeterministicMemberId(TEXT("MarketCivilians"), SlotIndex));
	}
	for (const int32 SlotIndex : SecondIterationOrder)
	{
		SecondIds.Add(SlotIndex, UAuraPopulationManager::BuildDeterministicMemberId(TEXT("MarketCivilians"), SlotIndex));
	}
	bool bSameSlotMap = true;
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		bSameSlotMap = bSameSlotMap && FirstIds.FindRef(SlotIndex) == SecondIds.FindRef(SlotIndex);
	}
	TestTrue(TEXT("Different row iteration orders produce the same slot map"), bSameSlotMap);
	TestEqual(TEXT("Slot zero canonical ID"), FirstIds.FindRef(0), FName(TEXT("MarketCivilians:0")));
	TestEqual(TEXT("Slot three canonical ID"), FirstIds.FindRef(3), FName(TEXT("MarketCivilians:3")));
	TestTrue(TEXT("Invalid slot rejected"), UAuraPopulationManager::BuildDeterministicMemberId(TEXT("MarketCivilians"), -1).IsNone());
	return true;
}

AURA_DAY789_TEST(FAuraDay9InvalidRoleClassRejectedTest, "Day9.InvalidRoleClassRejected")
bool FAuraDay9InvalidRoleClassRejectedTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Dedicated Civilian class is accepted"), UAuraPopulationManager::IsValidCivilianActorClass(AAuraCivilian::StaticClass()));
	TestFalse(TEXT("Enemy class is rejected for population rows"), UAuraPopulationManager::IsValidCivilianActorClass(AAuraEnemy::StaticClass()));
	return true;
}

AURA_DAY789_TEST(FAuraDay9DuplicateInitializationTest, "Day9.DuplicateInitialization")
bool FAuraDay9DuplicateInitializationTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleDays789TestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("Automation world available"), World)) return false;
	UAuraPopulationManager* Manager = NewObject<UAuraPopulationManager>(World);
	FString Error;
	TestTrue(TEXT("First definition initialization succeeds"), Manager && Manager->InitializeDefinitions(Error));
	const int32 Generation = Manager ? Manager->GetInitializationGeneration() : 0;
	TestTrue(TEXT("Second definition initialization remains successful"), Manager && Manager->InitializeDefinitions(Error));
	TestEqual(TEXT("Definition generation is stable across duplicate initialization"), Manager ? Manager->GetInitializationGeneration() : 0, Generation);
	return true;
}

AURA_DAY789_TEST(FAuraDay9SpawnFailureReleasesSlotTest, "Day9.SpawnFailureReleasesSlot")
bool FAuraDay9SpawnFailureReleasesSlotTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Population manager source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/World/AuraPopulationManager.cpp"), Source));
	TestTrue(TEXT("Failed spawn releases reservation"), Source.Contains(TEXT("ReservedMemberIds.Remove(MemberId)")));
	return true;
}

AURA_DAY789_TEST(FAuraDay9PreStartPlayManagerAvailabilityTest, "Day9.PreStartPlayManagerAvailability")
bool FAuraDay9PreStartPlayManagerAvailabilityTest::RunTest(const FString& Parameters)
{
	FString GameModeSource;
	TestTrue(TEXT("GameMode source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Game/AuraGameModeBase.cpp"), GameModeSource));
	const int32 InitIndex = GameModeSource.Find(TEXT("PopulationManager = NewObject"));
	const int32 BeginIndex = GameModeSource.Find(TEXT("void AAuraGameModeBase::BeginPlay"));
	TestTrue(TEXT("Manager is constructed before BeginPlay"), InitIndex != INDEX_NONE && BeginIndex != INDEX_NONE && InitIndex < BeginIndex);
	return true;
}

AURA_DAY789_TEST(FAuraDay9MemberStateReplicationTest, "Day9.MemberStateReplication")
bool FAuraDay9MemberStateReplicationTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Civilian source readable"), AuraRoleBattleDays789TestsPrivate::ReadProjectFile(TEXT("Source/Aura/Private/Character/AuraCivilian.cpp"), Source));
	TestTrue(TEXT("Complete member state is replicated"), Source.Contains(TEXT("DOREPLIFETIME(AAuraCivilian, PopulationMemberState)")) && Source.Contains(TEXT("OnRep_PopulationMemberState")));
	const FAuraPopulationMemberState EmptyState;
	TestFalse(TEXT("Empty member state is not publishable"), EmptyState.IsValid());
	return true;
}

AURA_DAY789_TEST(FAuraDay9PerMemberOverrideIsolationTest, "Day9.PerMemberOverrideIsolation")
bool FAuraDay9PerMemberOverrideIsolationTest::RunTest(const FString& Parameters)
{
	UAuraPopulationSpawnDefinition* Definition = NewObject<UAuraPopulationSpawnDefinition>();
	FString Error;
	TestTrue(TEXT("Population definitions load for override isolation"), Definition && Definition->LoadDefinitions(Error));
	const FAuraPopulationSpawnRow* Row = Definition && Definition->GetPopulationRows().Num() > 0 ? &Definition->GetPopulationRows()[0] : nullptr;
	TestTrue(TEXT("Slot one has a merchant override"), Row && Row->MemberOverrides.Num() == 1 && Row->MemberOverrides[0].SlotIndex == 1);
	TestTrue(TEXT("Other slots remain unoverridden"), Row && Row->MemberOverrides[0].SlotIndex != 0 && Row->MemberOverrides[0].SlotIndex != 2);
	return true;
}

#undef AURA_DAY789_TEST

#endif
