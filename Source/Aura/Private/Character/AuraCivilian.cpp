// Copyright Druid Mechanics

#include "Character/AuraCivilian.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AI/AuraCivilianAIController.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Economy/AuraCommerceSubsystem.h"
#include "Economy/AuraMerchantComponent.h"
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
	MerchantComponent = CreateDefaultSubobject<UAuraMerchantComponent>(TEXT("MerchantComponent"));

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

FGameplayTag AAuraCivilian::GetAuraTargetKind() const
{
	return FAuraGameplayTags::Get().Target_Kind_Civilian;
}

FText AAuraCivilian::GetAuraTargetDisplayName() const
{
	return FText::FromString(TEXT("Civilian"));
}

void AAuraCivilian::GetAuraInteractionOptions(const AActor* RequestingActor, TArray<FAuraInteractionOption>& OutOptions) const
{
	OutOptions.Reset();
	if (!IsCombatAlive()) return;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	auto AddEnabled = [&OutOptions](const FGameplayTag Tag, const TCHAR* Text)
	{
		FAuraInteractionOption& Option = OutOptions.AddDefaulted_GetRef();
		Option.OptionTag = Tag;
		Option.DisplayText = FText::FromString(Text);
		Option.bEnabled = true;
		Option.DisabledReason = EAuraInteractionResultCode::Success;
	};
	AddEnabled(GameplayTags.Interaction_Talk, TEXT("Talk"));
	AddEnabled(GameplayTags.Interaction_Observe, TEXT("Observe"));
	if (MerchantComponent && MerchantComponent->IsMerchantActive())
	{
		FAuraInteractionOption& Trade = OutOptions.AddDefaulted_GetRef();
		Trade.OptionTag = GameplayTags.Interaction_Trade;
		Trade.DisplayText = FText::FromString(TEXT("Trade"));
	Trade.bEnabled = true;
	Trade.DisabledReason = EAuraInteractionResultCode::Success;
	}
}

bool AAuraCivilian::ExecuteAuraInteraction(const AActor* RequestingActor, FGameplayTag OptionTag) const
{
	if (!IsValid(RequestingActor) || !IsCombatAlive()) return false;
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (!OptionTag.MatchesTagExact(GameplayTags.Interaction_Talk)
		&& !OptionTag.MatchesTagExact(GameplayTags.Interaction_Observe)
		&& !(OptionTag.MatchesTagExact(GameplayTags.Interaction_Trade) && MerchantComponent && MerchantComponent->IsMerchantActive()))
	{
		return false;
	}
	UE_LOG(LogAura, Display, TEXT("[Interaction][Server] Executed option=%s requester=%s target=%s member=%s."),
		*OptionTag.ToString(), *GetNameSafe(RequestingActor), *GetNameSafe(this), *PopulationMemberState.PopulationMemberId.ToString());
	return true;
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
		if (UAuraCombatStateComponent* CombatState = GetCombatStateComponentMutable())
		{
			CombatState->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleMerchantLifeStateChanged);
		}
		const FAuraRoleApplicationResult RoleResult = ApplyRoleAtSpawn(RequestedCivilianRoleId);
		if (!RoleResult.bSuccess)
		{
			UE_LOG(LogAura, Error, TEXT("[Civilian][Init] Failed closed actor=%s role=%s reason=%s; no population registration."),
				*GetNameSafe(this), *RequestedCivilianRoleId.ToString(), *RoleResult.Message);
			SetActorEnableCollision(false);
			return;
		}
		if (MerchantComponent && !PopulationMemberState.MerchantDefinitionId.IsNone())
		{
			MerchantComponent->InitializeAuthorityBinding(PopulationMemberState.PopulationMemberId, PopulationMemberState.MerchantDefinitionId);
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

void AAuraCivilian::HandleMerchantLifeStateChanged(EAuraCombatLifeState NewState)
{
	if (HasAuthority() && NewState != EAuraCombatLifeState::Alive && MerchantComponent)
	{
		if (UWorld* World = GetWorld())
		{
			if (UAuraCommerceSubsystem* Commerce = World->GetSubsystem<UAuraCommerceSubsystem>())
			{
				Commerce->MarkMerchantUnavailable(MerchantComponent);
				return;
			}
		}
		MerchantComponent->SetAuthorityUnavailable();
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
