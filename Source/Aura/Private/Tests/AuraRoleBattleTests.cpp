#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "AuraAttributeGameplayEffect.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "Actor/AuraProjectile.h"
#include "Character/AuraCharacter.h"
#include "Character/AuraEnemy.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatRules.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraCombatTypes.h"
#include "Data/AuraGameplayConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Game/LoadScreenSaveGame.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/CoreNet.h"
#include "Player/AuraPlayerState.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#if WITH_EDITOR
#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"
#endif

namespace AuraRoleBattleTestsPrivate
{
	UWorld* FindAutomationWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	AActor* SpawnIdentityFixture(UWorld* World, const FAuraCombatIdentity* Identity, UAuraCombatIdentityComponent*& OutComponent)
	{
		OutComponent = nullptr;
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!Actor)
		{
			return nullptr;
		}

		OutComponent = NewObject<UAuraCombatIdentityComponent>(Actor, NAME_None, RF_Transient);
		Actor->AddInstanceComponent(OutComponent);
		OutComponent->RegisterComponent();
		UAuraCombatStateComponent* StateComponent = NewObject<UAuraCombatStateComponent>(Actor, NAME_None, RF_Transient);
		Actor->AddInstanceComponent(StateComponent);
		StateComponent->RegisterComponent();
		StateComponent->TryEnterAlive();
		if (Identity && !OutComponent->InitializeIdentity(*Identity))
		{
			Actor->Destroy();
			OutComponent = nullptr;
			return nullptr;
		}
		return Actor;
	}

	FAuraCombatIdentity MakeIdentity(
		const FGameplayTag& Faction,
		const FGameplayTag& Control,
		const FGameplayTag& Profile,
		const FGameplayTag& DeathPolicy,
		bool bCanAttack = true,
		bool bTargetable = true,
		bool bCanBeDamaged = true,
		bool bAllowFriendlyFire = false)
	{
		FAuraCombatIdentity Identity;
		Identity.FactionTag = Faction;
		Identity.ControlTypeTag = Control;
		Identity.CombatProfileTag = Profile;
		Identity.DeathPolicyTag = DeathPolicy;
		Identity.bTargetable = bTargetable;
		Identity.bCanAttack = bCanAttack;
		Identity.bCanBeDamaged = bCanBeDamaged;
		Identity.bAllowFriendlyFire = bAllowFriendlyFire;
		return Identity;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraRoleBattleCatalogTest,
	"Aura.RoleBattle.Day1Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraRoleBattleCatalogTest::RunTest(const FString& Parameters)
{
	const URoleInfo* Roles = UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
	if (!TestNotNull(TEXT("RoleConfig.json loads"), Roles))
	{
		return false;
	}

	const FRoleDefaultInfo* Bungee = Roles->RoleInformation.Find(FName(TEXT("BungeeMan")));
	if (!TestNotNull(TEXT("BungeeMan role exists"), Bungee))
	{
		return false;
	}

	TestTrue(TEXT("BungeeMan role is configured"), Roles->IsRoleConfigured(FName(TEXT("BungeeMan"))));
	TestNotNull(TEXT("BungeeMan body mesh resolves"), Bungee->SkeletalMesh.Get());
	TestNotNull(TEXT("BungeeMan animation blueprint resolves"), Bungee->AnimBlueprintClass.Get());
	TestNotNull(TEXT("BungeeMan rifle mesh resolves"), Bungee->WeaponMesh.Get());
	TestEqual(TEXT("BungeeMan weapon tip socket is Muzzle"), Bungee->WeaponTipSocketName, FName(TEXT("Muzzle")));
	if (Bungee->WeaponMesh)
	{
		TestTrue(TEXT("BungeeMan rifle exposes Muzzle socket"), Bungee->WeaponMesh->FindSocket(Bungee->WeaponTipSocketName) != nullptr);
	}

	const UAuraAbilityDefinition* FireGun = Cast<UAuraAbilityDefinition>(Bungee->DefaultLMBAbilityDefinition.Get());
	if (!TestNotNull(TEXT("BungeeMan FireGun definition resolves"), FireGun))
	{
		return false;
	}
	TestEqual(TEXT("BungeeMan FireGun tag"), FireGun->AbilityTag, FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire")));
	TestEqual(TEXT("BungeeMan FireGun input"), FireGun->InputTag, FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB")));
	TestNotNull(TEXT("BungeeMan FireGun graph resolves"), FireGun->RootNode.Get());

	const FAuraProjectileDefinition* Bullet = FAuraGameplayConfig::FindProjectile(TEXT("fireGunBullet"));
	if (!TestNotNull(TEXT("FireGun bullet definition resolves"), Bullet))
	{
		return false;
	}
	TestEqual(TEXT("FireGun bullet uses native projectile"), Bullet->NativeClass.Get(), AAuraProjectile::StaticClass());
	TestEqual(TEXT("FireGun bullet speed"), Bullet->InitialSpeed, 550.f);
	TestEqual(TEXT("FireGun bullet collision radius"), Bullet->CollisionRadius, 15.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraRespawnAttributeGuardTest,
	"Aura.RoleBattle.Day1RespawnAttributeGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraRespawnAttributeGuardTest::RunTest(const FString& Parameters)
{
	AAuraPlayerState* PlayerState = NewObject<AAuraPlayerState>(GetTransientPackage());
	if (!TestNotNull(TEXT("Transient AuraPlayerState created"), PlayerState))
	{
		return false;
	}

	TestFalse(TEXT("Persistent attribute guard starts clear"), PlayerState->HasInitializedDefaultAttributes());
	PlayerState->MarkDefaultAttributesInitialized();
	TestTrue(TEXT("Persistent attribute guard records initialization"), PlayerState->HasInitializedDefaultAttributes());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAttributeMagnitudeDefaultsTest,
	"Aura.RoleBattle.Day1AttributeMagnitudeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAttributeMagnitudeDefaultsTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World())
		{
			World = Context.World();
			break;
		}
	}
	if (!TestNotNull(TEXT("World available for attribute smoke fixture"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AActor* Owner = World->SpawnActor<AActor>(SpawnParameters);
	if (!TestNotNull(TEXT("Transient attribute owner spawned"), Owner))
	{
		return false;
	}

	UAuraAbilitySystemComponent* ASC = NewObject<UAuraAbilitySystemComponent>(Owner);
	ASC->RegisterComponent();
	UAuraAttributeSet* Attributes = NewObject<UAuraAttributeSet>(ASC);
	ASC->AddAttributeSetSubobject(Attributes);
	ASC->InitAbilityActorInfo(Owner, Owner);
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetMaxHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), 10.f);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UAuraPickupGameplayEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!TestTrue(TEXT("Attribute default spec created"), Spec.IsValid()))
	{
		Owner->Destroy();
		return false;
	}

	UAuraAbilitySystemLibrary::AssignDefaultAttributeMagnitudes(Spec);
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	for (const FGameplayTag& Tag : {
		Tags.Attributes_Primary_Strength,
		Tags.Attributes_Secondary_MaxHealth,
		Tags.Attributes_Resistance_Fire,
		Tags.Attributes_Vital_Health,
		Tags.Attributes_Vital_Mana})
	{
		TestTrue(FString::Printf(TEXT("Default magnitude assigned for %s"), *Tag.ToString()),
			FMath::IsNearlyZero(Spec.Data->GetSetByCallerMagnitude(Tag, false, -1.f)));
	}

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Tags.Attributes_Vital_Health, 50.f);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	TestEqual(TEXT("Selective vital override still applies"), Attributes->GetHealth(), 60.f);
	Owner->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCombatIdentityDefaultsTest,
	"Aura.RoleBattle.Day2.IdentityDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCombatIdentityDefaultsTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	for (const FGameplayTag& RequiredTag : {
		Tags.Faction_Player,
		Tags.Faction_Enemy,
		Tags.Faction_Civilian,
		Tags.Control_Player,
		Tags.Control_EnemyAI,
		Tags.Control_CivilianAI,
		Tags.Combat_Unassigned,
		Tags.Combat_Magic,
		Tags.Combat_Gun,
		Tags.Combat_Civilian,
		Tags.Death_PlayerRespawn,
		Tags.Death_EnemyLoot,
		Tags.Death_PopulationRespawn})
	{
		TestTrue(FString::Printf(TEXT("Native identity tag is valid: %s"), *RequiredTag.ToString()), RequiredTag.IsValid());
	}

	const FAuraCombatIdentity EmptyIdentity;
	TestFalse(TEXT("Default identity is invalid"), EmptyIdentity.IsValid());
	TestFalse(TEXT("Default identity is not targetable"), EmptyIdentity.bTargetable);
	TestFalse(TEXT("Default identity cannot attack"), EmptyIdentity.bCanAttack);
	TestFalse(TEXT("Default identity cannot be damaged"), EmptyIdentity.bCanBeDamaged);
	TestFalse(TEXT("Default identity disallows friendly fire"), EmptyIdentity.bAllowFriendlyFire);

	const AAuraCharacter* PlayerCDO = GetDefault<AAuraCharacter>();
	const AAuraEnemy* EnemyCDO = GetDefault<AAuraEnemy>();
	if (!TestNotNull(TEXT("AuraCharacter CDO exists"), PlayerCDO)
		|| !TestNotNull(TEXT("AuraEnemy CDO exists"), EnemyCDO))
	{
		return false;
	}

	const FAuraCombatIdentity PlayerIdentity = PlayerCDO->GetResolvedDefaultCombatIdentity();
	const FAuraCombatIdentity EnemyIdentity = EnemyCDO->GetResolvedDefaultCombatIdentity();
	TestTrue(TEXT("Player default identity is valid"), PlayerIdentity.IsValid());
	TestEqual(TEXT("Player faction"), PlayerIdentity.FactionTag, Tags.Faction_Player);
	TestEqual(TEXT("Player control"), PlayerIdentity.ControlTypeTag, Tags.Control_Player);
	TestEqual(TEXT("Player profile"), PlayerIdentity.CombatProfileTag, Tags.Combat_Unassigned);
	TestEqual(TEXT("Player death policy"), PlayerIdentity.DeathPolicyTag, Tags.Death_PlayerRespawn);
	TestTrue(TEXT("Player is targetable, attacking, and damageable"),
		PlayerIdentity.bTargetable && PlayerIdentity.bCanAttack && PlayerIdentity.bCanBeDamaged);
	TestFalse(TEXT("Player friendly fire is disabled"), PlayerIdentity.bAllowFriendlyFire);

	TestTrue(TEXT("Enemy default identity is valid"), EnemyIdentity.IsValid());
	TestEqual(TEXT("Enemy faction"), EnemyIdentity.FactionTag, Tags.Faction_Enemy);
	TestEqual(TEXT("Enemy control"), EnemyIdentity.ControlTypeTag, Tags.Control_EnemyAI);
	TestEqual(TEXT("Enemy profile"), EnemyIdentity.CombatProfileTag, Tags.Combat_Unassigned);
	TestEqual(TEXT("Enemy death policy"), EnemyIdentity.DeathPolicyTag, Tags.Death_EnemyLoot);
	TestTrue(TEXT("Enemy is targetable, attacking, and damageable"),
		EnemyIdentity.bTargetable && EnemyIdentity.bCanAttack && EnemyIdentity.bCanBeDamaged);
	TestFalse(TEXT("Enemy friendly fire is disabled"), EnemyIdentity.bAllowFriendlyFire);

	TestNotNull(TEXT("Player owns one combat identity component"), PlayerCDO->GetCombatIdentityComponent());
	TestNotNull(TEXT("Enemy owns one combat identity component"), EnemyCDO->GetCombatIdentityComponent());
	if (PlayerCDO->GetCombatIdentityComponent() && EnemyCDO->GetCombatIdentityComponent())
	{
		TestTrue(TEXT("Player identity component replicates"), PlayerCDO->GetCombatIdentityComponent()->GetIsReplicated());
		TestTrue(TEXT("Enemy identity component replicates"), EnemyCDO->GetCombatIdentityComponent()->GetIsReplicated());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraIsNotFriendCompatibilityTest,
	"Aura.RoleBattle.Day2.IsNotFriendCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraIsNotFriendCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for identity compatibility fixtures"), World))
	{
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity PlayerIdentity = MakeIdentity(
		Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity EnemyIdentity = MakeIdentity(
		Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	const FAuraCombatIdentity CivilianIdentity = MakeIdentity(
		Tags.Faction_Civilian, Tags.Control_CivilianAI, Tags.Combat_Civilian, Tags.Death_PopulationRespawn, false);

	UAuraCombatIdentityComponent* PlayerOneComponent = nullptr;
	UAuraCombatIdentityComponent* PlayerTwoComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyOneComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyTwoComponent = nullptr;
	UAuraCombatIdentityComponent* CivilianComponent = nullptr;
	UAuraCombatIdentityComponent* MissingComponent = nullptr;
	UAuraCombatIdentityComponent* InvalidComponent = nullptr;
	AActor* PlayerOne = SpawnIdentityFixture(World, &PlayerIdentity, PlayerOneComponent);
	AActor* PlayerTwo = SpawnIdentityFixture(World, &PlayerIdentity, PlayerTwoComponent);
	AActor* EnemyOne = SpawnIdentityFixture(World, &EnemyIdentity, EnemyOneComponent);
	AActor* EnemyTwo = SpawnIdentityFixture(World, &EnemyIdentity, EnemyTwoComponent);
	AActor* Civilian = SpawnIdentityFixture(World, &CivilianIdentity, CivilianComponent);
	AActor* Missing = World->SpawnActor<AActor>();
	AActor* Invalid = SpawnIdentityFixture(World, nullptr, InvalidComponent);

	const TArray<AActor*> Fixtures = {PlayerOne, PlayerTwo, EnemyOne, EnemyTwo, Civilian, Missing, Invalid};
	for (AActor* Fixture : Fixtures)
	{
		if (!TestNotNull(TEXT("Identity compatibility fixture spawned"), Fixture))
		{
			for (AActor* Cleanup : Fixtures)
			{
				if (Cleanup) Cleanup->Destroy();
			}
			return false;
		}
	}

	TestTrue(TEXT("Generic component lookup finds Player identity"),
		UAuraCombatIdentityComponent::FindForActor(PlayerOne) == PlayerOneComponent);
	TestTrue(TEXT("Player versus Enemy is allowed"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, EnemyOne));
	TestTrue(TEXT("Enemy versus Player is allowed"), UAuraAbilitySystemLibrary::IsNotFriend(EnemyOne, PlayerOne));
	TestFalse(TEXT("Player versus Player is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, PlayerTwo));
	TestFalse(TEXT("Enemy versus Enemy is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(EnemyOne, EnemyTwo));
	TestFalse(TEXT("Self targeting is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, PlayerOne));
	TestFalse(TEXT("Player versus Civilian is deferred/rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, Civilian));
	TestFalse(TEXT("Civilian versus Enemy is deferred/rejected"), UAuraAbilitySystemLibrary::IsNotFriend(Civilian, EnemyOne));
	TestFalse(TEXT("Missing component is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, Missing));
	TestFalse(TEXT("Invalid identity is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, Invalid));
	TestFalse(TEXT("Null first actor is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(nullptr, EnemyOne));
	TestFalse(TEXT("Null second actor is rejected"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerOne, nullptr));

	for (AActor* Fixture : Fixtures)
	{
		Fixture->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCivilianIdentityTest,
	"Aura.RoleBattle.Day2.CivilianIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCivilianIdentityTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for Civilian identity fixture"), World))
	{
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Expected = MakeIdentity(
		Tags.Faction_Civilian,
		Tags.Control_CivilianAI,
		Tags.Combat_Civilian,
		Tags.Death_PopulationRespawn,
		false);
	UAuraCombatIdentityComponent* Component = nullptr;
	AActor* Civilian = SpawnIdentityFixture(World, &Expected, Component);
	if (!TestNotNull(TEXT("Civilian fixture spawned"), Civilian)
		|| !TestNotNull(TEXT("Civilian identity component created"), Component))
	{
		if (Civilian) Civilian->Destroy();
		return false;
	}

	TestTrue(TEXT("Civilian identity is valid"), Component->HasValidIdentity());
	TestTrue(TEXT("Civilian identity round-trips through component"), Component->GetIdentity() == Expected);
	TestTrue(TEXT("Civilian is targetable"), Component->GetIdentity().bTargetable);
	TestFalse(TEXT("Civilian cannot attack"), Component->GetIdentity().bCanAttack);
	TestTrue(TEXT("Civilian can be damaged"), Component->GetIdentity().bCanBeDamaged);
	TestFalse(TEXT("Civilian friendly fire is disabled"), Component->GetIdentity().bAllowFriendlyFire);
	TestTrue(TEXT("Generic actor lookup returns Civilian component"),
		UAuraCombatIdentityComponent::FindForActor(Civilian) == Component);

	Civilian->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3CombatStateTransitionsTest,
	"Aura.RoleBattle.Day3.CombatStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3CombatStateTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for combat-state fixture"), World))
	{
		return false;
	}

	AActor* Actor = World->SpawnActor<AActor>();
	UAuraCombatStateComponent* State = NewObject<UAuraCombatStateComponent>(Actor, NAME_None, RF_Transient);
	Actor->AddInstanceComponent(State);
	State->RegisterComponent();

	TestEqual(TEXT("Initial state is Respawning"), State->GetLifeState(), EAuraCombatLifeState::Respawning);
	TestTrue(TEXT("Respawning transitions to Alive"), State->TryEnterAlive());
	TestFalse(TEXT("Repeated Alive transition is rejected"), State->TryEnterAlive());
	TestTrue(TEXT("Alive transitions to Dying"), State->TryEnterDying());
	TestFalse(TEXT("Repeated Dying transition is idempotently rejected"), State->TryEnterDying());
	TestTrue(TEXT("Dying transitions to Dead"), State->TryEnterDead());
	TestFalse(TEXT("Repeated Dead transition is rejected"), State->TryEnterDead());
	TestTrue(TEXT("Dead transitions to Respawning"), State->TryEnterRespawning());
	TestTrue(TEXT("Replacement lifecycle returns to Alive"), State->TryEnterAlive());
	TestEqual(TEXT("Final state is Alive"), State->GetLifeState(), EAuraCombatLifeState::Alive);

	Actor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3ReplacementPawnLifecycleTest,
	"Aura.RoleBattle.Day3.ReplacementPawnRespawnLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3ReplacementPawnLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for replacement lifecycle fixture"), World))
	{
		return false;
	}

	AActor* OldPawn = World->SpawnActor<AActor>();
	UAuraCombatStateComponent* OldState = NewObject<UAuraCombatStateComponent>(OldPawn, NAME_None, RF_Transient);
	OldPawn->AddInstanceComponent(OldState);
	OldState->RegisterComponent();
	OldState->TryEnterAlive();
	OldState->TryEnterDying();
	OldState->TryEnterDead();

	AActor* ReplacementPawn = World->SpawnActor<AActor>();
	UAuraCombatStateComponent* ReplacementState = NewObject<UAuraCombatStateComponent>(ReplacementPawn, NAME_None, RF_Transient);
	ReplacementPawn->AddInstanceComponent(ReplacementState);
	ReplacementState->RegisterComponent();

	TestEqual(TEXT("Old pawn remains Dead"), OldState->GetLifeState(), EAuraCombatLifeState::Dead);
	TestEqual(TEXT("Replacement begins Respawning"), ReplacementState->GetLifeState(), EAuraCombatLifeState::Respawning);
	TestTrue(TEXT("Replacement becomes Alive only after initialization"), ReplacementState->TryEnterAlive());
	TestEqual(TEXT("Old pawn never becomes Alive"), OldState->GetLifeState(), EAuraCombatLifeState::Dead);

	OldPawn->Destroy();
	ReplacementPawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3RelationshipMatrixTest,
	"Aura.RoleBattle.Day3.RelationshipMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3RelationshipMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for relationship fixtures"), World))
	{
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	FAuraCombatIdentity PlayerFriendlyFire = Player;
	PlayerFriendlyFire.bAllowFriendlyFire = true;
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	const FAuraCombatIdentity Civilian = MakeIdentity(Tags.Faction_Civilian, Tags.Control_CivilianAI, Tags.Combat_Civilian, Tags.Death_PopulationRespawn, false);

	UAuraCombatIdentityComponent* PlayerComponent = nullptr;
	UAuraCombatIdentityComponent* PlayerTwoComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyTwoComponent = nullptr;
	UAuraCombatIdentityComponent* CivilianComponent = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, &Player, PlayerComponent);
	AActor* PlayerTwoActor = SpawnIdentityFixture(World, &PlayerFriendlyFire, PlayerTwoComponent);
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	AActor* EnemyTwoActor = SpawnIdentityFixture(World, &Enemy, EnemyTwoComponent);
	AActor* CivilianActor = SpawnIdentityFixture(World, &Civilian, CivilianComponent);

	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = PlayerActor;

	const FAuraCombatRuleResult PlayerEnemy = FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context);
	const FAuraCombatRuleResult EnemyPlayer = FAuraCombatRules::CanDamage(EnemyActor, PlayerActor, Context);
	const FAuraCombatRuleResult PlayerPlayer = FAuraCombatRules::CanDamage(PlayerActor, PlayerTwoActor, Context);
	const FAuraCombatRuleResult PlayerCivilian = FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, Context);
	const FAuraCombatRuleResult EnemyEnemy = FAuraCombatRules::GetRelationship(EnemyActor, EnemyTwoActor, Context);
	const FAuraCombatRuleResult EnemyCivilian = FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context);
	const FAuraCombatRuleResult CivilianPlayer = FAuraCombatRules::CanDamage(CivilianActor, PlayerActor, Context);
	const FAuraCombatRuleResult CivilianEnemy = FAuraCombatRules::CanDamage(CivilianActor, EnemyActor, Context);

	TestTrue(TEXT("Player can damage Enemy"), PlayerEnemy.bCanDamage);
	TestEqual(TEXT("Player to Enemy is Hostile"), PlayerEnemy.Relationship, EAuraCombatRelationship::Hostile);
	TestTrue(TEXT("Enemy can damage Player"), EnemyPlayer.bCanDamage);
	TestEqual(TEXT("Enemy to Player is Hostile"), EnemyPlayer.Relationship, EAuraCombatRelationship::Hostile);
	TestFalse(TEXT("Player friendly fire is denied by default"), PlayerPlayer.bCanDamage);
	TestEqual(TEXT("Player to Player is Friendly"), PlayerPlayer.Relationship, EAuraCombatRelationship::Friendly);
	TestFalse(TEXT("Player to Civilian is denied by default"), PlayerCivilian.bCanDamage);
	TestEqual(TEXT("Player to Civilian is Protected"), PlayerCivilian.Relationship, EAuraCombatRelationship::Protected);
	TestEqual(TEXT("Enemy to Enemy is Friendly"), EnemyEnemy.Relationship, EAuraCombatRelationship::Friendly);
	TestFalse(TEXT("Enemy to Civilian is denied by default"), EnemyCivilian.bCanDamage);
	TestEqual(TEXT("Enemy to Civilian is Protected"), EnemyCivilian.Relationship, EAuraCombatRelationship::Protected);
	TestFalse(TEXT("Civilian cannot damage Player"), CivilianPlayer.bCanDamage);
	TestEqual(TEXT("Civilian to Player is Neutral"), CivilianPlayer.Relationship, EAuraCombatRelationship::Neutral);
	TestFalse(TEXT("Civilian cannot damage Enemy"), CivilianEnemy.bCanDamage);
	TestEqual(TEXT("Civilian to Enemy is Neutral"), CivilianEnemy.Relationship, EAuraCombatRelationship::Neutral);
	TestEqual(TEXT("Civilian denial reason is SourceCannotAttack"), CivilianEnemy.RejectionReason, EAuraCombatRuleRejectionReason::SourceCannotAttack);

	Context.SetPolicySnapshot(FAuraCombatRules::MakeTrustedTestPolicySnapshot(World, true, false, false, false));
	TestTrue(TEXT("Trusted PvP policy and source flag permit friendly fire"), FAuraCombatRules::CanDamage(PlayerTwoActor, PlayerActor, Context).bCanDamage);

	TestFalse(TEXT("Null source is denied"), FAuraCombatRules::CanDamage(nullptr, EnemyActor, Context).bCanDamage);
	TestFalse(TEXT("Null target is denied"), FAuraCombatRules::CanDamage(PlayerActor, nullptr, Context).bCanDamage);
	TestEqual(TEXT("Self is denied without explicit intent"),
		FAuraCombatRules::CanDamage(PlayerActor, PlayerActor, Context).RejectionReason,
		EAuraCombatRuleRejectionReason::SelfDenied);

	UAuraCombatIdentityComponent* InvalidIdentityComponent = nullptr;
	AActor* InvalidIdentityActor = SpawnIdentityFixture(World, nullptr, InvalidIdentityComponent);
	AActor* MissingIdentityActor = World->SpawnActor<AActor>();
	TestEqual(TEXT("Missing identity source is denied"),
		FAuraCombatRules::CanDamage(MissingIdentityActor, EnemyActor, Context).RejectionReason,
		EAuraCombatRuleRejectionReason::InvalidSource);
	TestEqual(TEXT("Invalid identity target is denied"),
		FAuraCombatRules::CanDamage(PlayerActor, InvalidIdentityActor, Context).RejectionReason,
		EAuraCombatRuleRejectionReason::InvalidTarget);
	TestEqual(TEXT("Missing identity target is denied"),
		FAuraCombatRules::CanDamage(PlayerActor, MissingIdentityActor, Context).RejectionReason,
		EAuraCombatRuleRejectionReason::InvalidTarget);

	for (AActor* Actor : {PlayerActor, PlayerTwoActor, EnemyActor, EnemyTwoActor, CivilianActor, InvalidIdentityActor, MissingIdentityActor})
	{
		if (Actor) Actor->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3ConservativeCivilianPolicyTest,
	"Aura.RoleBattle.Day3.ConservativeCivilianPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3ConservativeCivilianPolicyTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for civilian policy fixtures"), World)) return false;

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	const FAuraCombatIdentity Civilian = MakeIdentity(Tags.Faction_Civilian, Tags.Control_CivilianAI, Tags.Combat_Civilian, Tags.Death_PopulationRespawn, false);
	UAuraCombatIdentityComponent* PlayerComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	UAuraCombatIdentityComponent* CivilianComponent = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, &Player, PlayerComponent);
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	AActor* CivilianActor = SpawnIdentityFixture(World, &Civilian, CivilianComponent);

	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = PlayerActor;
	TestFalse(TEXT("Absent policy denies Player to Civilian"), FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, Context).bCanDamage);
	TestFalse(TEXT("Absent policy denies Enemy to Civilian"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);

	Context.SetPolicySnapshot(FAuraCombatRules::MakeTrustedTestPolicySnapshot(World, false, true, true, false, FName(TEXT("TestZone")), FName(TEXT("TestEvent"))));
	TestTrue(TEXT("Trusted authority policy permits Player to Civilian"), FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, Context).bCanDamage);
	TestTrue(TEXT("Trusted authority policy permits Enemy to Civilian"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);

	FAuraCombatRuleContext InvalidContext;
	InvalidContext.TrustedWorldContext = PlayerActor;
	FAuraCombatPolicySnapshot UntrustedSnapshot;
	InvalidContext.SetPolicySnapshot(UntrustedSnapshot);
	TestFalse(TEXT("Invalid permissive snapshot remains denied"), FAuraCombatRules::CanDamage(PlayerActor, CivilianActor, InvalidContext).bCanDamage);

	for (AActor* Actor : {PlayerActor, EnemyActor, CivilianActor})
	{
		if (Actor) Actor->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3ThreatDirectionTest,
	"Aura.RoleBattle.Day3.ThreatDirectionAndDefaultPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3ThreatDirectionTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for threat fixtures"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	const FAuraCombatIdentity Civilian = MakeIdentity(Tags.Faction_Civilian, Tags.Control_CivilianAI, Tags.Combat_Civilian, Tags.Death_PopulationRespawn, false);
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	UAuraCombatIdentityComponent* CivilianComponent = nullptr;
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	AActor* CivilianActor = SpawnIdentityFixture(World, &Civilian, CivilianComponent);
	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = EnemyActor;
	TestFalse(TEXT("Default policy rejects Enemy threat against Civilian"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);
	Context.SetPolicySnapshot(FAuraCombatRules::MakeTrustedTestPolicySnapshot(World, false, false, true, false));
	TestTrue(TEXT("Trusted policy accepts candidate-to-observer Enemy threat direction"), FAuraCombatRules::CanDamage(EnemyActor, CivilianActor, Context).bCanDamage);
	TestFalse(TEXT("Reverse Civilian-to-Enemy direction remains denied"), FAuraCombatRules::CanDamage(CivilianActor, EnemyActor, Context).bCanDamage);
	EnemyActor->Destroy();
	CivilianActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3IdentityFlagsLifeStateTest,
	"Aura.RoleBattle.Day3.IdentityFlagsAndLifeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3IdentityFlagsLifeStateTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for flag/state fixtures"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	UAuraCombatIdentityComponent* PlayerComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, &Player, PlayerComponent);
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = PlayerActor;

	Player.bCanAttack = false;
	PlayerComponent->InitializeIdentity(Player);
	TestEqual(TEXT("Source cannot attack reason"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::SourceCannotAttack);
	Player.bCanAttack = true;
	Player.bTargetable = false;
	PlayerComponent->InitializeIdentity(Player);
	TestEqual(TEXT("Untargetable target reason"), FAuraCombatRules::CanCombatTarget(EnemyActor, PlayerActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::TargetNotTargetable);
	Player.bTargetable = true;
	Player.bCanBeDamaged = false;
	PlayerComponent->InitializeIdentity(Player);
	TestEqual(TEXT("Undamageable target reason"), FAuraCombatRules::CanDamage(EnemyActor, PlayerActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::TargetNotDamageable);

	Player.bCanBeDamaged = true;
	PlayerComponent->InitializeIdentity(Player);
	UAuraCombatStateComponent* PlayerState = UAuraCombatStateComponent::FindForActor(PlayerActor);
	UAuraCombatStateComponent* EnemyState = UAuraCombatStateComponent::FindForActor(EnemyActor);
	TestTrue(TEXT("Alive target can receive damage"), FAuraCombatRules::CanReceiveDamage(EnemyActor, Context).bCanDamage);
	EnemyState->TryEnterDying();
	TestEqual(TEXT("Dying target is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Dying);
	TestEqual(TEXT("Dying target cannot receive damage"), FAuraCombatRules::CanReceiveDamage(EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Dying);
	EnemyState->TryEnterDead();
	TestEqual(TEXT("Dead target is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Dead);
	EnemyState->TryEnterRespawning();
	TestEqual(TEXT("Respawning target is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Respawning);
	EnemyState->TryEnterAlive();

	PlayerState->TryEnterDying();
	TestEqual(TEXT("Dying source is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Dying);
	PlayerState->TryEnterDead();
	TestEqual(TEXT("Dead source is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Dead);
	PlayerState->TryEnterRespawning();
	TestEqual(TEXT("Respawning source is rejected"), FAuraCombatRules::CanDamage(PlayerActor, EnemyActor, Context).RejectionReason, EAuraCombatRuleRejectionReason::Respawning);
	PlayerState->TryEnterAlive();

	PlayerActor->Destroy();
	EnemyActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3IsNotFriendCompatibilityTest,
	"Aura.RoleBattle.Day3.IsNotFriendCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3IsNotFriendCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for compatibility fixtures"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	UAuraCombatIdentityComponent* PlayerComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, &Player, PlayerComponent);
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	TestTrue(TEXT("Compatibility wrapper accepts hostile Player to Enemy"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerActor, EnemyActor));
	TestTrue(TEXT("Compatibility wrapper accepts hostile Enemy to Player"), UAuraAbilitySystemLibrary::IsNotFriend(EnemyActor, PlayerActor));
	TestFalse(TEXT("Compatibility wrapper rejects self"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerActor, PlayerActor));
	TestFalse(TEXT("Compatibility wrapper rejects null"), UAuraAbilitySystemLibrary::IsNotFriend(PlayerActor, nullptr));
	PlayerActor->Destroy();
	EnemyActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay3EnemyPlayerTargetingTest,
	"Aura.RoleBattle.Day3.EnemyPlayerTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay3EnemyPlayerTargetingTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	UWorld* World = FindAutomationWorld();
	if (!TestNotNull(TEXT("World available for enemy targeting fixtures"), World)) return false;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FAuraCombatIdentity Player = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
	const FAuraCombatIdentity Enemy = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
	UAuraCombatIdentityComponent* PlayerComponent = nullptr;
	UAuraCombatIdentityComponent* EnemyComponent = nullptr;
	AActor* PlayerActor = SpawnIdentityFixture(World, &Player, PlayerComponent);
	AActor* EnemyActor = SpawnIdentityFixture(World, &Enemy, EnemyComponent);
	FAuraCombatRuleContext Context;
	Context.TrustedWorldContext = EnemyActor;
	TestTrue(TEXT("Enemy can combat-target Player"), FAuraCombatRules::CanCombatTarget(EnemyActor, PlayerActor, Context).bCanCombatTarget);
	TestTrue(TEXT("Player can combat-target Enemy"), FAuraCombatRules::CanCombatTarget(PlayerActor, EnemyActor, Context).bCanCombatTarget);
	EnemyActor->Destroy();
	PlayerActor->Destroy();
	return true;
}

namespace AuraRoleBattleTestsPrivate
{
	/** One row of the Day 4 damage-producer inventory. */
	struct FAuraDamageProducerEntry
	{
		const TCHAR* ProducerId;
		const TCHAR* SourcePath;
		const TCHAR* Category;
		bool bProduction;
		const TCHAR* ExpectedBoundary;
		const TCHAR* ExpectedAuthority;
	};

	/**
	 * Canonical Day 4 producer table. Mirrors the "Producer table" in
	 * Docs/Reports/Role-Battle-Damage-Producer-Inventory.md; the producer test
	 * fails when the two differ.
	 */
	const FAuraDamageProducerEntry DamageProducerTable[] = {
		// Native producers (Source/Aura)
		{ TEXT("Native.Projectile.AuraProjectile"), TEXT("Source/Aura/Private/Actor/AuraProjectile.cpp"), TEXT("Projectile"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority at impact") },
		{ TEXT("Native.Projectile.AuraFireBall"), TEXT("Source/Aura/Private/Actor/AuraFireBall.cpp"), TEXT("Projectile"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority at overlap") },
		{ TEXT("Native.Projectile.AuraProjectileSpell"), TEXT("Source/Aura/Private/AbilitySystem/Abilities/AuraProjectileSpell.cpp"), TEXT("Projectile (param builder)"), true, TEXT("ApplyDamageEffect via projectile impact"), TEXT("HasAuthority at spawn") },
		{ TEXT("Native.Projectile.AuraFireBolt"), TEXT("Source/Aura/Private/AbilitySystem/Abilities/AuraFireBolt.cpp"), TEXT("Projectile (param builder)"), true, TEXT("ApplyDamageEffect via projectile impact"), TEXT("HasAuthority at spawn") },
		{ TEXT("Native.Projectile.AuraFireBlast"), TEXT("Source/Aura/Private/AbilitySystem/Abilities/AuraFireBlast.cpp"), TEXT("Projectile (param builder)"), true, TEXT("ApplyDamageEffect via projectile impact"), TEXT("HasAuthority at spawn") },
		{ TEXT("Native.Direct.CauseDamage"), TEXT("Source/Aura/Private/AbilitySystem/Abilities/AuraDamageGameplayAbility.cpp"), TEXT("Direct"), true, TEXT("ApplyDamageEffect"), TEXT("Server ability activation") },
		{ TEXT("Native.Periodic.Debuff"), TEXT("Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp"), TEXT("Periodic"), true, TEXT("Final AttributeSet revalidation"), TEXT("Server attribute execution") },
		{ TEXT("Smoke.Day1.AuraPlayerController"), TEXT("Source/Aura/Private/Player/AuraPlayerController.cpp"), TEXT("Smoke"), false, TEXT("ApplyDamageEffect"), TEXT("HasAuthority (test-only)") },
		// AuraAbilityGraph producers (Plugins/AuraAbilityGraph)
		{ TEXT("Graph.ApplyDamage"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp"), TEXT("Direct"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.CauseDamage"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp"), TEXT("Direct"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.ElectrocuteBeam"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp"), TEXT("Beam"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.EnemyMeleeDamage"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp"), TEXT("Melee"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.HitscanTrace"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp"), TEXT("Hitscan"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.ApplyBeamDamage"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp"), TEXT("Beam"), true, TEXT("ApplyDamageEffect"), TEXT("bAuthorityOnly XML (must be enforced)") },
		{ TEXT("Graph.SpawnProjectile"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp"), TEXT("Projectile"), true, TEXT("ApplyDamageEffect via projectile impact"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.SpawnProjectiles"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp"), TEXT("Projectile"), true, TEXT("ApplyDamageEffect via projectile impact"), TEXT("HasAuthority gate") },
		{ TEXT("Graph.SpawnShards"), TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp"), TEXT("Radial"), true, TEXT("ApplyDamageEffect"), TEXT("HasAuthority gate") },
	};
	const int32 DamageProducerTableCount = UE_ARRAY_COUNT(DamageProducerTable);

	/**
	 * Files that legitimately contain a damage-application symbol but are not
	 * damage producers: they define the shared machinery, apply non-damage
	 * gameplay effects, or are test fixtures. Kept in sync with the inventory
	 * report's "Non-producer infrastructure" section.
	 */
	const TCHAR* DamageSymbolNonProducerFiles[] = {
		// Shared damage machinery (defines the symbols)
		TEXT("Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp"),
		TEXT("Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h"),
		TEXT("Source/Aura/Public/AuraAbilityTypes.h"),
		TEXT("Source/Aura/Private/AuraAbilityTypes.cpp"),
		TEXT("Source/Aura/Public/AbilitySystem/AuraAttributeSet.h"),
		TEXT("Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp"),
		TEXT("Source/Aura/Public/AbilitySystem/Abilities/AuraDamageGameplayAbility.h"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AbilityDefinition.h"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AbilityDefinition.cpp"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AuraAbilityGraphModule.cpp"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AuraDamageGameplayEffect.h"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/Nodes/Actions/SpawnShardsNode.h"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/Nodes/Actions/CauseDamageNode.h"),
		// Non-damage gameplay-effect application
		TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp"),
		TEXT("Source/Aura/Private/Actor/AuraEffectActor.cpp"),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/DataAbility.cpp"),
		// Test fixtures
		TEXT("Source/Aura/Private/Tests/AuraRoleBattleTests.cpp"),
		TEXT("Source/Aura/Private/Tests/AuraPickupGameplayEffectTests.cpp"),
	};
	const int32 DamageSymbolNonProducerFileCount = UE_ARRAY_COUNT(DamageSymbolNonProducerFiles);

	bool IsDamageProducerId(const FString& Cell)
	{
		return Cell.StartsWith(TEXT("Native.")) || Cell.StartsWith(TEXT("Graph.")) || Cell.StartsWith(TEXT("Smoke."));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4ProducerInventoryTest,
	"Aura.RoleBattle.Day4.ProducerInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4ProducerInventoryTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;

	// 1. Every production producer declares a shared boundary and expected authority.
	for (int32 i = 0; i < DamageProducerTableCount; ++i)
	{
		const FAuraDamageProducerEntry& Entry = DamageProducerTable[i];
		if (Entry.bProduction)
		{
			TestTrue(FString::Printf(TEXT("Production producer %s declares a shared boundary"), Entry.ProducerId), FCString::Strlen(Entry.ExpectedBoundary) > 0);
			TestTrue(FString::Printf(TEXT("Production producer %s declares expected authority"), Entry.ProducerId), FCString::Strlen(Entry.ExpectedAuthority) > 0);
		}
	}

	// 2. The inventory report exists and matches the table in both directions.
	const FString ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Docs/Reports/Role-Battle-Damage-Producer-Inventory.md"));
	FString ReportText;
	if (!TestTrue(TEXT("Producer inventory report exists"), FFileHelper::LoadFileToString(ReportText, *ReportPath)))
	{
		return false;
	}

	// 2a. Every table entry is present in the report.
	for (int32 i = 0; i < DamageProducerTableCount; ++i)
	{
		const FAuraDamageProducerEntry& Entry = DamageProducerTable[i];
		TestTrue(FString::Printf(TEXT("Report covers producer %s"), Entry.ProducerId), ReportText.Contains(Entry.ProducerId));
		TestTrue(FString::Printf(TEXT("Report covers producer path %s"), Entry.SourcePath), ReportText.Contains(Entry.SourcePath));
	}

	// 2b. Every producer row in the report's table is present in the compile-time table
	//     with a matching production/test-only classification.
	TArray<FString> ReportLines;
	ReportText.ParseIntoArrayLines(ReportLines);
	for (const FString& Line : ReportLines)
	{
		if (!Line.StartsWith(TEXT("| ")))
		{
			continue;
		}
		TArray<FString> Cells;
		Line.ParseIntoArray(Cells, TEXT("|"), true);
		if (Cells.Num() < 4)
		{
			continue;
		}
		const FString Cell = Cells[0].TrimStartAndEnd();
		if (!IsDamageProducerId(Cell))
		{
			continue;
		}

		const FAuraDamageProducerEntry* Match = nullptr;
		for (int32 i = 0; i < DamageProducerTableCount; ++i)
		{
			if (Cell == DamageProducerTable[i].ProducerId)
			{
				Match = &DamageProducerTable[i];
				break;
			}
		}
		TestTrue(FString::Printf(TEXT("Report producer %s is present in the compile-time table"), *Cell), Match != nullptr);
		if (Match)
		{
			const FString ProdCell = Cells[3].TrimStartAndEnd();
			const bool bReportProduction = (ProdCell == TEXT("Production"));
			TestTrue(FString::Printf(TEXT("Report producer %s production flag matches the table"), *Cell), bReportProduction == Match->bProduction);
		}
	}

	// 3. Every discovered damage-application site is either a known producer or a
	//    documented non-producer. A new application site that is neither fails.
	const TCHAR* DamageSymbols[] = {
		TEXT("ApplyDamageEffect"),
		TEXT("CauseDamage"),
		TEXT("ApplyGameplayEffectSpecToTarget"),
		TEXT("ApplyGameplayEffectSpecToSelf"),
	};

	TArray<FString> ScanRoots;
	ScanRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/Aura")));
	ScanRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Plugins/AuraAbilityGraph/Source")));

	for (const FString& Root : ScanRoots)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.h"), true, false, false);
		for (const FString& File : Files)
		{
			FString Content;
			if (!FFileHelper::LoadFileToString(Content, *File))
			{
				continue;
			}
			bool bHasSymbol = false;
			for (const TCHAR* Symbol : DamageSymbols)
			{
				if (Content.Contains(Symbol))
				{
					bHasSymbol = true;
					break;
				}
			}
			if (!bHasSymbol)
			{
				continue;
			}

			FString RepoRelative = File;
			FPaths::MakePathRelativeTo(RepoRelative, *FPaths::ProjectDir());
			FPaths::NormalizeFilename(RepoRelative);

			bool bKnown = false;
			for (int32 i = 0; i < DamageProducerTableCount; ++i)
			{
				if (RepoRelative == DamageProducerTable[i].SourcePath)
				{
					bKnown = true;
					break;
				}
			}
			if (!bKnown)
			{
				for (int32 i = 0; i < DamageSymbolNonProducerFileCount; ++i)
				{
					if (RepoRelative == DamageSymbolNonProducerFiles[i])
					{
						bKnown = true;
						break;
					}
				}
			}
			TestTrue(FString::Printf(TEXT("Discovered damage-application site %s is inventoried"), *RepoRelative), bKnown);
		}
	}

	return true;
}

namespace AuraRoleBattleTestsPrivate
{
	struct FDay4DamageFixture
	{
		AActor* SourceActor = nullptr;
		AActor* TargetActor = nullptr;
		UAuraAbilitySystemComponent* SourceASC = nullptr;
		UAuraAbilitySystemComponent* TargetASC = nullptr;
		UAuraAttributeSet* SourceAttributes = nullptr;
		UAuraAttributeSet* TargetAttributes = nullptr;
	};

	UAuraAbilitySystemComponent* AddDamageAbilitySystem(AActor* Owner, UAuraAttributeSet*& OutAttributes)
	{
		OutAttributes = nullptr;
		if (!Owner)
		{
			return nullptr;
		}

		UAuraAbilitySystemComponent* ASC = NewObject<UAuraAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		OutAttributes = NewObject<UAuraAttributeSet>(ASC, NAME_None, RF_Transient);
		ASC->AddAttributeSetSubobject(OutAttributes);
		ASC->InitAbilityActorInfo(Owner, Owner);
		ASC->SetNumericAttributeBase(UAuraAttributeSet::GetMaxHealthAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), 100.f);
		return ASC;
	}

	bool MakeDay4DamageFixture(UWorld* World, FDay4DamageFixture& OutFixture)
	{
		if (!World)
		{
			return false;
		}

		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		const FAuraCombatIdentity SourceIdentity = MakeIdentity(Tags.Faction_Player, Tags.Control_Player, Tags.Combat_Unassigned, Tags.Death_PlayerRespawn);
		const FAuraCombatIdentity TargetIdentity = MakeIdentity(Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Combat_Unassigned, Tags.Death_EnemyLoot);
		UAuraCombatIdentityComponent* SourceIdentityComponent = nullptr;
		UAuraCombatIdentityComponent* TargetIdentityComponent = nullptr;
		OutFixture.SourceActor = SpawnIdentityFixture(World, &SourceIdentity, SourceIdentityComponent);
		OutFixture.TargetActor = SpawnIdentityFixture(World, &TargetIdentity, TargetIdentityComponent);
		OutFixture.SourceASC = AddDamageAbilitySystem(OutFixture.SourceActor, OutFixture.SourceAttributes);
		OutFixture.TargetASC = AddDamageAbilitySystem(OutFixture.TargetActor, OutFixture.TargetAttributes);
		return OutFixture.SourceActor && OutFixture.TargetActor && OutFixture.SourceASC && OutFixture.TargetASC;
	}

	FDamageEffectParams MakeDay4DamageParams(const FDay4DamageFixture& Fixture)
	{
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		FDamageEffectParams Params;
		Params.WorldContextObject = Fixture.SourceActor;
		Params.SourceAbilitySystemComponent = Fixture.SourceASC;
		Params.TargetAbilitySystemComponent = Fixture.TargetASC;
		Params.BaseDamage = 10.f;
		Params.AbilityLevel = 1.f;
		Params.DamageType = Tags.Damage_Physical;
		Params.AbilityTag = Tags.Abilities_Attack;
		Params.CombatRuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
		Params.CombatRuleContext.TrustedWorldContext = Fixture.SourceActor;
		Params.CombatRuleContext.SourceActor = Fixture.SourceActor;
		Params.CombatRuleContext.TargetActor = Fixture.TargetActor;
		Params.CombatRuleContext.ImpactLocation = Fixture.TargetActor ? Fixture.TargetActor->GetActorLocation() : FVector::ZeroVector;
		return Params;
	}

	void DestroyDay4DamageFixture(FDay4DamageFixture& Fixture)
	{
		if (Fixture.SourceActor)
		{
			Fixture.SourceActor->Destroy();
		}
		if (Fixture.TargetActor)
		{
			Fixture.TargetActor->Destroy();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4EffectContextNetSerializeTest,
	"Aura.RoleBattle.Day4.EffectContextNetSerialize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4EffectContextNetSerializeTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FAuraGameplayEffectContext Source;
	Source.SetIsBlockedHit(true);
	Source.SetIsCriticalHit(true);
	Source.SetIsSuccessfulDebuff(true);
	Source.SetDebuffDamage(7.5f);
	Source.SetDebuffDuration(4.5f);
	Source.SetDebuffFrequency(0.5f);
	Source.SetDamageType(TSharedPtr<FGameplayTag>(new FGameplayTag(Tags.Damage_Physical)));
	Source.SetAbilityTag(Tags.Abilities_Attack);
	Source.SetSourceRoleId(FName(TEXT("BungeeMan")));
	Source.SetBattleZoneId(FName(TEXT("Day4Zone")));
	Source.SetBattleEventId(FName(TEXT("Day4Event")));
	Source.SetDeathImpulse(FVector(1.f, 2.f, 3.f));
	Source.SetKnockbackForce(FVector(4.f, 5.f, 6.f));
	Source.SetIsRadialDamage(true);
	Source.SetRadialDamageInnerRadius(100.f);
	Source.SetRadialDamageOuterRadius(500.f);
	Source.SetRadialDamageOrigin(FVector(7.f, 8.f, 9.f));

	FNetBitWriter Writer(nullptr, 4096);
	bool bSaveSuccess = false;
	Source.NetSerialize(Writer, nullptr, bSaveSuccess);
	TestTrue(TEXT("Effect context save succeeds"), bSaveSuccess);

	FAuraGameplayEffectContext RoundTrip;
	FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
	bool bLoadSuccess = false;
	RoundTrip.NetSerialize(Reader, nullptr, bLoadSuccess);
	TestTrue(TEXT("Effect context load succeeds"), bLoadSuccess);
	TestTrue(TEXT("Ability tag round-trips"), RoundTrip.GetAbilityTag().MatchesTagExact(Tags.Abilities_Attack));
	TestEqual(TEXT("Source role round-trips"), RoundTrip.GetSourceRoleId(), FName(TEXT("BungeeMan")));
	TestEqual(TEXT("Battle zone round-trips"), RoundTrip.GetBattleZoneId(), FName(TEXT("Day4Zone")));
	TestEqual(TEXT("Battle event round-trips"), RoundTrip.GetBattleEventId(), FName(TEXT("Day4Event")));
	TestEqual(TEXT("Damage type round-trips"), RoundTrip.GetDamageType().IsValid() ? *RoundTrip.GetDamageType() : FGameplayTag(), Tags.Damage_Physical);
	TestTrue(TEXT("Damage flags round-trip"), RoundTrip.IsBlockedHit() && RoundTrip.IsCriticalHit() && RoundTrip.IsSuccessfulDebuff());
	TestTrue(TEXT("Impulse data round-trips"), RoundTrip.GetDeathImpulse().Equals(FVector(1.f, 2.f, 3.f)) && RoundTrip.GetKnockbackForce().Equals(FVector(4.f, 5.f, 6.f)));
	TestTrue(TEXT("Radial data round-trips"), RoundTrip.IsRadialDamage() && FMath::IsNearlyEqual(RoundTrip.GetRadialDamageOuterRadius(), 500.f) && RoundTrip.GetRadialDamageOrigin().Equals(FVector(7.f, 8.f, 9.f)));

	FGameplayEffectContext* Duplicate = Source.Duplicate();
	TestNotNull(TEXT("Effect context duplicates"), Duplicate);
	if (Duplicate)
	{
		const FAuraGameplayEffectContext* AuraDuplicate = static_cast<const FAuraGameplayEffectContext*>(Duplicate);
		TestTrue(TEXT("Duplicate preserves ability attribution"), AuraDuplicate->GetAbilityTag().MatchesTagExact(Tags.Abilities_Attack));
		delete Duplicate;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4SharedDamageBoundaryTest,
	"Aura.RoleBattle.Day4.SharedDamageBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4SharedDamageBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!TestTrue(TEXT("Shared damage fixture created"), MakeDay4DamageFixture(FindAutomationWorld(), Fixture)))
	{
		return false;
	}

	const FGameplayEffectContextHandle Context = UAuraAbilitySystemLibrary::ApplyDamageEffect(MakeDay4DamageParams(Fixture));
	TestTrue(TEXT("Hostile authoritative damage crosses the shared boundary"), Context.IsValid());
	TestTrue(TEXT("Shared boundary preserves ability attribution"), UAuraAbilitySystemLibrary::GetAbilityTag(Context).MatchesTagExact(FAuraGameplayTags::Get().Abilities_Attack));
	TestTrue(TEXT("Shared boundary preserves empty source role field for a generic fixture"), UAuraAbilitySystemLibrary::GetSourceRoleId(Context).IsNone());
	DestroyDay4DamageFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4DirectCauseDamageBoundaryTest,
	"Aura.RoleBattle.Day4.DirectCauseDamageBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4DirectCauseDamageBoundaryTest::RunTest(const FString& Parameters)
{
	UAuraDamageGameplayAbility* Ability = NewObject<UAuraDamageGameplayAbility>(GetTransientPackage());
	TestNotNull(TEXT("Transient damage ability created"), Ability);
	if (Ability)
	{
		Ability->CauseDamage(nullptr);
		TestTrue(TEXT("Direct CauseDamage remains safe when no target is available"), true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4AuthorityRejectionTest,
	"Aura.RoleBattle.Day4.AuthorityRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4AuthorityRejectionTest::RunTest(const FString& Parameters)
{
	FDamageEffectParams MissingParams;
	TestFalse(TEXT("Missing ASCs are rejected before spec creation"), UAuraAbilitySystemLibrary::ApplyDamageEffect(MissingParams).IsValid());

	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		FDamageEffectParams InvalidTargetParams = MakeDay4DamageParams(Fixture);
		InvalidTargetParams.TargetAbilitySystemComponent = nullptr;
		TestFalse(TEXT("Missing target ASC is rejected"), UAuraAbilitySystemLibrary::ApplyDamageEffect(InvalidTargetParams).IsValid());
		DestroyDay4DamageFixture(Fixture);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4AllProducerAttributionTest,
	"Aura.RoleBattle.Day4.AllProducerAttribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4AllProducerAttributionTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		TestTrue(TEXT("Attribution fixture created"), false);
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	const FString XML = TEXT("<ability name=\"Day4Attribution\" abilityTag=\"Abilities.Attack\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Damage\"><damage type=\"Damage.Physical\" base=\"10\"/><graph><node class=\"Sequence\"/></graph></ability>");
	TestTrue(TEXT("Attribution definition parses"), Definition && Definition->LoadFromXML(XML));
	if (Definition)
	{
		FDamageEffectParams Params;
		Definition->BuildDamageEffectParams(Params, Fixture.SourceASC, Fixture.TargetASC, Fixture.SourceActor, 1.f, FVector::ForwardVector);
		TestTrue(TEXT("Definition carries stable ability tag"), Params.AbilityTag.MatchesTagExact(Definition->AbilityTag));
		TestTrue(TEXT("Definition carries source and target rule actors"), Params.CombatRuleContext.SourceActor == Fixture.SourceActor && Params.CombatRuleContext.TargetActor == Fixture.TargetActor);
	}
	DestroyDay4DamageFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4NonAliveSourceAndTargetRejectionTest,
	"Aura.RoleBattle.Day4.NonAliveSourceAndTargetRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4NonAliveSourceAndTargetRejectionTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		return false;
	}

	UAuraCombatStateComponent* SourceState = UAuraCombatStateComponent::FindForActor(Fixture.SourceActor);
	UAuraCombatStateComponent* TargetState = UAuraCombatStateComponent::FindForActor(Fixture.TargetActor);
	TestNotNull(TEXT("Source state exists"), SourceState);
	TestNotNull(TEXT("Target state exists"), TargetState);
	if (SourceState && TargetState)
	{
		FDamageEffectParams Params = MakeDay4DamageParams(Fixture);
		TestTrue(TEXT("Alive damage is accepted by the rule fixture"), FAuraCombatRules::CanDamage(Fixture.SourceActor, Fixture.TargetActor, Params.CombatRuleContext).bCanDamage);
		SourceState->TryEnterDying();
		TestFalse(TEXT("Dying source cannot damage"), UAuraAbilitySystemLibrary::ApplyDamageEffect(Params).IsValid());
		SourceState->TryEnterDead();
		SourceState->TryEnterRespawning();
		SourceState->TryEnterAlive();
		TargetState->TryEnterDying();
		TestFalse(TEXT("Dying target cannot receive damage"), UAuraAbilitySystemLibrary::ApplyDamageEffect(Params).IsValid());
	}
	DestroyDay4DamageFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4PeriodicAttributionAndRevalidationTest,
	"Aura.RoleBattle.Day4.PeriodicAttributionAndRevalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4PeriodicAttributionAndRevalidationTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		return false;
	}

	const FGameplayEffectContextHandle Original = UAuraAbilitySystemLibrary::ApplyDamageEffect(MakeDay4DamageParams(Fixture));
	TestTrue(TEXT("Original damage context exists for periodic attribution"), Original.IsValid());
	if (Original.IsValid())
	{
		FGameplayEffectContextHandle Copied(Original.Get()->Duplicate());
		TestTrue(TEXT("Periodic context duplicates the original"), Copied.IsValid());
		TestTrue(TEXT("Periodic context retains ability attribution"), UAuraAbilitySystemLibrary::GetAbilityTag(Copied).MatchesTagExact(FAuraGameplayTags::Get().Abilities_Attack));
		TestEqual(TEXT("Periodic context retains damage type"), UAuraAbilitySystemLibrary::GetDamageType(Copied), FAuraGameplayTags::Get().Damage_Physical);
	}
	if (UAuraCombatStateComponent* TargetState = UAuraCombatStateComponent::FindForActor(Fixture.TargetActor))
	{
		TargetState->TryEnterDying();
		FAuraCombatRuleContext Context;
		Context.QueryPurpose = EAuraCombatQueryPurpose::Damage;
		Context.TrustedWorldContext = Fixture.SourceActor;
		TestFalse(TEXT("Periodic revalidation rejects a target that becomes dying"), FAuraCombatRules::CanDamage(Fixture.SourceActor, Fixture.TargetActor, Context).bCanDamage);
	}
	DestroyDay4DamageFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4SafeDamageProfileTest,
	"Aura.RoleBattle.Day4.SafeDamageProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4SafeDamageProfileTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		return false;
	}

	// These bare actors have no CombatInterface, game mode, CharacterClassInfo, curve table,
	// or individual coefficient curves. The execution path must use neutral coefficients.
	const FGameplayEffectContextHandle Context = UAuraAbilitySystemLibrary::ApplyDamageEffect(MakeDay4DamageParams(Fixture));
	TestTrue(TEXT("Damage remains safe without character-class profile data"), Context.IsValid());
	DestroyDay4DamageFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDay4PreservedDamageSemanticsTest,
	"Aura.RoleBattle.Day4.PreservedDamageSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDay4PreservedDamageSemanticsTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleTestsPrivate;
	FDay4DamageFixture Fixture;
	if (!MakeDay4DamageFixture(FindAutomationWorld(), Fixture))
	{
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	const FString XML = TEXT("<ability name=\"Day4Semantics\" abilityTag=\"Abilities.Attack\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Damage\"><damage type=\"Damage.Physical\" base=\"25\" deathImpulseMagnitude=\"500\" knockbackForceMagnitude=\"750\"/><graph><node class=\"Sequence\"/></graph></ability>");
	if (Definition && Definition->LoadFromXML(XML))
	{
		FDamageEffectParams Params;
		Definition->BuildDamageEffectParams(Params, Fixture.SourceASC, Fixture.TargetASC, Fixture.SourceActor, 1.f, FVector::ForwardVector, true, FVector(10.f, 20.f, 30.f), 50.f, 250.f);
		TestTrue(TEXT("Radial damage flag is preserved"), Params.bIsRadialDamage);
		TestEqual(TEXT("Radial inner radius is preserved"), Params.RadialDamageInnerRadius, 50.f);
		TestEqual(TEXT("Radial outer radius is preserved"), Params.RadialDamageOuterRadius, 250.f);
		TestTrue(TEXT("Impulse semantics are direction aligned"), Params.DeathImpulse.Equals(FVector::ForwardVector * Definition->DeathImpulseMagnitude));
	}
	else
	{
		TestTrue(TEXT("Semantics definition parses"), false);
	}
	DestroyDay4DamageFixture(Fixture);
	return true;
}

namespace AuraRoleBattleDay5TestsPrivate
{
	FAuraRoleLoadResult LoadShipped()
	{
		return UAuraAbilitySystemLibrary::LoadRoleInfoCandidate(nullptr);
	}

	FString LegacyAuraJson(bool bExplicitVersion)
	{
		return FString::Printf(TEXT(R"JSON({%s"defaultRole":"Aura","roles":[{"role":"Aura","displayName":"Aura","mesh":"/Game/Assets/Characters/Aura/SKM_Aura","animBlueprint":"/Game/Blueprints/Character/Aura/ABP_Aura.ABP_Aura_C","weaponMesh":"","weaponSocket":"","weaponTipSocket":"","attributes":{"strength":10,"intelligence":15,"resilience":10,"vigor":10},"lmbAbility":"","lmbAbilityDefinition":"/Game/AbilityDefinitions/FireBolt.xml"}]})JSON"),
			bExplicitVersion ? TEXT("\"roleDefinitionVersion\":1,") : TEXT(""));
	}

	bool HasIssue(const FAuraRoleLoadResult& Result, const FString& Fragment)
	{
		return Result.Issues.ContainsByPredicate([&Fragment](const FAuraRoleValidationIssue& Issue)
		{
			return Issue.JsonPath.Contains(Fragment) || Issue.Message.Contains(Fragment);
		});
	}
}

#define AURA_DAY5_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.RoleBattle.Day5." TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AURA_DAY5_TEST(FAuraDay5ValidVersion2SchemaTest, "ValidVersion2Schema")
bool FAuraDay5ValidVersion2SchemaTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	AddInfo(Result.ToLogString());
	TestTrue(TEXT("Version 2 registry publishes"), Result.bCanPublish);
	TestEqual(TEXT("Authored version detected"), Result.DetectedVersion, 2);
	TestEqual(TEXT("Three roles published"), Result.Candidate ? Result.Candidate->RoleInformation.Num() : 0, 3);
	return true;
}

AURA_DAY5_TEST(FAuraDay5LegacyVersion1MigrationTest, "LegacyVersion1Migration")
bool FAuraDay5LegacyVersion1MigrationTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleDay5TestsPrivate;
	const FAuraRoleLoadResult Omitted = UAuraAbilitySystemLibrary::ParseRoleInfoJson(nullptr, LegacyAuraJson(false));
	const FAuraRoleLoadResult Explicit = UAuraAbilitySystemLibrary::ParseRoleInfoJson(nullptr, LegacyAuraJson(true));
	TestTrue(TEXT("Omitted version migrates"), Omitted.bCanPublish);
	TestTrue(TEXT("Explicit version 1 migrates"), Explicit.bCanPublish);
	TestEqual(TEXT("Migration retains detected version"), Omitted.DetectedVersion, 1);
	TestEqual(TEXT("Migration publishes version 2"), Omitted.PublishedVersion, 2);
	const FRoleDefaultInfo* Aura = Omitted.Candidate ? Omitted.Candidate->RoleInformation.Find(TEXT("Aura")) : nullptr;
	TestTrue(TEXT("Legacy Aura receives fixed Player identity"), Aura && Aura->EntityType.MatchesTagExact(FAuraGameplayTags::Get().Entity_Player));
	return true;
}

AURA_DAY5_TEST(FAuraDay5AggregateValidationErrorsTest, "AggregateValidationErrors")
bool FAuraDay5AggregateValidationErrorsTest::RunTest(const FString& Parameters)
{
	using namespace AuraRoleBattleDay5TestsPrivate;
	const FString Bad = TEXT(R"JSON({"roleDefinitionVersion":2,"defaultRole":"Broken","roles":[{"role":"Broken","displayName":7,"entityType":"Faction.Player","controlType":"Control.Player","combatProfile":"Combat.Magic","faction":"Faction.Player","deathPolicy":"Death.PlayerRespawn","economyProfile":"Economy.None","interactionProfile":"Interaction.Combatant","playerSelectable":true,"targetable":"yes","canAttack":true,"canBeDamaged":true,"allowFriendlyFire":false,"mesh":"/Game/Missing/Mesh","animBlueprint":"/Game/Missing/Anim_C","attributes":{"strength":"bad","intelligence":1,"resilience":1,"vigor":1},"startupAbilities":["/Game/Missing/Ability_C"],"startupPassiveAbilities":[],"unlockableAbilities":[],"startupAbilityDefinitions":[],"startupPassiveAbilityDefinitions":[],"lmbAbility":"","lmbAbilityDefinition":"/Game/Missing/Definition.Missing"},{"role":"Broken"}]})JSON");
	const FAuraRoleLoadResult Result = UAuraAbilitySystemLibrary::ParseRoleInfoJson(nullptr, Bad);
	AddInfo(Result.ToLogString());
	TestFalse(TEXT("Malformed aggregate cannot publish"), Result.bCanPublish);
	TestTrue(TEXT("All malformed fields are accumulated"), Result.Issues.Num() >= 8);
	TestTrue(TEXT("Wrong type reported"), HasIssue(Result, TEXT("displayName")));
	TestTrue(TEXT("Duplicate reported"), HasIssue(Result, TEXT("duplicate stable role ID")));
	TestTrue(TEXT("Invalid asset reported"), HasIssue(Result, TEXT("failed to load")));
	return true;
}

AURA_DAY5_TEST(FAuraDay5DuplicateRoleIdsTest, "DuplicateRoleIds")
bool FAuraDay5DuplicateRoleIdsTest::RunTest(const FString& Parameters)
{
	FString Json = AuraRoleBattleDay5TestsPrivate::LegacyAuraJson(true);
	Json.ReplaceInline(TEXT("]}"), TEXT(",{\"role\":\"Aura\"}]}"));
	const FAuraRoleLoadResult Result = UAuraAbilitySystemLibrary::ParseRoleInfoJson(nullptr, Json);
	TestFalse(TEXT("Duplicate IDs prevent publication"), Result.bCanPublish);
	TestTrue(TEXT("Duplicate ID issue exists"), AuraRoleBattleDay5TestsPrivate::HasIssue(Result, TEXT("duplicate stable role ID")));
	return true;
}

AURA_DAY5_TEST(FAuraDay5FullGameplayTagContractTest, "FullGameplayTagContract")
bool FAuraDay5FullGameplayTagContractTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestTrue(TEXT("Entity.Player registered"), Tags.Entity_Player.IsValid());
	TestTrue(TEXT("Entity.AmbientNPC registered"), Tags.Entity_AmbientNPC.IsValid());
	TestTrue(TEXT("Economy capability tags registered"), Tags.Economy_None.IsValid() && Tags.Economy_Ambient.IsValid() && Tags.Economy_CommerceCapable.IsValid());
	TestTrue(TEXT("Interaction tags registered"), Tags.Interaction_Combatant.IsValid() && Tags.Interaction_Civilian.IsValid());
	return true;
}

AURA_DAY5_TEST(FAuraDay5EquipmentAndSocketValidationTest, "EquipmentAndSocketValidation")
bool FAuraDay5EquipmentAndSocketValidationTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	const FRoleDefaultInfo* Bungee = Result.Candidate ? Result.Candidate->RoleInformation.Find(TEXT("BungeeMan")) : nullptr;
	TestTrue(TEXT("Bungee equipment and sockets validate"), Result.bCanPublish && Bungee && Bungee->SkeletalMesh->FindSocket(Bungee->WeaponSocketName) && Bungee->WeaponMesh->FindSocket(Bungee->WeaponTipSocketName));
	return true;
}

AURA_DAY5_TEST(FAuraDay5CivilianEmptyLoadoutTest, "CivilianEmptyLoadout")
bool FAuraDay5CivilianEmptyLoadoutTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	const FRoleDefaultInfo* Civilian = Result.Candidate ? Result.Candidate->RoleInformation.Find(TEXT("Civilian")) : nullptr;
	TestTrue(TEXT("Civilian is valid ambient content"), Result.bCanPublish && Civilian && !Civilian->bPlayerSelectable && !Civilian->bCanAttack);
	TestTrue(TEXT("Civilian has empty spawn and unlock loadout"), Civilian && Civilian->StartupAbilities.IsEmpty() && Civilian->StartupAbilityDefinitions.IsEmpty() && Civilian->UnlockableAbilities.IsEmpty() && !Civilian->DefaultLMBAbility && !Civilian->DefaultLMBAbilityDefinition);
	return true;
}

AURA_DAY5_TEST(FAuraDay5DefaultRoleSelectableTest, "DefaultRoleSelectable")
bool FAuraDay5DefaultRoleSelectableTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	TestTrue(TEXT("Default is player selectable"), Result.Candidate && Result.Candidate->IsPlayerRoleSelectable(Result.Candidate->DefaultRole));
	return true;
}

AURA_DAY5_TEST(FAuraDay5AtomicReloadRetainsLastGoodTest, "AtomicReloadRetainsLastGood")
bool FAuraDay5AtomicReloadRetainsLastGoodTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Good = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	URoleInfo* Published = Good.Candidate;
	URoleInfo* Identity = Published;
	const FAuraRoleLoadResult Bad = UAuraAbilitySystemLibrary::ParseRoleInfoJson(nullptr, TEXT("{}"));
	TestFalse(TEXT("Bad candidate is rejected"), UAuraAbilitySystemLibrary::TryPublishRoleInfo(Published, Bad));
	TestTrue(TEXT("Last-good object identity retained"), Published == Identity && Published && Published->RoleInformation.Contains(TEXT("Aura")));
	return true;
}

AURA_DAY5_TEST(FAuraDay5ServerRejectsInvalidConfigOrRoleTest, "ServerRejectsInvalidConfigOrRole")
bool FAuraDay5ServerRejectsInvalidConfigOrRoleTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	FString Error;
	TestFalse(TEXT("Unavailable registry rejected"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(nullptr, TEXT("Aura"), Error));
	TestFalse(TEXT("Unknown role rejected"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("Unknown"), Error));
	TestFalse(TEXT("Ambient role rejected"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("Civilian"), Error));
	return true;
}

AURA_DAY5_TEST(FAuraDay5ConnectionScopedRoleRequestTest, "ConnectionScopedRoleRequest")
bool FAuraDay5ConnectionScopedRoleRequestTest::RunTest(const FString& Parameters)
{
	AAuraPlayerState* First = NewObject<AAuraPlayerState>(GetTransientPackage());
	AAuraPlayerState* Second = NewObject<AAuraPlayerState>(GetTransientPackage());
	First->SetPendingAcceptedRoleId(TEXT("Aura"));
	Second->SetPendingAcceptedRoleId(TEXT("BungeeMan"));
	TestEqual(TEXT("First connection retains Aura"), First->GetPendingAcceptedRoleId(), FName(TEXT("Aura")));
	TestEqual(TEXT("Second connection retains BungeeMan"), Second->GetPendingAcceptedRoleId(), FName(TEXT("BungeeMan")));
	return true;
}

AURA_DAY5_TEST(FAuraDay5NetworkCharacterConsumesAcceptedRoleTest, "NetworkCharacterConsumesAcceptedRole")
bool FAuraDay5NetworkCharacterConsumesAcceptedRoleTest::RunTest(const FString& Parameters)
{
	const FString CharacterSourcePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source/Aura/Private/Character/AuraCharacter.cpp"));
	FString CharacterSource;
	if (!TestTrue(TEXT("Network character source is available"), FFileHelper::LoadFileToString(CharacterSource, *CharacterSourcePath)))
	{
		return false;
	}

	TestTrue(TEXT("Network path reads the connection-scoped accepted role"), CharacterSource.Contains(TEXT("GetPendingAcceptedRoleId")));
	TestTrue(TEXT("Accepted role enters the single spawn transaction"), CharacterSource.Contains(TEXT("ApplyRoleAtSpawn(AuthorizedRole, SaveData)")));
	TestFalse(TEXT("Network path no longer commits PlayerState before the transaction"), CharacterSource.Contains(TEXT("AuraPlayerState->SetRole(AcceptedRole)")));
	TestFalse(TEXT("Network path no longer grants outside the transaction"), CharacterSource.Contains(TEXT("AddCharacterAbilitiesFromSaveData(SaveData)")));
	TestFalse(TEXT("Network path no longer initializes the default role"), CharacterSource.Contains(TEXT("InitializeDefaultAttributesForRole(UAuraAbilitySystemLibrary::GetDefaultRole(this))")));
	TestTrue(TEXT("Accepted role remains connection-scoped for respawn"), CharacterSource.Contains(TEXT("AuraPlayerState->HasPendingAcceptedRoleId()")));
	return true;
}

AURA_DAY5_TEST(FAuraDay5LoadScreenRoleValidationTest, "LoadScreenRoleValidation")
bool FAuraDay5LoadScreenRoleValidationTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	FString Error;
	TestTrue(TEXT("Aura accepted for load screen"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("Aura"), Error));
	TestFalse(TEXT("Civilian excluded from load screen"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("Civilian"), Error));
	return true;
}

AURA_DAY5_TEST(FAuraDay5SavedRoleIdCompatibilityTest, "SavedRoleIdCompatibility")
bool FAuraDay5SavedRoleIdCompatibilityTest::RunTest(const FString& Parameters)
{
	const FAuraRoleLoadResult Result = AuraRoleBattleDay5TestsPrivate::LoadShipped();
	FString Error;
	TestTrue(TEXT("Stable Aura save ID retained"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("Aura"), Error));
	TestTrue(TEXT("Stable BungeeMan save ID retained"), UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(Result.Candidate, TEXT("BungeeMan"), Error));
	return true;
}

#undef AURA_DAY5_TEST

#if WITH_EDITOR

namespace AuraRoleBattleDay6TestsPrivate
{
	AAuraRoleApplicationTestActor* SpawnFixture(FName RoleId,
		UAuraAbilitySystemComponent* ExternalASC = nullptr, UAttributeSet* ExternalAttributes = nullptr,
		AActor* ExternalOwner = nullptr)
	{
		UWorld* World = AuraRoleBattleTestsPrivate::FindAutomationWorld();
		const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(World);
		const FRoleDefaultInfo* RoleDefinition = RoleInfo ? RoleInfo->RoleInformation.Find(RoleId) : nullptr;
		if (!World || !RoleDefinition) return nullptr;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAuraRoleApplicationTestActor* Fixture = World->SpawnActor<AAuraRoleApplicationTestActor>(
			AAuraRoleApplicationTestActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!Fixture) return nullptr;
		Fixture->ConfigureRoleShell(RoleDefinition->EntityType, RoleDefinition->ControlType);
		if (ExternalASC)
		{
			Fixture->UseExternalAbilitySystem(ExternalASC, ExternalAttributes);
		}
		if (!Fixture->InitializeTestAbilityActorInfo(ExternalOwner))
		{
			Fixture->Destroy();
			return nullptr;
		}
		return Fixture;
	}

	TArray<FGameplayTag> GetRequiredTags(const FRoleDefaultInfo& RoleDefinition)
	{
		TArray<FGameplayTag> Tags;
		for (const TObjectPtr<UObject>& Object : RoleDefinition.StartupAbilityDefinitions)
		{
			if (const UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Object.Get())) Tags.Add(Definition->AbilityTag);
		}
		for (const TObjectPtr<UObject>& Object : RoleDefinition.StartupPassiveAbilityDefinitions)
		{
			if (const UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(Object.Get())) Tags.Add(Definition->AbilityTag);
		}
		if (const UAuraAbilityDefinition* Definition = Cast<UAuraAbilityDefinition>(RoleDefinition.DefaultLMBAbilityDefinition.Get()))
		{
			Tags.Add(Definition->AbilityTag);
		}
		return Tags;
	}
}

#define AURA_DAY6_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, "Aura.RoleBattle.Day6." TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AURA_DAY6_TEST(FAuraDay6AuraAppliedIdentityAndProfilesTest, "AuraAppliedIdentityAndProfiles")
bool FAuraDay6AuraAppliedIdentityAndProfilesTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Aura"));
	if (!TestNotNull(TEXT("Aura role fixture spawned"), Fixture)) return false;
	const FAuraRoleApplicationResult Result = Fixture->ApplyRoleAtSpawn(TEXT("Aura"));
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestTrue(TEXT("Aura applies transactionally"), Result.bSuccess);
	TestEqual(TEXT("Aura final combat profile"), Fixture->GetCombatIdentity().CombatProfileTag, Tags.Combat_Magic);
	TestEqual(TEXT("Aura entity profile"), Fixture->GetAppliedRoleState().EntityTypeTag, Tags.Entity_Player);
	TestEqual(TEXT("Aura economy profile"), Fixture->GetAppliedRoleState().EconomyProfileTag, Tags.Economy_None);
	TestEqual(TEXT("Aura interaction profile"), Fixture->GetAppliedRoleState().InteractionProfileTag, Tags.Interaction_Combatant);
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6BungeeAppliedIdentityAndProfilesTest, "BungeeAppliedIdentityAndProfiles")
bool FAuraDay6BungeeAppliedIdentityAndProfilesTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("BungeeMan"));
	if (!TestNotNull(TEXT("BungeeMan role fixture spawned"), Fixture)) return false;
	const FAuraRoleApplicationResult Result = Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan"));
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TestTrue(TEXT("BungeeMan applies transactionally"), Result.bSuccess);
	TestEqual(TEXT("BungeeMan final combat profile"), Fixture->GetCombatIdentity().CombatProfileTag, Tags.Combat_Gun);
	TestEqual(TEXT("BungeeMan role ID published"), Fixture->GetAppliedRoleState().RoleId, FName(TEXT("BungeeMan")));
	TestEqual(TEXT("BungeeMan muzzle socket published"), Fixture->GetWeaponTipSocketForTest(), FName(TEXT("Muzzle")));
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6FreshCivilianDefinitionHasEmptyLoadoutTest, "FreshCivilianDefinitionHasEmptyLoadout")
bool FAuraDay6FreshCivilianDefinitionHasEmptyLoadoutTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Civilian"));
	if (!TestNotNull(TEXT("Civilian fixture spawned"), Fixture)) return false;
	const FAuraRoleApplicationResult Result = Fixture->ApplyRoleAtSpawn(TEXT("Civilian"));
	TestTrue(TEXT("Compatible actor-owned civilian applies"), Result.bSuccess);
	TestEqual(TEXT("Civilian grants no specs"), Fixture->GetTestASC()->GetActivatableAbilities().Num(), 0);
	TestNull(TEXT("Civilian equips no weapon"), Fixture->GetEquippedWeaponMesh());
	TestFalse(TEXT("Civilian cannot attack"), Fixture->GetCombatIdentity().bCanAttack);
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6ExplicitEquipmentOnlyTest, "ExplicitEquipmentOnly")
bool FAuraDay6ExplicitEquipmentOnlyTest::RunTest(const FString& Parameters)
{
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
	const FRoleDefaultInfo* Bungee = RoleInfo ? RoleInfo->RoleInformation.Find(TEXT("BungeeMan")) : nullptr;
	AAuraRoleApplicationTestActor* Armed = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("BungeeMan"));
	AAuraRoleApplicationTestActor* Unarmed = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Civilian"));
	if (!TestNotNull(TEXT("Armed fixture"), Armed) || !TestNotNull(TEXT("Unarmed fixture"), Unarmed) || !Bungee) return false;
	TestTrue(TEXT("Armed role applies"), Armed->ApplyRoleAtSpawn(TEXT("BungeeMan")).bSuccess);
	TestTrue(TEXT("Unarmed role applies"), Unarmed->ApplyRoleAtSpawn(TEXT("Civilian")).bSuccess);
	TestTrue(TEXT("Only explicit Bungee equipment is used"), Armed->GetEquippedWeaponMesh() == Bungee->WeaponMesh.Get());
	TestNull(TEXT("Missing equipment structure clears inherited weapon"), Unarmed->GetEquippedWeaponMesh());
	Armed->Destroy();
	Unarmed->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6SpawnGrantIdempotenceTest, "SpawnGrantIdempotence")
bool FAuraDay6SpawnGrantIdempotenceTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("BungeeMan"));
	if (!TestNotNull(TEXT("BungeeMan fixture"), Fixture)) return false;
	UAuraAbilitySystemComponent* ASC = Fixture->GetTestASC();
	int32 Notifications = 0;
	ASC->AbilitiesGivenDelegate.AddLambda([&Notifications]() { ++Notifications; });
	TestTrue(TEXT("First apply succeeds"), Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan")).bSuccess);
	TestTrue(TEXT("Same-role reapply succeeds"), Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan")).bSuccess);
	const FGameplayTag FireGun = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"));
	TestEqual(TEXT("Exactly one FireGun spec"), ASC->CountAbilitySpecsByTag(FireGun), 1);
	TestEqual(TEXT("Grant notification fires once"), Notifications, 1);
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6PersistentASCRespawnLedgerTest, "PersistentASCRespawnLedger")
bool FAuraDay6PersistentASCRespawnLedgerTest::RunTest(const FString& Parameters)
{
	UWorld* World = AuraRoleBattleTestsPrivate::FindAutomationWorld();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AAuraPlayerState* PlayerState = World ? World->SpawnActor<AAuraPlayerState>(SpawnParameters) : nullptr;
	if (!TestNotNull(TEXT("Persistent PlayerState spawned"), PlayerState)) return false;
	UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent());
	UAttributeSet* Attributes = PlayerState->GetAttributeSet();
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(World);
	const FRoleDefaultInfo* Aura = RoleInfo ? RoleInfo->RoleInformation.Find(TEXT("Aura")) : nullptr;
	if (!TestNotNull(TEXT("Persistent ASC"), ASC) || !Aura) return false;
	int32 Notifications = 0;
	ASC->AbilitiesGivenDelegate.AddLambda([&Notifications]() { ++Notifications; });
	float ExpectedMaxHealth = 0.f;
	float ExpectedMaxMana = 0.f;
	for (int32 SpawnIndex = 0; SpawnIndex < 3; ++SpawnIndex)
	{
		AAuraRoleApplicationTestActor* Pawn = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Aura"), ASC, Attributes, PlayerState);
		if (!TestNotNull(FString::Printf(TEXT("Pawn %d spawned"), SpawnIndex + 1), Pawn)) return false;
		TestTrue(FString::Printf(TEXT("Pawn %d applies same role"), SpawnIndex + 1), Pawn->ApplyRoleAtSpawn(TEXT("Aura")).bSuccess);
		const UAuraAttributeSet* AuraAttributes = Cast<UAuraAttributeSet>(Attributes);
		if (SpawnIndex == 0)
		{
			ExpectedMaxHealth = AuraAttributes->GetMaxHealth();
			ExpectedMaxMana = AuraAttributes->GetMaxMana();
		}
		else
		{
			TestEqual(TEXT("MaxHealth unchanged across pawn replacement"), AuraAttributes->GetMaxHealth(), ExpectedMaxHealth);
			TestEqual(TEXT("MaxMana unchanged across pawn replacement"), AuraAttributes->GetMaxMana(), ExpectedMaxMana);
		}
		for (const FGameplayTag& Tag : AuraRoleBattleDay6TestsPrivate::GetRequiredTags(*Aura))
		{
			TestEqual(FString::Printf(TEXT("One persistent spec for %s"), *Tag.ToString()), ASC->CountAbilitySpecsByTag(Tag), 1);
		}
		TestEqual(TEXT("Pawn receives final magic identity"), Pawn->GetCombatIdentity().CombatProfileTag, FAuraGameplayTags::Get().Combat_Magic);
		Pawn->Destroy();
	}
	TestEqual(TEXT("One grant notification across three pawns"), Notifications, 1);
	TestEqual(TEXT("Ledger retains Aura"), ASC->GetRoleGrantLedger().GrantedRoleId, FName(TEXT("Aura")));
	PlayerState->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6ExistingSaveGrantReconciliationTest, "ExistingSaveGrantReconciliation")
bool FAuraDay6ExistingSaveGrantReconciliationTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Aura"));
	if (!TestNotNull(TEXT("Aura fixture"), Fixture)) return false;
	ULoadScreenSaveGame* Save = NewObject<ULoadScreenSaveGame>();
	Save->Role = TEXT("Aura");
	Save->bFirstTimeLoadIn = true;
	FSavedAbility FireBolt;
	FireBolt.AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt"));
	FireBolt.AbilitySlot = FAuraGameplayTags::Get().InputTag_LMB;
	FireBolt.AbilityStatus = FAuraGameplayTags::Get().Abilities_Status_Equipped;
	FireBolt.AbilityLevel = 3;
	FireBolt.GrantSource = static_cast<uint8>(EAuraAbilityGrantSource::Role);
	FireBolt.GrantedRoleId = TEXT("Aura");
	Save->SavedAbilities.Add(FireBolt);
	TestTrue(TEXT("Saved Aura reconciles"), Fixture->ApplyRoleAtSpawn(TEXT("Aura"), Save).bSuccess);
	FGameplayAbilitySpec* Restored = Fixture->GetTestASC()->GetSpecFromAbilityTag(FireBolt.AbilityTag);
	TestTrue(TEXT("Saved FireBolt retained level/slot/status"), Restored && Restored->Level == 3
		&& UAuraAbilitySystemComponent::AbilityHasSlot(*Restored, FireBolt.AbilitySlot)
		&& UAuraAbilitySystemComponent::GetStatusFromSpec(*Restored).MatchesTagExact(FireBolt.AbilityStatus));
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(nullptr);
	for (const FGameplayTag& Tag : AuraRoleBattleDay6TestsPrivate::GetRequiredTags(RoleInfo->GetRoleDefaultInfo(TEXT("Aura"))))
	{
		TestEqual(FString::Printf(TEXT("Save reconciliation produces one %s"), *Tag.ToString()), Fixture->GetTestASC()->CountAbilitySpecsByTag(Tag), 1);
	}
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6RemovedRoleGrantNotPromotedTest, "RemovedRoleGrantNotPromoted")
bool FAuraDay6RemovedRoleGrantNotPromotedTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("BungeeMan"));
	if (!TestNotNull(TEXT("Bungee fixture"), Fixture)) return false;
	ULoadScreenSaveGame* Save = NewObject<ULoadScreenSaveGame>();
	Save->Role = TEXT("BungeeMan");
	FSavedAbility Removed;
	Removed.AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Lightning.Electrocute"));
	Removed.GrantSource = static_cast<uint8>(EAuraAbilityGrantSource::Role);
	Removed.GrantedRoleId = TEXT("BungeeMan");
	Save->SavedAbilities.Add(Removed);
	TestTrue(TEXT("Role applies while removed grant is quarantined"), Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan"), Save).bSuccess);
	TestEqual(TEXT("Removed role grant is not restored as progression"), Fixture->GetTestASC()->CountAbilitySpecsByTag(Removed.AbilityTag), 0);
	TestEqual(TEXT("Current FireGun requirement is reconciled"), Fixture->GetTestASC()->CountAbilitySpecsByTag(
		FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"))), 1);
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6RoleGrantSourceMetadataTest, "RoleGrantSourceMetadata")
bool FAuraDay6RoleGrantSourceMetadataTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("BungeeMan"));
	if (!TestNotNull(TEXT("Bungee fixture"), Fixture)) return false;
	TestTrue(TEXT("Bungee role applies"), Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan")).bSuccess);
	const FGameplayTag FireGun = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"));
	FGameplayAbilitySpec* Spec = Fixture->GetTestASC()->GetSpecFromAbilityTag(FireGun);
	FName GrantedRoleId;
	TestTrue(TEXT("FireGun has replicated role-source tag"), Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(FAuraGameplayTags::Get().GrantSource_Role));
	if (Spec)
	{
		TestEqual(TEXT("Server provenance map retains role ID"), Fixture->GetTestASC()->GetGrantSourceForSpec(*Spec, GrantedRoleId), EAuraAbilityGrantSource::Role);
		TestEqual(TEXT("Server provenance is BungeeMan"), GrantedRoleId, FName(TEXT("BungeeMan")));
	}
	TestTrue(TEXT("Definition remains SourceObject"), Spec && Spec->SourceObject.IsValid() && Spec->SourceObject.Get()->IsA<UAuraAbilityDefinition>());
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6ClientRoleMutationRejectedTest, "ClientRoleMutationRejected")
bool FAuraDay6ClientRoleMutationRejectedTest::RunTest(const FString& Parameters)
{
	AAuraPlayerState* ClientLikeState = NewObject<AAuraPlayerState>(GetTransientPackage());
	ClientLikeState->AActor::SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Non-authority PlayerState role write is rejected"), ClientLikeState->SetRole(TEXT("BungeeMan")));
	TestTrue(TEXT("Rejected mutation publishes no role"), ClientLikeState->GetRole().IsNone());
	const FString SourcePath = FPaths::ProjectDir() / TEXT("Source/Aura/Private/Character/AuraCharacterBase.cpp");
	FString Source;
	TestTrue(TEXT("Role application source is readable"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestTrue(TEXT("ApplyRoleAtSpawn has an authority gate"), Source.Contains(TEXT("if (!HasAuthority())"))
		&& Source.Contains(TEXT("EAuraRoleApplicationError::NotAuthority")));
	for (const TCHAR* RelativePath : {
		TEXT("Source/Aura/Private/UI/ViewModel/MVVM_LoadScreen.cpp"),
		TEXT("Source/Aura/Private/UI/WidgetController/AuraWidgetController.cpp"),
		TEXT("Source/Aura/Private/UI/WidgetController/OverlayWidgetController.cpp"),
		TEXT("Source/Aura/Private/UI/WidgetController/SpellMenuWidgetController.cpp")})
	{
		FString UISource;
		const FString UIPath = FPaths::ProjectDir() / RelativePath;
		TestTrue(FString::Printf(TEXT("UI source is readable: %s"), RelativePath), FFileHelper::LoadFileToString(UISource, *UIPath));
		TestFalse(FString::Printf(TEXT("UI does not authorize role application: %s"), RelativePath), UISource.Contains(TEXT("ApplyRoleAtSpawn")));
		TestFalse(FString::Printf(TEXT("UI does not grant abilities: %s"), RelativePath), UISource.Contains(TEXT("GiveAbility(")));
		TestFalse(FString::Printf(TEXT("UI does not initialize authoritative role attributes: %s"), RelativePath), UISource.Contains(TEXT("InitializeDefaultAttributesForRole")));
	}
	return true;
}

AURA_DAY6_TEST(FAuraDay6LiveRoleSwitchRejectedTest, "LiveRoleSwitchRejected")
bool FAuraDay6LiveRoleSwitchRejectedTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Aura"));
	if (!TestNotNull(TEXT("Aura fixture"), Fixture)) return false;
	TestTrue(TEXT("Initial Aura applies"), Fixture->ApplyRoleAtSpawn(TEXT("Aura")).bSuccess);
	const int32 SpecsBefore = Fixture->GetTestASC()->GetActivatableAbilities().Num();
	const FAuraRoleApplicationResult LedgerSwitch = Fixture->ApplyRoleAtSpawn(TEXT("BungeeMan"));
	TestFalse(TEXT("Persistent ledger rejects different player role"), LedgerSwitch.bSuccess);
	TestEqual(TEXT("Ledger rejection is unsupported live switch"), LedgerSwitch.Error, EAuraRoleApplicationError::UnsupportedLiveSwitch);
	TestEqual(TEXT("Applied role remains Aura"), Fixture->GetAppliedRoleState().RoleId, FName(TEXT("Aura")));
	TestEqual(TEXT("Spec count is unchanged"), Fixture->GetTestASC()->GetActivatableAbilities().Num(), SpecsBefore);
	Fixture->Destroy();
	return true;
}

AURA_DAY6_TEST(FAuraDay6ApplicationFailureIsAtomicTest, "ApplicationFailureIsAtomic")
bool FAuraDay6ApplicationFailureIsAtomicTest::RunTest(const FString& Parameters)
{
	AAuraRoleApplicationTestActor* Fixture = AuraRoleBattleDay6TestsPrivate::SpawnFixture(TEXT("Civilian"));
	if (!TestNotNull(TEXT("Civilian shell fixture"), Fixture)) return false;
	const FAuraCombatIdentity IdentityBefore = Fixture->GetCombatIdentity();
	const FAuraRoleApplicationResult Failure = Fixture->ApplyRoleAtSpawn(TEXT("Aura"));
	TestFalse(TEXT("Incompatible candidate fails staging"), Failure.bSuccess);
	TestTrue(TEXT("No applied state is published"), Fixture->GetAppliedRoleState().RoleId.IsNone());
	TestTrue(TEXT("Combat identity is unchanged"), Fixture->GetCombatIdentity() == IdentityBefore);
	TestEqual(TEXT("No ability grant is published"), Fixture->GetTestASC()->GetActivatableAbilities().Num(), 0);
	TestNull(TEXT("No equipment is published"), Fixture->GetEquippedWeaponMesh());
	TestFalse(TEXT("Attribute initialization remains uncommitted"), Fixture->GetTestASC()->GetRoleGrantLedger().bAttributesInitialized);
	Fixture->Destroy();
	return true;
}

#undef AURA_DAY6_TEST

#endif

#endif
