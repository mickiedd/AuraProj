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
#include "Data/AuraGameplayConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/AuraPlayerState.h"

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

#endif
