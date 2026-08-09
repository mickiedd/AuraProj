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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/AuraPlayerState.h"

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

#endif
