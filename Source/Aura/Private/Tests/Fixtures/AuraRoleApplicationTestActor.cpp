// Copyright Druid Mechanics

#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AuraGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

AAuraRoleApplicationTestActor::AAuraRoleApplicationTestActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAuraAbilitySystemComponent>(TEXT("TestAbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AttributeSet = CreateDefaultSubobject<UAuraAttributeSet>(TEXT("TestAttributeSet"));
}

void AAuraRoleApplicationTestActor::ConfigureRoleShell(
	const FGameplayTag& EntityType, const FGameplayTag& ControlType)
{
	RequiredEntityType = EntityType;
	RequiredControlType = ControlType;
}

void AAuraRoleApplicationTestActor::UseExternalAbilitySystem(
	UAuraAbilitySystemComponent* ExternalASC, UAttributeSet* ExternalAttributes)
{
	AbilitySystemComponent = ExternalASC;
	AttributeSet = ExternalAttributes;
}

bool AAuraRoleApplicationTestActor::InitializeTestAbilityActorInfo(AActor* OwnerActor)
{
	UAuraAbilitySystemComponent* AuraASC = GetTestASC();
	if (!AuraASC) return false;
	AuraASC->InitAbilityActorInfo(OwnerActor ? OwnerActor : this, this);
	AuraASC->AbilityActorInfoSet();
	return AuraASC->GetOwnerActor() != nullptr && AuraASC->GetAvatarActor() == this;
}

UAuraAbilitySystemComponent* AAuraRoleApplicationTestActor::GetTestASC() const
{
	return Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
}

USkeletalMesh* AAuraRoleApplicationTestActor::GetEquippedWeaponMesh() const
{
	return Weapon ? Weapon->GetSkeletalMeshAsset() : nullptr;
}

FGameplayTag AAuraRoleApplicationTestActor::GetRequiredRoleEntityType() const
{
	return RequiredEntityType;
}

FAuraCombatIdentity AAuraRoleApplicationTestActor::BuildDefaultCombatIdentity() const
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	FAuraCombatIdentity Identity;
	Identity.FactionTag = RequiredEntityType.MatchesTagExact(GameplayTags.Entity_AmbientNPC) ? GameplayTags.Faction_Civilian : GameplayTags.Faction_Player;
	Identity.ControlTypeTag = RequiredControlType.IsValid() ? RequiredControlType : GameplayTags.Control_Player;
	Identity.CombatProfileTag = RequiredEntityType.MatchesTagExact(GameplayTags.Entity_AmbientNPC) ? GameplayTags.Combat_Civilian : GameplayTags.Combat_Unassigned;
	Identity.DeathPolicyTag = RequiredEntityType.MatchesTagExact(GameplayTags.Entity_AmbientNPC) ? GameplayTags.Death_PopulationRespawn : GameplayTags.Death_PlayerRespawn;
	Identity.bTargetable = true;
	Identity.bCanAttack = !RequiredEntityType.MatchesTagExact(GameplayTags.Entity_AmbientNPC);
	Identity.bCanBeDamaged = true;
	return Identity;
}

FGameplayTag AAuraRoleApplicationTestActor::GetRequiredRoleControlType() const
{
	return RequiredControlType;
}
