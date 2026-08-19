// Copyright Druid Mechanics

#include "Character/AuraCivilian.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AI/AuraCivilianAIController.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"

AAuraCivilian::AAuraCivilian()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAuraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UAuraAttributeSet>(TEXT("AttributeSet"));

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetRootComponent());

	AIControllerClass = AAuraCivilianAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->GravityScale = 1.f;
	BaseWalkSpeed = 120.f;
}

void AAuraCivilian::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraCivilian, PopulationMemberState);
	DOREPLIFETIME(AAuraCivilian, CivilianActivity);
}

void AAuraCivilian::SetRequestedCivilianRoleId(FName InRoleId)
{
	if (HasAuthority() && !InRoleId.IsNone())
	{
		RequestedCivilianRoleId = InRoleId;
	}
}

void AAuraCivilian::SetPopulationMemberState(const FAuraPopulationMemberState& InState)
{
	if (HasAuthority() && InState.IsValid())
	{
		PopulationMemberState = InState;
	}
}

bool AAuraCivilian::AlignToGroundForSpawn()
{
	if (!HasAuthority() || !GetWorld() || !GetCapsuleComponent())
	{
		return false;
	}

	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector CurrentLocation = GetActorLocation();
	const FVector TraceStart = CurrentLocation + FVector(0.f, 0.f, 1000.f);
	const FVector TraceEnd = CurrentLocation - FVector(0.f, 0.f, 1000.f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AuraCivilianGroundPlacement), false);
	QueryParams.AddIgnoredActor(this);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult GroundHit;
	FVector GroundLocation = CurrentLocation;
	float GroundZ = CurrentLocation.Z - CapsuleHalfHeight;
	bool bFoundGround = GetWorld()->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectQueryParams, QueryParams);
	if (bFoundGround)
	{
		GroundZ = GroundHit.ImpactPoint.Z;
		GroundLocation.Z = GroundZ + CapsuleHalfHeight + 2.f;
	}
	else if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (NavigationSystem->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, FVector(200.f, 200.f, 1000.f)))
		{
			GroundZ = ProjectedLocation.Location.Z;
			GroundLocation.Z = GroundZ + CapsuleHalfHeight + 2.f;
			bFoundGround = true;
		}
	}

	if (!bFoundGround)
	{
		UE_LOG(LogAura, Warning, TEXT("[Civilian][Ground] No floor found for %s at %s."),
			*GetNameSafe(this), *CurrentLocation.ToCompactString());
		return false;
	}

	const bool bCorrected = !FMath::IsNearlyEqual(CurrentLocation.Z, GroundLocation.Z, 0.5f);
	if (bCorrected)
	{
		SetActorLocation(GroundLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->bForceNextFloorCheck = true;
	UE_LOG(LogAura, Display, TEXT("[Civilian][Ground] actor=%s floorZ=%.2f actorZ=%.2f corrected=%d."),
		*GetNameSafe(this), GroundZ, GetActorLocation().Z, bCorrected ? 1 : 0);
	return true;
}

void AAuraCivilian::SetCivilianActivity(EAuraCivilianActivity InActivity)
{
	if (HasAuthority())
	{
		CivilianActivity = InActivity;
		ForceNetUpdate();
	}
}

FAuraCombatIdentity AAuraCivilian::BuildDefaultCombatIdentity() const
{
	// Civilian identity is role-owned. Returning an invalid pre-role identity
	// keeps the actor fail-closed until ApplyRoleAtSpawn commits the validated
	// Civilian definition on authority.
	return FAuraCombatIdentity();
}

FGameplayTag AAuraCivilian::GetRequiredRoleEntityType() const
{
	return FAuraGameplayTags::Get().Entity_AmbientNPC;
}

FGameplayTag AAuraCivilian::GetRequiredRoleControlType() const
{
	return FAuraGameplayTags::Get().Control_CivilianAI;
}

void AAuraCivilian::BeginPlay()
{
	if (HasAuthority() && !GetController())
	{
		SpawnDefaultController();
	}
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	if (HealthBar && GetNetMode() == NM_DedicatedServer) HealthBar->SetVisibility(false);

	InitAbilityActorInfo();
	if (HasAuthority())
	{
		const FAuraRoleApplicationResult RoleResult = ApplyRoleAtSpawn(RequestedCivilianRoleId);
		if (!RoleResult.bSuccess)
		{
			UE_LOG(LogAura, Error, TEXT("[Civilian][Init] Failed closed actor=%s role=%s reason=%s; no population registration."),
				*GetNameSafe(this), *RequestedCivilianRoleId.ToString(), *RoleResult.Message);
			SetActorEnableCollision(false);
			return;
		}

		AlignToGroundForSpawn();
		UE_LOG(LogAura, Display, TEXT("[Civilian][Init] actor=%s role=%s ASCOwner=%s Avatar=%s identity=%s/%s/%s healthDelegates=%d."),
			*GetNameSafe(this), *RoleResult.RoleId.ToString(), *GetNameSafe(AbilitySystemComponent->GetOwnerActor()),
			*GetNameSafe(AbilitySystemComponent->GetAvatarActor()), *GetCombatIdentity().FactionTag.ToString(),
			*GetCombatIdentity().ControlTypeTag.ToString(), *GetCombatIdentity().CombatProfileTag.ToString(), bHealthDelegatesBound ? 1 : 0);
		MarkCombatReady();
		if (AAuraCivilianAIController* CivilianController = Cast<AAuraCivilianAIController>(GetController()))
		{
			CivilianController->StartCivilianBehavior();
		}
	}
}

void AAuraCivilian::InitAbilityActorInfo()
{
	if (!AbilitySystemComponent) return;
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent)) AuraASC->AbilityActorInfoSet();

	if (!bHealthDelegatesBound)
	{
		if (const UAuraAttributeSet* AuraAttributes = Cast<UAuraAttributeSet>(AttributeSet))
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAttributes->GetHealthAttribute()).AddLambda(
				[this](const FOnAttributeChangeData& Data) { OnHealthChanged.Broadcast(Data.NewValue); });
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAttributes->GetMaxHealthAttribute()).AddLambda(
				[this](const FOnAttributeChangeData& Data) { OnMaxHealthChanged.Broadcast(Data.NewValue); });
			bHealthDelegatesBound = true;
		}
	}
	OnAscRegistered.Broadcast(AbilitySystemComponent);
}

void AAuraCivilian::OnRep_PopulationMemberState()
{
	UE_LOG(LogAura, Display, TEXT("[Civilian][Population] Replicated member=%s population=%s slot=%d work=%s zone=%s merchant=%s."),
		*PopulationMemberState.PopulationMemberId.ToString(), *PopulationMemberState.PopulationId.ToString(),
		PopulationMemberState.PopulationSlotIndex, *PopulationMemberState.WorkProfileId.ToString(),
		*PopulationMemberState.ZoneId.ToString(), *PopulationMemberState.MerchantDefinitionId.ToString());
}
