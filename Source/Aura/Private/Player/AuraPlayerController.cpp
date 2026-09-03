// Copyright Druid Mechanics


#include "Player/AuraPlayerController.h"
#include "UI/HUD/AuraHUD.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "Actor/MagicCircle.h"
#include "AuraAbilityTypes.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraEnemy.h"
#include "Character/AuraCivilian.h"
#include "Character/AuraCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/AuraInputComponent.h"
#include "Interaction/EnemyInterface.h"
#include "Interaction/AuraInteractionComponent.h"
#include "Combat/AuraTargetableInterface.h"
#include "Combat/AuraCombatRules.h"
#include "UI/WidgetController/TargetInteractionWidgetController.h"
#include "GameFramework/Character.h"
#include "Interaction/HighlightInterface.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "UI/Widget/DamageTextComponent.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "Game/ServerTravelComponent.h"
#include "Game/AuraGameModeBase.h"
#include "Client/AuraClientDisconnectHandler.h"
#include "Network/AuraHeartbeatComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Vehicle/AuraBroomVehicle.h"
#include "Building/AuraBuildingComponent.h"
#include "Actor/AuraProjectile.h"
#include "Data/AuraGameplayConfig.h"
#include "Economy/AuraCommerceSubsystem.h"
#include "Economy/AuraMerchantComponent.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Game/LoadScreenSaveGame.h"
#include "Player/AuraPlayerState.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
	Spline = CreateDefaultSubobject<USplineComponent>("Spline");
	ServerTravelComponent = CreateDefaultSubobject<UServerTravelComponent>(TEXT("ServerTravelComponent"));
	ClientDisconnectHandler = CreateDefaultSubobject<UAuraClientDisconnectHandler>(TEXT("ClientDisconnectHandler"));
	HeartbeatComponent = CreateDefaultSubobject<UAuraHeartbeatComponent>(TEXT("HeartbeatComponent"));
	InteractionComponent = CreateDefaultSubobject<UAuraInteractionComponent>(TEXT("InteractionComponent"));
}

void AAuraPlayerController::RequestBroomDismount(AAuraBroomVehicle* BroomToDismount)
{
	if (!IsValid(BroomToDismount))
	{
		UE_LOG(LogAura, Warning, TEXT("RequestBroomDismount ignored: invalid broom. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		ServerRequestBroomDismount_Implementation(BroomToDismount);
		return;
	}

	ServerRequestBroomDismount(BroomToDismount);
}

void AAuraPlayerController::RequestBroomMount(AAuraBroomVehicle* BroomToMount)
{
	if (!IsValid(BroomToMount))
	{
		UE_LOG(LogAura, Warning, TEXT("RequestBroomMount ignored: invalid broom. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		ServerRequestBroomMount_Implementation(BroomToMount);
		return;
	}

	ServerRequestBroomMount(BroomToMount);
}

void AAuraPlayerController::ShiftPressed()
{
	bShiftKeyDown = true;
}

void AAuraPlayerController::ShiftReleased()
{
	bShiftKeyDown = false;

	if (bIsSprinting)
	{
		bIsSprinting = false;
		ApplySprintState(false);
		if (!HasAuthority())
		{
			ServerSetSprinting(false);
		}
	}
}

void AAuraPlayerController::ServerSetSprinting_Implementation(bool bShouldSprint)
{
	bIsSprinting = bShouldSprint;
	ApplySprintState(bShouldSprint);
}

void AAuraPlayerController::ServerRequestBroomDismount_Implementation(AAuraBroomVehicle* BroomToDismount)
{
	if (!IsBroomMountedByControlledCharacter(BroomToDismount))
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomDismount ignored: broom is not mounted by this controller. Controller=%s Broom=%s"),
			*GetNameSafe(this), *GetNameSafe(BroomToDismount));
		return;
	}

	UE_LOG(LogAura, Log, TEXT("ServerRequestBroomDismount processing. Controller=%s Broom=%s"),
		*GetNameSafe(this),
		*GetNameSafe(BroomToDismount));

	BroomToDismount->RequestDismount();
}

void AAuraPlayerController::ServerRequestBroomMount_Implementation(AAuraBroomVehicle* BroomToMount)
{
	if (!IsValid(BroomToMount))
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomMount ignored: invalid broom. Controller=%s"), *GetNameSafe(this));
		return;
	}

	ACharacter* ControlledCharacter = GetPawn<ACharacter>();
	if (!IsValid(ControlledCharacter))
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomMount ignored: no character pawn. Controller=%s Broom=%s"),
			*GetNameSafe(this),
			*GetNameSafe(BroomToMount));
		return;
	}

	if (IsValid(BroomToMount->GetMountedCharacter()) && BroomToMount->GetMountedCharacter() != ControlledCharacter)
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomMount ignored: broom already has another rider. Controller=%s Broom=%s Rider=%s"),
			*GetNameSafe(this), *GetNameSafe(BroomToMount), *GetNameSafe(BroomToMount->GetMountedCharacter()));
		return;
	}
	if (!IsBroomWithinMountRange(BroomToMount, ControlledCharacter))
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomMount ignored: broom is out of range. Controller=%s Broom=%s Character=%s"),
			*GetNameSafe(this), *GetNameSafe(BroomToMount), *GetNameSafe(ControlledCharacter));
		return;
	}
	if (AActor* ExistingMount = ControlledCharacter->GetAttachParentActor(); IsValid(ExistingMount) && ExistingMount != BroomToMount)
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomMount ignored: character is already mounted on another actor. Controller=%s Character=%s ExistingMount=%s"),
			*GetNameSafe(this), *GetNameSafe(ControlledCharacter), *GetNameSafe(ExistingMount));
		return;
	}

	UE_LOG(LogAura, Log, TEXT("ServerRequestBroomMount processing. Controller=%s Character=%s Broom=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ControlledCharacter),
		*GetNameSafe(BroomToMount));

	BroomToMount->RequestMount(ControlledCharacter);
}

void AAuraPlayerController::ServerSetBroomYaw_Implementation(AAuraBroomVehicle* Broom, float WorldYaw)
{
	if (!IsBroomMountedByControlledCharacter(Broom) || !FMath::IsFinite(WorldYaw))
	{
		return;
	}
	WorldYaw = FRotator::NormalizeAxis(WorldYaw);

	// Apply the camera yaw immediately so screen-edge / right-mouse rotation feels instant.
	FRotator NewRotation = Broom->GetActorRotation();
	NewRotation.Yaw = WorldYaw;
	Broom->SetActorRotation(NewRotation);

	// Sync the interpolation target so the movement-driven smooth-turn system
	// does not fight against the camera-set rotation.
	Broom->SetFlightTargetYaw(WorldYaw);
}

void AAuraPlayerController::UpdateMountedBroomYaw(float WorldYaw)
{
	ACharacter* ControlledCharacter = GetPawn<ACharacter>();
	if (!IsValid(ControlledCharacter))
	{
		return;
	}

	AAuraBroomVehicle* MountedBroom = Cast<AAuraBroomVehicle>(ControlledCharacter->GetAttachParentActor());
	if (!IsBroomMountedByControlledCharacter(MountedBroom) || !FMath::IsFinite(WorldYaw))
	{
		return;
	}

	if (HasAuthority())
	{
		// Listen-server or standalone: apply directly.
		FRotator NewRotation = MountedBroom->GetActorRotation();
		NewRotation.Yaw = WorldYaw;
		MountedBroom->SetActorRotation(NewRotation);
	}
	else
	{
		// Dedicated-server client: forward to server via unreliable RPC.
		ServerSetBroomYaw(MountedBroom, WorldYaw);
	}
}

void AAuraPlayerController::ServerApplyBroomFlightInput_Implementation(AAuraBroomVehicle* Broom, const FVector& WorldDirection, float ScaleValue)
{
	UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] ServerApplyBroomFlightInput received. Controller=%s Broom=%s BroomValid=%s Dir=%s Scale=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(Broom),
		IsValid(Broom) ? TEXT("yes") : TEXT("NULL"),
		*WorldDirection.ToCompactString(),
		ScaleValue);

	if (!IsBroomMountedByControlledCharacter(Broom)
		|| !FMath::IsFinite(WorldDirection.X) || !FMath::IsFinite(WorldDirection.Y) || !FMath::IsFinite(WorldDirection.Z)
		|| !FMath::IsFinite(ScaleValue))
	{
		return;
	}

	const FVector SafeDirection = WorldDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	Broom->AddFlightInput(SafeDirection, FMath::Clamp(ScaleValue, 0.f, 1.f));
}

bool AAuraPlayerController::IsBroomMountedByControlledCharacter(const AAuraBroomVehicle* Broom) const
{
	const ACharacter* ControlledCharacter = GetPawn<ACharacter>();
	return IsValid(Broom) && IsValid(ControlledCharacter)
		&& Broom->GetMountedCharacter() == ControlledCharacter
		&& ControlledCharacter->GetAttachParentActor() == Broom;
}

bool AAuraPlayerController::IsBroomWithinMountRange(const AAuraBroomVehicle* Broom, const ACharacter* CandidateCharacter) const
{
	constexpr float MountInteractionRange = 300.f;
	return IsValid(Broom) && IsValid(CandidateCharacter)
		&& FVector::DistSquared(Broom->GetActorLocation(), CandidateCharacter->GetActorLocation()) <= FMath::Square(MountInteractionRange);
}

void AAuraPlayerController::ApplySprintState(bool bShouldSprint)
{
	UCharacterMovementComponent* CharacterMovement = GetControlledCharacterMovement();
	if (!IsValid(CharacterMovement))
	{
		return;
	}

	if (CachedWalkSpeed <= 0.f)
	{
		CachedWalkSpeed = CharacterMovement->MaxWalkSpeed;
	}

	const float SprintMultiplier = FMath::Max(1.f, SprintSpeedMultiplier);
	const float TargetSpeed = bShouldSprint ? CachedWalkSpeed * SprintMultiplier : CachedWalkSpeed;
	if (!FMath::IsNearlyEqual(CharacterMovement->MaxWalkSpeed, TargetSpeed))
	{
		CharacterMovement->MaxWalkSpeed = TargetSpeed;
	}
}

UCharacterMovementComponent* AAuraPlayerController::GetControlledCharacterMovement() const
{
	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		return ControlledCharacter->GetCharacterMovement();
	}

	return nullptr;
}

void AAuraPlayerController::WebAbilityInputTagPressed(const FGameplayTag& InputTag)
{
	AbilityInputTagPressed(InputTag);
}

void AAuraPlayerController::WebAbilityInputTagHeld(const FGameplayTag& InputTag)
{
	AbilityInputTagHeld(InputTag);
}

void AAuraPlayerController::WebAbilityInputTagReleased(const FGameplayTag& InputTag)
{
	AbilityInputTagReleased(InputTag);
}

void AAuraPlayerController::RequestFirearmReload()
{
	if (HasAuthority()) ServerRequestFirearmReload_Implementation();
	else ServerRequestFirearmReload();
}

void AAuraPlayerController::ServerRequestFirearmReload_Implementation()
{
	FName ResultCode = TEXT("InvalidState");
	AAuraPlayerState* AuraState = GetPlayerState<AAuraPlayerState>();
	const bool bStarted = AuraState && AuraState->BeginFirearmReload(ResultCode);
	if (bStarted) ResultCode = TEXT("ReloadStarted");
	UE_LOG(LogAura, Display, TEXT("[Firearm][Server] Reload request controller=%s accepted=%d result=%s."),
		*GetNameSafe(this), bStarted ? 1 : 0, *ResultCode.ToString());
	ClientFirearmCommandResult(ResultCode);
}

void AAuraPlayerController::ClientFirearmCommandResult_Implementation(FName ResultCode)
{
	UE_LOG(LogAura, Display, TEXT("[Firearm][Client] CommandResult=%s."), *ResultCode.ToString());
}

void AAuraPlayerController::WebInteractPressed()
{
	InteractPressed();
}

void AAuraPlayerController::ShowLocation()
{
	if (AAuraHUD* AuraHUD = GetHUD<AAuraHUD>())
	{
		AuraHUD->ToggleLocationDisplay();
	}
}

// ---- UGC Building Exec Commands -------------------------------------------

void AAuraPlayerController::StartPlacement(const FString& MeshPath)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UAuraBuildingComponent* BuildComp = ControlledPawn->FindComponentByClass<UAuraBuildingComponent>();
	if (!BuildComp)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("StartPlacement: no BuildingComponent on pawn"));
		return;
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			FString::Printf(TEXT("StartPlacement: cannot load mesh '%s'"), *MeshPath));
		return;
	}

	BuildComp->EnterPlacementMode(Mesh);
}

void AAuraPlayerController::ConfirmPlacement()
{
	if (APawn* ControlledPawn = GetPawn())
	{
		if (UAuraBuildingComponent* BuildComp = ControlledPawn->FindComponentByClass<UAuraBuildingComponent>())
		{
			BuildComp->ConfirmPlacement();
		}
	}
}

void AAuraPlayerController::CancelPlacement()
{
	if (APawn* ControlledPawn = GetPawn())
	{
		if (UAuraBuildingComponent* BuildComp = ControlledPawn->FindComponentByClass<UAuraBuildingComponent>())
		{
			BuildComp->CancelPlacement();
		}
	}
}

void AAuraPlayerController::RotatePlacement(float DeltaYaw)
{
	(void)DeltaYaw;

	if (APawn* ControlledPawn = GetPawn())
	{
		if (UAuraBuildingComponent* BuildComp = ControlledPawn->FindComponentByClass<UAuraBuildingComponent>())
		{
			BuildComp->RotatePlacement(90.f);
		}
	}
}

void AAuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	ExpireFirearmInputLeaseIfNeeded();
	CursorTrace();
	RotateCameraFromMouseDelta();
	RotateCameraFromScreenEdge(DeltaTime);
	AutoRun();
	UpdateMagicCircleLocation();
	ApplyBroomVerticalFlight();
}

void AAuraPlayerController::ExpireFirearmInputLeaseIfNeeded()
{
	if (!bFirearmInputLeaseActive || !GetWorld() || GetWorld()->GetTimeSeconds() < FirearmInputLeaseExpiresAt) return;
	bFirearmInputLeaseActive = false;
	UE_LOG(LogAura, Warning, TEXT("[Firearm][Input] LMB input lease expired; releasing held input controller=%s."), *GetNameSafe(this));
	AbilityInputTagReleased(FAuraGameplayTags::Get().InputTag_LMB);
}

void AAuraPlayerController::ShowMagicCircle(UMaterialInterface* DecalMaterial)
{
	if (!IsValid(MagicCircle))
	{
		MagicCircle = GetWorld()->SpawnActor<AMagicCircle>(MagicCircleClass);
		if (DecalMaterial)
		{
			MagicCircle->MagicCircleDecal->SetMaterial(0, DecalMaterial);
		}
	}
}

void AAuraPlayerController::HideMagicCircle()
{
	if (IsValid(MagicCircle))
	{
		MagicCircle->Destroy();
	}
}

void AAuraPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter, bool bBlockedHit, bool bCriticalHit)
{
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount, bBlockedHit, bCriticalHit);
	}
}

void AAuraPlayerController::AutoRun()
{
	if (!bAutoRunning) return;
	if (APawn* ControlledPawn = GetPawn())
	{
		const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);
		const FVector Direction = Spline->FindDirectionClosestToWorldLocation(LocationOnSpline, ESplineCoordinateSpace::World);
		ControlledPawn->AddMovementInput(Direction);

		const float DistanceToDestination = (LocationOnSpline - CachedDestination).Length();
		if (DistanceToDestination <= AutoRunAcceptanceRadius)
		{
			bAutoRunning = false;
		}
	}
}

void AAuraPlayerController::UpdateMagicCircleLocation()
{
	if (IsValid(MagicCircle))
	{
		MagicCircle->SetActorLocation(CursorHit.ImpactPoint);
	}
}

void AAuraPlayerController::HighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_HighlightActor(InActor);
	}
}

void AAuraPlayerController::UnHighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_UnHighlightActor(InActor);
	}
}

void AAuraPlayerController::CursorTrace()
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_CursorTrace))
	{
		UnHighlightActor(LastActor);
		UnHighlightActor(ThisActor);
		if (IsValid(ThisActor) && ThisActor->Implements<UHighlightInterface>())

		LastActor = nullptr;
		ThisActor = nullptr;
		FocusedTargetDescriptor = FAuraTargetDescriptor();
		SelectedInteractionOptionIndex = INDEX_NONE;
		if (TargetInteractionWidgetController) TargetInteractionWidgetController->ClearPreview();
		return;
	}
	const ECollisionChannel TraceChannel = IsValid(MagicCircle) ? ECC_ExcludePlayers : ECC_Visibility;
	GetHitResultUnderCursor(TraceChannel, false, CursorHit);
	if (!CursorHit.bBlockingHit)
	{
		UnHighlightActor(ThisActor);
		LastActor = ThisActor;
		ThisActor = nullptr;
		FocusedTargetDescriptor = FAuraTargetDescriptor();
		SelectedInteractionOptionIndex = INDEX_NONE;
		if (TargetInteractionWidgetController) TargetInteractionWidgetController->ClearPreview();
		return;
	}

	LastActor = ThisActor;
	if (IsValid(CursorHit.GetActor()) && CursorHit.GetActor()->Implements<UAuraTargetableInterface>())
	{
		ThisActor = CursorHit.GetActor();
	}
	else
	{
		ThisActor = nullptr;
	}

	if (LastActor != ThisActor)
	{
		UnHighlightActor(LastActor);
		HighlightActor(ThisActor);
		SelectedInteractionOptionIndex = INDEX_NONE;
	}
	if (IsValid(ThisActor))
	{
		if (TargetInteractionWidgetController
			&& TargetInteractionWidgetController->BuildPreview(GetPawn(), ThisActor, FocusedTargetDescriptor))
		{
			TargetInteractionWidgetController->PublishPreview(FocusedTargetDescriptor);
		}
		else
		{
			FocusedTargetDescriptor = FAuraTargetDescriptor();
			SelectedInteractionOptionIndex = INDEX_NONE;
			if (TargetInteractionWidgetController) TargetInteractionWidgetController->ClearPreview();
		}
	}
	else
	{
		FocusedTargetDescriptor = FAuraTargetDescriptor();
		SelectedInteractionOptionIndex = INDEX_NONE;
		if (TargetInteractionWidgetController) TargetInteractionWidgetController->ClearPreview();
	}
}

void AAuraPlayerController::InteractPressed()
{
	if (!InteractionComponent || !IsValid(ThisActor)) return;
	const FAuraInteractionOption* Selected = FocusedTargetDescriptor.InteractionOptions.IsValidIndex(SelectedInteractionOptionIndex)
		&& FocusedTargetDescriptor.InteractionOptions[SelectedInteractionOptionIndex].bEnabled
		? &FocusedTargetDescriptor.InteractionOptions[SelectedInteractionOptionIndex]
		: FocusedTargetDescriptor.InteractionOptions.FindByPredicate(
			[](const FAuraInteractionOption& Option) { return Option.bEnabled; });
	if (Selected)
	{
		InteractionComponent->RequestInteraction(ThisActor, Selected->OptionTag, NextInteractionRequestId++);
	}
}

void AAuraPlayerController::SetFocusedInteractionOptionIndex(int32 Index)
{
	SelectedInteractionOptionIndex = FocusedTargetDescriptor.InteractionOptions.IsValidIndex(Index)
		&& FocusedTargetDescriptor.InteractionOptions[Index].bEnabled ? Index : INDEX_NONE;
}

void AAuraPlayerController::InteractionOptionOnePressed()
{
	SetFocusedInteractionOptionIndex(0);
}

void AAuraPlayerController::InteractionOptionTwoPressed()
{
	SetFocusedInteractionOptionIndex(1);
}

void AAuraPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!IsAbilityInputReady())
	{
		UE_LOG(LogAura, Warning, TEXT("[PC] AbilityInputTagPressed BLOCKED: Attributes not ready. Tag=%s"), *InputTag.ToString());
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagPressed BLOCKED by Player_Block_InputPressed: Tag=%s"), *InputTag.ToString());
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Mounted_Broom))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagPressed BLOCKED: Player is mounted on broom. Tag=%s"), *InputTag.ToString());
		return;
	}
	UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagPressed: Tag=%s ASC=%s"), *InputTag.ToString(), GetASC() ? TEXT("valid") : TEXT("null"));
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		return;
	}
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		bFirearmInputLeaseActive = true;
		FirearmInputLeaseExpiresAt = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + FirearmInputLeaseSeconds;
		FollowTime = 0.f;
		if (IsValid(ThisActor))
		{
			FAuraCombatRuleContext PreviewContext;
			const FAuraCombatRuleResult AttackPreview = FAuraCombatRules::CanDamage(GetPawn(), ThisActor, PreviewContext);
			TargetingStatus = AttackPreview.bCanDamage ? ETargetingStatus::TargetingEnemy : ETargetingStatus::TargetingNonEnemy;
		}
		else
		{
			TargetingStatus = ETargetingStatus::NotTargeting;
		}
		UE_LOG(LogAura, Log, TEXT("[PC] LMB Pressed: TargetingStatus=%d ThisActor=%s"),
			(int32)TargetingStatus, *GetNameSafe(ThisActor));
		bAutoRunning = false;
	}
	if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
}

void AAuraPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!IsAbilityInputReady())
	{
		UE_LOG(LogAura, Warning, TEXT("[PC] AbilityInputTagReleased BLOCKED: Attributes not ready. Tag=%s"), *InputTag.ToString());
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputReleased))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagReleased BLOCKED: Tag=%s"), *InputTag.ToString());
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Mounted_Broom))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagReleased BLOCKED: Player is mounted on broom. Tag=%s"), *InputTag.ToString());
		return;
	}
	UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagReleased: Tag=%s TargetingStatus=%d FollowTime=%.3f"),
		*InputTag.ToString(), (int32)TargetingStatus, FollowTime);
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		return;
	}
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
		return;
	}
	bFirearmInputLeaseActive = false;
	FirearmInputLeaseExpiresAt = 0.0;

	const bool bHasLMBAbility = HasEquippedAbilityForInputTag(InputTag);
	if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
	if (bHasLMBAbility)
	{
		UE_LOG(LogAura, Log, TEXT("[PC] LMB Released: ability equipped, skipping click-to-move release behavior"));
		FollowTime = 0.f;
		TargetingStatus = ETargetingStatus::NotTargeting;
		return;
	}

	// Click-to-move is disabled. LMB without an equipped ability no longer moves the character.
	FollowTime = 0.f;
	TargetingStatus = ETargetingStatus::NotTargeting;
}

void AAuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (!IsAbilityInputReady())
	{
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputHeld))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagHeld BLOCKED: Tag=%s"), *InputTag.ToString());
		return;
	}
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Mounted_Broom))
	{
		UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagHeld BLOCKED: Player is mounted on broom. Tag=%s"), *InputTag.ToString());
		return;
	}
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_RMB))
	{
		return;
	}
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
		return;
	}

	// LMB: if targeting an enemy or holding Shift, always try to activate the ability.
	// If not targeting an enemy, also try the ability first — if there is an equipped LMB ability
	// it should still fire (e.g. projectile spells toward cursor).
	if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
	{
		UE_LOG(LogAura, Log, TEXT("[PC] LMB Held: TargetingEnemy or Shift — routing to ASC. TargetingStatus=%d Shift=%s"),
			(int32)TargetingStatus, bShiftKeyDown ? TEXT("true") : TEXT("false"));
		if (GetASC()) GetASC()->AbilityInputTagHeld(InputTag);
	}
	else
	{
		// Check if there is an ability equipped to the LMB slot; if so, activate it.
		const bool bHasLMBAbility = HasEquippedAbilityForInputTag(InputTag);
		if (bHasLMBAbility)
		{
			UE_LOG(LogAura, Log, TEXT("[PC] LMB Held: LMB ability equipped and not targeting enemy — routing to ASC"));
			GetASC()->AbilityInputTagHeld(InputTag);
		}
		else
		{
			// Click-to-move is disabled. LMB without an equipped ability is a no-op for movement.
		}
	}
}

void AAuraPlayerController::AutoTestUseRandomEquippedAbility()
{
	#if UE_BUILD_SHIPPING
	return;
	#else
	if (!IsAbilityInputReady())
	{
		UE_LOG(LogAura, Log, TEXT("[AutoTest] UseRandomAbility: ability input not ready yet."));
		return;
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	// Candidate skill slots: the numbered keys plus LMB. RMB and passive slots are
	// skipped — RMB is a no-op in the press/release path and passives aren't player-fired.
	TArray<FGameplayTag> EquippedTags;
	for (const FGameplayTag Tag : { GameplayTags.InputTag_1, GameplayTags.InputTag_2, GameplayTags.InputTag_3, GameplayTags.InputTag_4, GameplayTags.InputTag_LMB })
	{
		if (Tag.IsValid() && HasEquippedAbilityForInputTag(Tag))
		{
			EquippedTags.Add(Tag);
		}
	}

	if (EquippedTags.Num() == 0)
	{
		UE_LOG(LogAura, Log, TEXT("[AutoTest] UseRandomAbility: no equipped skill slots found."));
		return;
	}

	const FGameplayTag Chosen = EquippedTags[FMath::RandRange(0, EquippedTags.Num() - 1)];
	UE_LOG(LogAura, Log, TEXT("[AutoTest] UseRandomAbility: firing equipped slot %s."), *Chosen.ToString());

	// Mirror a real tap-hold-release. The ASC only calls TryActivateAbility from the HELD
	// path (AbilityInputTagHeld), so a bare press+release never activates the ability —
	// press only marks the spec input-pressed and sets LMB targeting state. We press, then
	// drive one held tick to trigger activation (which runs the targeting task and, for
	// projectile spells, sends cursor target data to the server), then release a short
	// moment later so the server-side activation + target-data RPC have time to complete on
	// a network client before the ability is ended.
	AbilityInputTagPressed(Chosen);
	AbilityInputTagHeld(Chosen);

	if (UWorld* World = GetWorld())
	{
		FTimerHandle ReleaseHandle;
		World->GetTimerManager().SetTimer(ReleaseHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, Chosen]()
			{
				AbilityInputTagReleased(Chosen);
			}),
			0.12f, false);
	}
	else
	{
		AbilityInputTagReleased(Chosen);
	}
	#endif
}

bool AAuraPlayerController::HasEquippedAbilityForInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	UAuraAbilitySystemComponent* ASC = GetASC();
	return ASC && ASC->GetSpecWithSlot(InputTag) != nullptr;
}

UAuraAbilitySystemComponent* AAuraPlayerController::GetASC()
{
	if (AuraAbilitySystemComponent == nullptr)
	{
		AuraAbilitySystemComponent = Cast<UAuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return AuraAbilitySystemComponent;
}

const UAuraAttributeSet* AAuraPlayerController::GetAuraAS() const
{
	if (AuraAttributeSet == nullptr)
	{
		if (UAuraAbilitySystemComponent* ASC = const_cast<AAuraPlayerController*>(this)->GetASC())
		{
			const_cast<AAuraPlayerController*>(this)->AuraAttributeSet = ASC->GetSet<UAuraAttributeSet>();
		}
	}
	return AuraAttributeSet;
}

bool AAuraPlayerController::IsAbilityInputReady() const
{
	const UAuraAbilitySystemComponent* ASC = const_cast<AAuraPlayerController*>(this)->GetASC();
	if (ASC == nullptr)
	{
		return false;
	}

	// Let GAS handle cooldown/cost validity. Do not hard-block startup input on attribute replication.
	return IsValid(GetPawn());
}

void AAuraPlayerController::ClientRejectLogin_Implementation(const FString& Reason)
{
	UE_LOG(LogAura, Error, TEXT("[Role][Login] ClientRejectLogin: %s"), *Reason);

	// Reuse the same server-lost path as a mid-game server loss: stash the reason as
	// the Login-screen alert message and travel back to the Login level (dropping the connection).
	if (ClientDisconnectHandler)
	{
		ClientDisconnectHandler->RequestServerLost(Reason);
	}
	else
	{
		ClientTravel(UServerTravelComponent::LoginLevelPath, TRAVEL_Absolute);
	}
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && InteractionComponent && GetWorld())
	{
		if (UAuraCommerceSubsystem* Commerce = GetWorld()->GetSubsystem<UAuraCommerceSubsystem>())
		{
			const FGuid SessionNonce = Commerce->RotateSession(this);
			InteractionComponent->InitializeCommerceSessionNonce(SessionNonce);
			UE_LOG(LogAura, Display, TEXT("[Commerce][Session] Rotated player=%s nonce=%s."), *GetNameSafe(this), *SessionNonce.ToString());
		}
	}
	if (!TargetInteractionWidgetController)
	{
		TargetInteractionWidgetController = NewObject<UTargetInteractionWidgetController>(this);
	}
	if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AuraRoleBattleDay1SmokeTest")))
	{
		bRoleBattleDay1SmokeEnabled = true;
		GetWorldTimerManager().SetTimer(
			RoleBattleDay1SmokeTimerHandle,
			this,
			&ThisClass::TickRoleBattleDay1Smoke,
			0.25f,
			true);
		UE_LOG(LogAura, Display, TEXT("[Day1Smoke] Scheduled role/respawn smoke test."));
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay6NetworkProbe")))
	{
		bRoleBattleDay6ProbeEnabled = true;
		GetWorldTimerManager().SetTimer(
			RoleBattleDay6ProbeTimerHandle,
			this,
			&ThisClass::TickRoleBattleDay6NetworkProbe,
			0.25f,
			true);
		UE_LOG(LogAura, Display, TEXT("[Day6NetworkProbe][%s] Scheduled Controller=%s"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"), *GetNameSafe(this));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay17NetworkProbeClient")) && !HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RoleBattleDay17ProbeTimerHandle, this,
			&ThisClass::TickRoleBattleDay17NetworkProbeClient, 0.25f, true);
		UE_LOG(LogAura, Display, TEXT("[Day17NetworkProbe][Client] Scheduled merchant purchase/replay probe."));
	}
	if ((FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceProbeClientA"))
		|| FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceProbeClientB"))) && !HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RoleBattleDay18ProbeTimerHandle, this,
			&ThisClass::TickRoleBattleDay18PersistenceProbeClient, 0.25f, true);
		UE_LOG(LogAura, Display, TEXT("[Day18PersistenceProbe][Client] Scheduled identity-isolated purchase probe."));
	}
	if ((FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbeClientA"))
		|| FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbeClientB"))) && !HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RoleBattleDay19ProbeTimerHandle, this,
			&ThisClass::TickRoleBattleDay19NetworkProbeClient, 0.25f, true);
		UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] Scheduled security/replay probe."));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("CrunchComboNetworkProbe")))
	{
		bCrunchComboNetworkProbeEnabled = true;
		FString ProbeScenario;
		FParse::Value(FCommandLine::Get(), TEXT("CrunchComboNetworkProbeScenario="), ProbeScenario);
		bCrunchComboNetworkProbeCancelScenario = ProbeScenario.Equals(TEXT("Cancel"), ESearchCase::IgnoreCase);
		bCrunchComboNetworkProbeBeforeCloseScenario = ProbeScenario.Equals(TEXT("BeforeClose"), ESearchCase::IgnoreCase);
		bCrunchComboNetworkProbeAtOrAfterCloseScenario = ProbeScenario.Equals(TEXT("AtOrAfterClose"), ESearchCase::IgnoreCase);
		bCrunchComboNetworkProbePreserveMovement = FParse::Param(FCommandLine::Get(), TEXT("CrunchComboNetworkProbePreserveMovement"));
		bCrunchComboNetworkProbeOffscreenAuthority = FParse::Param(FCommandLine::Get(), TEXT("CrunchComboNetworkProbeOffscreenAuthority"));
		// BeforeClose follows the normal authored-window press path (each press
		// is sent while open). AtOrAfterClose uses the late-boundary branch below.
		bCrunchComboNetworkProbeNearCloseScenario = bCrunchComboNetworkProbeAtOrAfterCloseScenario;
		CrunchComboNetworkProbeClientNextActionTime = 0.0;
		GetWorldTimerManager().SetTimer(
			CrunchComboNetworkProbeTimerHandle,
			this,
			&ThisClass::TickCrunchComboNetworkProbe,
			0.05f,
			true);
		UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][%s] Scheduled test-only combo probe."),
			HasAuthority() ? TEXT("Server") : TEXT("Client"));
		UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][%s] ProbeConfig MovementReplication=%d OffscreenAuthority=%d"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"),
			bCrunchComboNetworkProbePreserveMovement ? 1 : 0,
			bCrunchComboNetworkProbeOffscreenAuthority ? 1 : 0);
	}
#endif
	if (!AuraContext)
	{
		UE_LOG(LogAura, Warning, TEXT("AAuraPlayerController::BeginPlay: AuraContext is not set; using native input fallback where available."));
	}
	if (!InteractAction)
	{
		InteractAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprints/Input/InputActions/IA_Interact.IA_Interact"));
	}

	if (UCharacterMovementComponent* CharacterMovement = GetControlledCharacterMovement())
	{
		CachedWalkSpeed = CharacterMovement->MaxWalkSpeed;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem && AuraContext)
	{
		if (InteractAction)
		{
			bool bMapped = false;
			for (const FEnhancedActionKeyMapping& Mapping : AuraContext->GetMappings())
			{
				if (Mapping.Action == InteractAction && Mapping.Key == EKeys::F) { bMapped = true; break; }
			}
			if (!bMapped) AuraContext->MapKey(InteractAction, EKeys::F);
		}
		Subsystem->AddMappingContext(AuraContext, 0);
	}

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	ApplyGameAndUIInputMode();

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (USpringArmComponent* CameraBoom = ControlledPawn->FindComponentByClass<USpringArmComponent>())
		{
			SetControlRotation(FRotator(0.f, CameraBoom->GetComponentRotation().Yaw, 0.f));
		}
	}

	// Ensure keyboard focus is on the game viewport immediately so WASD works on spawn.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AAuraPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RoleBattleDay17ProbeTimerHandle);
		GetWorldTimerManager().ClearTimer(RoleBattleDay18ProbeTimerHandle);
		GetWorldTimerManager().ClearTimer(RoleBattleDay19ProbeTimerHandle);
		GetWorldTimerManager().ClearTimer(CrunchComboNetworkProbeTimerHandle);
	}
	CrunchComboNetworkProbeConfiguredPawn.Reset();
	CrunchComboNetworkProbeServerLastMovementStateSection = -1;
	if (HasAuthority() && GetWorld())
	{
		if (AAuraPlayerState* AuraState = GetPlayerState<AAuraPlayerState>()) AuraState->CancelFirearmReload(TEXT("ControllerEndPlay"));
		if (UAuraCommerceSubsystem* Commerce = GetWorld()->GetSubsystem<UAuraCommerceSubsystem>()) Commerce->InvalidateSession(this);
	}
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
void AAuraPlayerController::TickRoleBattleDay17NetworkProbeClient()
{
	if (HasAuthority() || bRoleBattleDay17ProbeSubmitted || !InteractionComponent
		|| !InteractionComponent->GetCommerceSessionNonce().IsValid()) return;
	for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
	{
		AAuraCivilian* Civilian = *It;
		UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		if (!Merchant || !Merchant->IsMerchantActive()) continue;
		const FAuraMerchantOfferPresentation* ProbeOffer = Merchant->GetPresentation().Offers.FindByPredicate(
			[](const FAuraMerchantOfferPresentation& Offer) { return Offer.OfferId == FName(TEXT("market_health_potion")); });
		// Both clients must enter the same contention window. If the other client
		// wins before this replica receives the fixture update, CurrentStock is
		// already zero; that client still needs to submit so the server returns a
		// typed SoldOut result instead of leaving the probe one-sided.
		if (!ProbeOffer || ProbeOffer->CurrentStock > 1 || Merchant->GetPresentation().StockRevision < 2) continue;
		InteractionComponent->RequestPurchase(Civilian, FName(TEXT("market_health_potion")));
		InteractionComponent->RequestPurchaseWithRequestIdForDevelopmentProbe(Civilian, FName(TEXT("market_health_potion")), 1);
		bRoleBattleDay17ProbeSubmitted = true;
		GetWorldTimerManager().ClearTimer(RoleBattleDay17ProbeTimerHandle);
		UE_LOG(LogAura, Display, TEXT("[Day17NetworkProbe][Client] Submitted request=1 and exact replay merchant=%s."), *GetNameSafe(Civilian));
		return;
	}
}

void AAuraPlayerController::TickRoleBattleDay18PersistenceProbeClient()
{
	if (HasAuthority() || !InteractionComponent || !InteractionComponent->GetCommerceSessionNonce().IsValid()) return;
	const bool bProfileA = FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceProbeClientA"));
	const bool bStaleOnly = FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceStaleOnly"));
	if (!bProfileA && !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceProbeClientB"))) return;
	for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
	{
		AAuraCivilian* Civilian = *It;
		UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		if (!Merchant || !Merchant->IsMerchantActive() || !Merchant->GetOwner()) continue;
		if (!GetPawn() || FVector::DistSquared(GetPawn()->GetActorLocation(), Merchant->GetOwner()->GetActorLocation()) > FMath::Square(240.f)) continue;
		if (bStaleOnly)
		{
			if (bRoleBattleDay18StaleProbeSubmitted) return;
			FString StaleNonceText;
			if (!FParse::Value(FCommandLine::Get(), TEXT("AuraRoleBattleDay18StaleNonce="), StaleNonceText)) return;
			FGuid StaleNonce;
			if (!FGuid::Parse(StaleNonceText, StaleNonce) || !StaleNonce.IsValid()) return;
			InteractionComponent->RequestPurchaseWithSessionNonceForDevelopmentProbe(
				Civilian, FName(TEXT("market_health_potion")), StaleNonce, 999);
			bRoleBattleDay18StaleProbeSubmitted = true;
			GetWorldTimerManager().ClearTimer(RoleBattleDay18ProbeTimerHandle);
			UE_LOG(LogAura, Display, TEXT("[Day18PersistenceProbe][Client] Submitted stale nonce request=999 merchant=%s."), *GetNameSafe(Civilian));
			return;
		}
		if (bRoleBattleDay18ProbeSubmitted) return;
		const FName OfferId = bProfileA ? FName(TEXT("market_health_potion")) : FName(TEXT("market_mana_potion"));
		if (!Merchant->GetPresentation().Offers.ContainsByPredicate([OfferId](const FAuraMerchantOfferPresentation& Offer) { return Offer.OfferId == OfferId; })) continue;
		InteractionComponent->RequestPurchase(Civilian, OfferId);
		bRoleBattleDay18ProbeSubmitted = true;
		GetWorldTimerManager().ClearTimer(RoleBattleDay18ProbeTimerHandle);
		UE_LOG(LogAura, Display, TEXT("[Day18PersistenceProbe][Client] Submitted profile=%s offer=%s merchant=%s."),
			bProfileA ? TEXT("A") : TEXT("B"), *OfferId.ToString(), *GetNameSafe(Civilian));
		return;
	}
}

void AAuraPlayerController::TickRoleBattleDay19NetworkProbeClient()
{
	if (HasAuthority() || !InteractionComponent || !InteractionComponent->GetCommerceSessionNonce().IsValid()) return;
	if (!FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbeClientA"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay19MultiplayerProbeClientB"))) return;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now < RoleBattleDay19NextProbeTime) return;
	for (TActorIterator<AAuraCivilian> It(GetWorld()); It; ++It)
	{
		AAuraCivilian* Civilian = *It;
		UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		if (!Merchant || !Merchant->IsMerchantActive() || !Merchant->GetOwner() || !GetPawn()
			|| FVector::DistSquared(GetPawn()->GetActorLocation(), Merchant->GetOwner()->GetActorLocation()) > FMath::Square(320.f)) continue;
		const FName OfferId = FName(TEXT("market_health_potion"));
		switch (RoleBattleDay19ProbeStep)
		{
		case 0:
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] LateJoinObserved=1 merchantAvailable=%d stockRevision=%u."),
				Merchant->GetPresentation().bAvailable ? 1 : 0, Merchant->GetPresentation().StockRevision);
			InteractionComponent->RequestPurchaseWithRequestIdForDevelopmentProbe(Civilian, OfferId, 1);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] SentValidRequest=1 offer=%s."), *OfferId.ToString());
			break;
		case 1:
			InteractionComponent->RequestPurchaseWithRequestIdForDevelopmentProbe(Civilian, OfferId, 1);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] SentExactReplay=1."));
			break;
		case 2:
			InteractionComponent->RequestPurchaseWithRequestIdForDevelopmentProbe(Civilian, OfferId, 3);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] SentRequestGap=3."));
			break;
		case 3:
			InteractionComponent->RequestPurchaseWithRequestIdForDevelopmentProbe(Civilian, OfferId, 2);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] SentOrderedRequest=2."));
			break;
		case 4:
			InteractionComponent->RequestPurchaseWithSessionNonceForDevelopmentProbe(Civilian, OfferId, FGuid::NewGuid(), 4);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] SentWrongNonce=4."));
			break;
		default:
			GetWorldTimerManager().ClearTimer(RoleBattleDay19ProbeTimerHandle);
			UE_LOG(LogAura, Display, TEXT("[Day19NetworkProbe][Client] ProbeSequenceComplete=1."));
			return;
		}
		++RoleBattleDay19ProbeStep;
		RoleBattleDay19NextProbeTime = Now + 1.0;
		return;
	}
}

void AAuraPlayerController::TickCrunchComboNetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!bCrunchComboNetworkProbeEnabled || !GetWorld()) return;
	AAuraCharacter* PlayerCharacter = GetPawn<AAuraCharacter>();
	UAuraAbilitySystemComponent* ASC = GetASC();
	if (!PlayerCharacter || !ASC || !IsAbilityInputReady()) return;
	auto LogListenMovementState = [&](const TCHAR* Phase, int32 Section)
	{
		if (!HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer) return;
		UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
		USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh();
		UE_LOG(LogAura, Display,
			TEXT("[CrunchComboNetworkProbe][Server] MovementState Phase=%s Section=%d Replicate=%d MovementTick=%d MeshTick=%d AutonomousPose=%d VisibilityTick=%d LocalRole=%d RemoteRole=%d"),
			Phase, Section,
			PlayerCharacter->IsReplicatingMovement() ? 1 : 0,
			Movement && Movement->IsComponentTickEnabled() ? 1 : 0,
			Mesh && Mesh->IsComponentTickEnabled() ? 1 : 0,
			Mesh && Mesh->bOnlyAllowAutonomousTickPose ? 1 : 0,
			Mesh ? static_cast<int32>(Mesh->VisibilityBasedAnimTickOption) : -1,
			static_cast<int32>(PlayerCharacter->GetLocalRole()), static_cast<int32>(PlayerCharacter->GetRemoteRole()));
	};
	// A headless listen host can leave the skeletal mesh dormant even though the
	// owning client is connected. Preserve mode stays on the engine's
	// CharacterMovement-owned authority pose path; timing isolation uses an
	// explicit regular mesh clock. Neither mode synthesizes gameplay events.
	if (HasAuthority() && GetWorld()->GetNetMode() == NM_ListenServer)
	{
		const bool bPreserveMovement = bCrunchComboNetworkProbePreserveMovement;
		if (USkeletalMeshComponent* ProbeMesh = PlayerCharacter->GetMesh())
		{
			// Both probe modes keep the mesh available to the animation system and
			// use the not-rendered-safe tick policy. Timing isolation owns a regular
			// mesh pose clock; preserve mode leaves pose ownership with the native
			// CharacterMovement/autonomous path so movement replication is exercised
			// without a second competing montage clock.
			ProbeMesh->SetComponentTickEnabled(true);
			ProbeMesh->SetVisibility(true, true);
			ProbeMesh->SetRenderInMainPass(!bCrunchComboNetworkProbeOffscreenAuthority);
			ProbeMesh->bOnlyAllowAutonomousTickPose = bPreserveMovement;
			ProbeMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			// The listen host can receive the replicated ability before the remote
			// pawn has initialized its animation blueprint. Request one explicit
			// initialization attempt so PlayMontageAndWait can bind to the same
			// authored montage instance that the client is already driving.
			if (ProbeMesh->GetSkeletalMeshAsset() && !ProbeMesh->GetAnimInstance())
			{
				ProbeMesh->InitAnim(true);
			}
		}
		if (UCharacterMovementComponent* ProbeMovement = PlayerCharacter->GetCharacterMovement())
		{
			// Timing isolation pauses movement simulation. Preserve mode keeps the
			// native movement/network tick enabled and only clears any incidental
			// velocity/forces; the harness supplies no movement input.
			ProbeMovement->SetComponentTickEnabled(bPreserveMovement);
			if (bPreserveMovement)
			{
				ProbeMovement->StopMovementImmediately();
				ProbeMovement->ClearAccumulatedForces();
			}
		}
		// Configure the actual possessed pawn once. BeginPlay can schedule this
		// controller before possession swaps the transient pawn, so a controller-
		// wide bool would leave the real probe pawn unconfigured.
		if (CrunchComboNetworkProbeConfiguredPawn.Get() != PlayerCharacter)
		{
			PlayerCharacter->SetReplicateMovement(bPreserveMovement);
			CrunchComboNetworkProbeConfiguredPawn = PlayerCharacter;
			CrunchComboNetworkProbeServerLastMovementStateSection = -1;
			LogListenMovementState(TEXT("Config"), -1);
		}
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Melee.CrunchCombo"), false);
	FGameplayAbilitySpec* ComboSpec = ASC->GetSpecFromAbilityTag(ComboTag);

	if (HasAuthority())
	{
		if (!ComboSpec)
		{
			// This is a process-local development probe. Remove the configured LMB
			// slot only for this run, then grant the combo without touching RoleConfig.
			ASC->ClearAbilitiesOfSlot(GameplayTags.InputTag_LMB);
			FGameplayAbilitySpec ProbeSpec(UAuraMeleeAttack::StaticClass(), 1);
			ProbeSpec.GetDynamicSpecSourceTags().AddTag(GameplayTags.InputTag_LMB);
			ProbeSpec.GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_Equipped);
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(ProbeSpec);
			ComboSpec = Handle.IsValid() ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Server] GrantedTestAbility=%d HandleValid=%d RoleConfigUnchanged=1"),
				ComboSpec ? 1 : 0, Handle.IsValid() ? 1 : 0);
		}
		if (!ComboSpec) return;
		UAuraMeleeAttack* AbilityInstance = Cast<UAuraMeleeAttack>(ComboSpec->GetPrimaryInstance());
		if (!AbilityInstance) return;
		if (ComboSpec->IsActive() && !bCrunchComboNetworkProbeServerWasActive)
		{
			bCrunchComboNetworkProbeServerWasActive = true;
			CrunchComboNetworkProbeServerLastMovementStateSection = -1;
			++CrunchComboNetworkProbeServerActivationCount;
			LogListenMovementState(TEXT("AbilityStart"), -1);
			if (!bCrunchComboNetworkProbeServerObservedActivation)
			{
				bCrunchComboNetworkProbeServerObservedActivation = true;
				UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Server] RemoteActivationObserved=1 EventSource=%s FallbackActive=%d"),
					AbilityInstance->GetTestEventSourceName(), AbilityInstance->IsTestAuthorityFallbackTimelineActive() ? 1 : 0);
			}
		}
		const int32 OpenSectionCount = AbilityInstance->GetTestOpenEventCount();
		if (ComboSpec->IsActive() && OpenSectionCount > 0
			&& OpenSectionCount > CrunchComboNetworkProbeServerLastMovementStateSection)
		{
			LogListenMovementState(TEXT("SectionBoundary"), OpenSectionCount - 1);
			CrunchComboNetworkProbeServerLastMovementStateSection = OpenSectionCount;
		}
		if (!ComboSpec->IsActive())
		{
			bCrunchComboNetworkProbeServerWasActive = false;
		}
		if (bCrunchComboNetworkProbeCancelScenario
			&& bCrunchComboNetworkProbeServerObservedActivation
			&& !ComboSpec->IsActive()
			&& CrunchComboNetworkProbeServerActivationCount == 1
			&& !bCrunchComboNetworkProbeServerCancelObserved)
		{
			bCrunchComboNetworkProbeServerCancelObserved = true;
			const bool bCleanupComplete = !ComboSpec->IsActive()
				&& !AbilityInstance->IsTestComboWindowOpen()
				&& !AbilityInstance->IsTestAuthorityFallbackTimelineActive()
				&& !AbilityInstance->HasTestAuthorityFallbackTimerPending()
				&& !AbilityInstance->HasTestInputPressTask()
				&& !AbilityInstance->HasTestQueuedSuccessor();
			UE_LOG(LogAura, Display,
				TEXT("[CrunchComboNetworkProbe][Server] CANCEL_OBSERVED Open=%d Damage=%d Close=%d ImplicitClose=%d Accepted=%d Mask=0x%X WindowOpen=%d FallbackActive=%d FallbackTimers=%d InputTask=%d Queued=%d Cleanup=%d"),
				AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestDamageEventCount(),
				AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(), AbilityInstance->GetTestAcceptedDamageCount(),
				AbilityInstance->GetTestAcceptedDamageSectionMask(), AbilityInstance->IsTestComboWindowOpen() ? 1 : 0,
				AbilityInstance->IsTestAuthorityFallbackTimelineActive() ? 1 : 0,
				AbilityInstance->HasTestAuthorityFallbackTimerPending() ? 1 : 0,
				AbilityInstance->HasTestInputPressTask() ? 1 : 0,
				AbilityInstance->HasTestQueuedSuccessor() ? 1 : 0, bCleanupComplete ? 1 : 0);
		}
		if (bCrunchComboNetworkProbeNearCloseScenario
			&& bCrunchComboNetworkProbeServerObservedActivation
			&& !ComboSpec->IsActive()
			&& !bCrunchComboNetworkProbeServerNearCloseOutcomeLogged)
		{
			bCrunchComboNetworkProbeServerNearCloseOutcomeLogged = true;
			if (AbilityInstance->GetTestOpenEventCount() == 4
				&& AbilityInstance->GetTestDamageEventCount() == 4
				&& AbilityInstance->GetTestCloseEventCount() == 4
				&& AbilityInstance->GetTestAcceptedDamageCount() == 4
				&& AbilityInstance->GetTestAcceptedDamageSectionMask() == 0xF)
			{
				const bool bCleanupComplete = !AbilityInstance->IsTestComboWindowOpen()
					&& !AbilityInstance->IsTestAuthorityFallbackTimelineActive()
					&& !AbilityInstance->HasTestAuthorityFallbackTimerPending()
					&& !AbilityInstance->HasTestInputPressTask()
					&& !AbilityInstance->HasTestQueuedSuccessor();
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Server] NEARCLOSE_ACCEPTED Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF WindowOpen=%d FallbackActive=%d FallbackTimers=%d InputTask=%d Queued=%d Cleanup=%d"),
					AbilityInstance->IsTestComboWindowOpen() ? 1 : 0,
					AbilityInstance->IsTestAuthorityFallbackTimelineActive() ? 1 : 0,
					AbilityInstance->HasTestAuthorityFallbackTimerPending() ? 1 : 0,
					AbilityInstance->HasTestInputPressTask() ? 1 : 0,
					AbilityInstance->HasTestQueuedSuccessor() ? 1 : 0, bCleanupComplete ? 1 : 0);
			}
			else
			{
				const bool bCleanupComplete = !AbilityInstance->IsTestComboWindowOpen()
					&& !AbilityInstance->IsTestAuthorityFallbackTimelineActive()
					&& !AbilityInstance->HasTestAuthorityFallbackTimerPending()
					&& !AbilityInstance->HasTestInputPressTask()
					&& !AbilityInstance->HasTestQueuedSuccessor();
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Server] NEARCLOSE_REJECTED Inactive=1 Open=%d Damage=%d Close=%d ImplicitClose=%d Accepted=%d Mask=0x%X WindowOpen=%d FallbackActive=%d FallbackTimers=%d InputTask=%d Queued=%d Cleanup=%d"),
					AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestDamageEventCount(),
					AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(), AbilityInstance->GetTestAcceptedDamageCount(),
					AbilityInstance->GetTestAcceptedDamageSectionMask(), AbilityInstance->IsTestComboWindowOpen() ? 1 : 0,
					AbilityInstance->IsTestAuthorityFallbackTimelineActive() ? 1 : 0,
					AbilityInstance->HasTestAuthorityFallbackTimerPending() ? 1 : 0,
					AbilityInstance->HasTestInputPressTask() ? 1 : 0,
					AbilityInstance->HasTestQueuedSuccessor() ? 1 : 0, bCleanupComplete ? 1 : 0);
			}
		}
		if (!bCrunchComboNetworkProbeServerPassed
			&& !ComboSpec->IsActive()
			&& AbilityInstance->GetTestOpenEventCount() == 4
			&& AbilityInstance->GetTestDamageEventCount() == 4
			&& AbilityInstance->GetTestCloseEventCount() == 4
			&& AbilityInstance->GetTestAcceptedDamageCount() == 4
			&& AbilityInstance->GetTestAcceptedDamageSectionMask() == 0xF
			&& (!bCrunchComboNetworkProbeCancelScenario || bCrunchComboNetworkProbeServerCancelObserved))
		{
			const bool bCleanupComplete = !AbilityInstance->IsTestComboWindowOpen()
				&& !AbilityInstance->IsTestAuthorityFallbackTimelineActive()
				&& !AbilityInstance->HasTestAuthorityFallbackTimerPending()
				&& !AbilityInstance->HasTestInputPressTask()
				&& !AbilityInstance->HasTestQueuedSuccessor();
			if (!bCleanupComplete)
			{
				return;
			}
			bCrunchComboNetworkProbeServerPassed = true;
			if (bCrunchComboNetworkProbeCancelScenario)
			{
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Server] CANCEL_PASS Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF AuthorityDamage=1 Cleanup=1"));
			}
			else
			{
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Server] PASS Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF AuthorityDamage=1 Cleanup=1"));
			}
			if (!bCrunchComboNetworkProbeNearCloseScenario)
			{
				bCrunchComboNetworkProbeEnabled = false;
				GetWorldTimerManager().ClearTimer(CrunchComboNetworkProbeTimerHandle);
			}
		}
		return;
	}

	if (!ComboSpec) return;
	UAuraMeleeAttack* AbilityInstance = Cast<UAuraMeleeAttack>(ComboSpec->GetPrimaryInstance());
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bCrunchComboNetworkProbeClientActivated)
	{
		FGameplayAbilitySpec* EquippedLmbSpec = ASC->GetSpecWithSlot(GameplayTags.InputTag_LMB);
		if (!EquippedLmbSpec || !EquippedLmbSpec->Ability
			|| !EquippedLmbSpec->Ability->IsA(UAuraMeleeAttack::StaticClass()))
		{
			// Wait until the server's test-only slot replacement has replicated;
			// pressing during this handoff would still activate FireBolt locally.
			return;
		}
		if (ComboSpec->IsActive()) return;
		ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
		ASC->AbilityInputTagHeld(GameplayTags.InputTag_LMB);
		bCrunchComboNetworkProbeClientActivated = true;
		UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] InitialLMBPressed=1"));
		return;
	}
	if (!AbilityInstance) return;
	if (bCrunchComboNetworkProbeCancelScenario)
	{
		if (!bCrunchComboNetworkProbeClientCancelQueued
			&& !bCrunchComboNetworkProbeClientCancelRequested
			&& !bCrunchComboNetworkProbeClientCancelObserved
			&& AbilityInstance->IsTestComboWindowOpen()
			&& AbilityInstance->GetTestOpenEventCount() > CrunchComboNetworkProbeLastOpenCount)
		{
			CrunchComboNetworkProbeLastOpenCount = AbilityInstance->GetTestOpenEventCount();
			ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
			bCrunchComboNetworkProbeClientCancelQueued = true;
			CrunchComboNetworkProbeClientNextActionTime = Now + 0.03;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] CancelQueued=1 Open=%d Section=%d"),
				CrunchComboNetworkProbeLastOpenCount, AbilityInstance->GetTestComboIndex());
			return;
		}
		if (bCrunchComboNetworkProbeClientCancelQueued
			&& !bCrunchComboNetworkProbeClientCancelRequested
			&& Now >= CrunchComboNetworkProbeClientNextActionTime)
		{
			ASC->CancelAbilityHandle(ComboSpec->Handle);
			bCrunchComboNetworkProbeClientCancelRequested = true;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] CancelRequested=1"));
			return;
		}
		if (bCrunchComboNetworkProbeClientCancelRequested
			&& !bCrunchComboNetworkProbeClientCancelObserved
			&& !ComboSpec->IsActive())
		{
			bCrunchComboNetworkProbeClientCancelObserved = true;
			const bool bCleanupComplete = !ComboSpec->IsActive()
				&& !AbilityInstance->IsTestComboWindowOpen()
				&& !AbilityInstance->HasTestInputPressTask();
			CrunchComboNetworkProbeClientNextActionTime = Now + 0.75;
			UE_LOG(LogAura, Display,
				TEXT("[CrunchComboNetworkProbe][Client] CANCEL_OBSERVED Inactive=1 Open=%d Damage=%d Close=%d ImplicitClose=%d Accepted=%d Mask=0x%X Cleanup=%d"),
				AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestDamageEventCount(),
				AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(), AbilityInstance->GetTestAcceptedDamageCount(),
				AbilityInstance->GetTestAcceptedDamageSectionMask(), bCleanupComplete ? 1 : 0);
			return;
		}
		if (bCrunchComboNetworkProbeClientCancelObserved
			&& !bCrunchComboNetworkProbeClientReactivated
			&& !ComboSpec->IsActive()
			&& Now >= CrunchComboNetworkProbeClientNextActionTime)
		{
			CrunchComboNetworkProbeLastOpenCount = 0;
			CrunchComboNetworkProbePresses = 0;
			ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
			ASC->AbilityInputTagHeld(GameplayTags.InputTag_LMB);
			bCrunchComboNetworkProbeClientReactivated = true;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] ReactivationLMBPressed=1"));
			return;
		}
		if (!bCrunchComboNetworkProbeClientReactivated)
		{
			return;
		}
	}
	if (bCrunchComboNetworkProbeNearCloseScenario)
	{
		if (!bCrunchComboNetworkProbeClientNearClosePrimed
			&& AbilityInstance->IsTestComboWindowOpen()
			&& AbilityInstance->GetTestOpenEventCount() > CrunchComboNetworkProbeLastOpenCount)
		{
			CrunchComboNetworkProbeLastOpenCount = AbilityInstance->GetTestOpenEventCount();
			ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
			++CrunchComboNetworkProbePresses;
			bCrunchComboNetworkProbeClientNearClosePrimed = true;
			CrunchComboNetworkProbeClientNextActionTime = Now + 60.0;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] NearClosePrime=1 Open=%d Section=%d"),
				CrunchComboNetworkProbeLastOpenCount, AbilityInstance->GetTestComboIndex());
			return;
		}
		if (bCrunchComboNetworkProbeClientNearClosePrimed
			&& !bCrunchComboNetworkProbeClientNearClosePressed
			&& AbilityInstance->IsTestComboWindowOpen()
			&& AbilityInstance->GetTestOpenEventCount() > CrunchComboNetworkProbeLastOpenCount)
		{
			CrunchComboNetworkProbeLastOpenCount = AbilityInstance->GetTestOpenEventCount();
			CrunchComboNetworkProbeClientOpenTime = Now;
			// BeforeClose sends shortly after Open; AtOrAfterClose waits beyond the
			// authored close boundary so the server must reject the late press.
			CrunchComboNetworkProbeClientNextActionTime = Now
				+ (bCrunchComboNetworkProbeAtOrAfterCloseScenario ? 0.25 : 0.05);
			return;
		}
		if (!bCrunchComboNetworkProbeClientNearClosePressed
			&& CrunchComboNetworkProbeLastOpenCount > 1
			&& Now >= CrunchComboNetworkProbeClientNextActionTime)
		{
			if (bCrunchComboNetworkProbeBeforeCloseScenario && !AbilityInstance->IsTestComboWindowOpen())
			{
				return;
			}
			ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
			++CrunchComboNetworkProbePresses;
			bCrunchComboNetworkProbeClientNearClosePressed = true;
			UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] NearClosePress=1 Open=%d Section=%d OffsetFromOpen=%.3f"),
				CrunchComboNetworkProbeLastOpenCount, AbilityInstance->GetTestComboIndex(),
				Now - CrunchComboNetworkProbeClientOpenTime);
			return;
		}
		if (!ComboSpec->IsActive() && !bCrunchComboNetworkProbeClientNearCloseOutcomeLogged)
		{
			bCrunchComboNetworkProbeClientNearCloseOutcomeLogged = true;
			const bool bCleanupComplete = !AbilityInstance->IsTestComboWindowOpen()
				&& !AbilityInstance->HasTestInputPressTask();
			if (AbilityInstance->GetTestOpenEventCount() == 4)
			{
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Client] NEARCLOSE_ACCEPTED Inactive=1 Open=4 Damage=0 Close=4 ImplicitClose=0 Accepted=0 Mask=0x0 Presses=%d Cleanup=%d"),
					CrunchComboNetworkProbePresses, bCleanupComplete ? 1 : 0);
			}
			else
			{
				UE_LOG(LogAura, Display,
					TEXT("[CrunchComboNetworkProbe][Client] NEARCLOSE_CONVERGED Inactive=1 Open=%d Damage=0 Close=%d ImplicitClose=%d Accepted=0 Mask=0x0 Presses=%d Cleanup=%d"),
					AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(),
					CrunchComboNetworkProbePresses, bCleanupComplete ? 1 : 0);
			}
			bCrunchComboNetworkProbeEnabled = false;
			GetWorldTimerManager().ClearTimer(CrunchComboNetworkProbeTimerHandle);
		}
		return;
	}
	if (AbilityInstance->IsTestComboWindowOpen()
		&& AbilityInstance->GetTestOpenEventCount() > CrunchComboNetworkProbeLastOpenCount
		&& AbilityInstance->GetTestComboIndex() < 3)
	{
		CrunchComboNetworkProbeLastOpenCount = AbilityInstance->GetTestOpenEventCount();
		ASC->AbilityInputTagPressed(GameplayTags.InputTag_LMB);
		++CrunchComboNetworkProbePresses;
		UE_LOG(LogAura, Display, TEXT("[CrunchComboNetworkProbe][Client] ComboPress=%d Open=%d Section=%d"),
			CrunchComboNetworkProbePresses, CrunchComboNetworkProbeLastOpenCount, AbilityInstance->GetTestComboIndex());
		return;
	}
	if (!ComboSpec->IsActive() && AbilityInstance->GetTestOpenEventCount() == 4)
	{
		const bool bCleanupComplete = !AbilityInstance->IsTestComboWindowOpen()
			&& !AbilityInstance->HasTestInputPressTask();
		if (bCrunchComboNetworkProbeCancelScenario)
		{
			UE_LOG(LogAura, Display,
				TEXT("[CrunchComboNetworkProbe][Client] CANCEL_COMPLETE Open=%d Damage=%d Close=%d ImplicitClose=%d Accepted=%d Mask=0x%X Presses=%d CancelObserved=1 Cleanup=%d"),
				AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestDamageEventCount(),
				AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(), AbilityInstance->GetTestAcceptedDamageCount(),
				AbilityInstance->GetTestAcceptedDamageSectionMask(), CrunchComboNetworkProbePresses, bCleanupComplete ? 1 : 0);
		}
		else
		{
			UE_LOG(LogAura, Display,
				TEXT("[CrunchComboNetworkProbe][Client] COMPLETE Open=%d Damage=%d Close=%d ImplicitClose=%d Accepted=%d Mask=0x%X Presses=%d Cleanup=%d"),
			AbilityInstance->GetTestOpenEventCount(), AbilityInstance->GetTestDamageEventCount(),
			AbilityInstance->GetTestCloseEventCount(), AbilityInstance->GetTestImplicitCloseEventCount(), AbilityInstance->GetTestAcceptedDamageCount(),
			AbilityInstance->GetTestAcceptedDamageSectionMask(), CrunchComboNetworkProbePresses, bCleanupComplete ? 1 : 0);
		}
		bCrunchComboNetworkProbeEnabled = false;
		GetWorldTimerManager().ClearTimer(CrunchComboNetworkProbeTimerHandle);
	}
#endif
}
#endif

bool AAuraPlayerController::ValidateRoleBattleDay6Pawn(AAuraCharacter* PlayerCharacter, FString& OutFailure) const
{
	if (!PlayerCharacter || !PlayerCharacter->GetAppliedRoleState().IsValid()
		|| !PlayerCharacter->HasValidCombatIdentity())
	{
		OutFailure = TEXT("Applied role state or final combat identity is not ready.");
		return false;
	}
	const FName RoleId = PlayerCharacter->GetAppliedRoleState().RoleId;
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(PlayerCharacter);
	const FRoleDefaultInfo* Definition = RoleInfo ? RoleInfo->RoleInformation.Find(RoleId) : nullptr;
	const UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(PlayerCharacter->GetAbilitySystemComponent());
	if (!Definition || !ASC)
	{
		OutFailure = TEXT("Role definition or ASC is unavailable.");
		return false;
	}
	if (!PlayerCharacter->GetCombatIdentity().CombatProfileTag.MatchesTagExact(Definition->CombatProfile)
		|| !PlayerCharacter->GetAppliedRoleState().EconomyProfileTag.MatchesTagExact(Definition->EconomyProfile)
		|| !PlayerCharacter->GetAppliedRoleState().InteractionProfileTag.MatchesTagExact(Definition->InteractionProfile))
	{
		OutFailure = TEXT("Applied identity/profile values do not match the authoritative definition.");
		return false;
	}

	TArray<FGameplayTag> RequiredTags;
	for (const TObjectPtr<UObject>& Object : Definition->StartupAbilityDefinitions)
	{
		if (const UAuraAbilityDefinition* AbilityDefinition = Cast<UAuraAbilityDefinition>(Object.Get())) RequiredTags.Add(AbilityDefinition->AbilityTag);
	}
	for (const TObjectPtr<UObject>& Object : Definition->StartupPassiveAbilityDefinitions)
	{
		if (const UAuraAbilityDefinition* AbilityDefinition = Cast<UAuraAbilityDefinition>(Object.Get())) RequiredTags.Add(AbilityDefinition->AbilityTag);
	}
	if (const UAuraAbilityDefinition* AbilityDefinition = Cast<UAuraAbilityDefinition>(Definition->DefaultLMBAbilityDefinition.Get()))
	{
		RequiredTags.Add(AbilityDefinition->AbilityTag);
	}
	for (const FGameplayTag& AbilityTag : RequiredTags)
	{
		if (ASC->CountAbilitySpecsByTag(AbilityTag) != 1)
		{
			OutFailure = FString::Printf(TEXT("Expected one role spec for %s."), *AbilityTag.ToString());
			return false;
		}
	}
	if (RoleId == TEXT("BungeeMan"))
	{
		const USkeletalMeshComponent* WeaponComponent = ICombatInterface::Execute_GetWeapon(PlayerCharacter);
		if (!WeaponComponent || WeaponComponent->GetSkeletalMeshAsset() != Definition->WeaponMesh
			|| Definition->WeaponTipSocketName != TEXT("Muzzle"))
		{
			OutFailure = TEXT("BungeeMan explicit rifle/Muzzle presentation is incomplete.");
			return false;
		}
	}
	return true;
}

bool AAuraPlayerController::AuditRoleBattleDay6SaveReconciliation(FString& OutFailure) const
{
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	const FRoleDefaultInfo* AuraDefinition = RoleInfo ? RoleInfo->RoleInformation.Find(TEXT("Aura")) : nullptr;
	if (!GetWorld() || !AuraDefinition)
	{
		OutFailure = TEXT("Save audit could not resolve the Aura definition.");
		return false;
	}
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	AActor* AuditOwner = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
	if (!AuditOwner)
	{
		OutFailure = TEXT("Save audit could not spawn its isolated owner.");
		return false;
	}
	UAuraAbilitySystemComponent* AuditASC = NewObject<UAuraAbilitySystemComponent>(AuditOwner, NAME_None, RF_Transient);
	AuditOwner->AddInstanceComponent(AuditASC);
	AuditASC->RegisterComponent();
	AuditASC->InitAbilityActorInfo(AuditOwner, AuditOwner);
	AuditASC->AbilityActorInfoSet();
	ULoadScreenSaveGame* Save = NewObject<ULoadScreenSaveGame>(AuditOwner);
	Save->Role = TEXT("Aura");
	FSavedAbility SavedFireBolt;
	SavedFireBolt.AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Fire.FireBolt"));
	SavedFireBolt.AbilityLevel = 2;
	SavedFireBolt.AbilitySlot = FAuraGameplayTags::Get().InputTag_LMB;
	SavedFireBolt.AbilityStatus = FAuraGameplayTags::Get().Abilities_Status_Equipped;
	SavedFireBolt.GrantSource = static_cast<uint8>(EAuraAbilityGrantSource::Role);
	SavedFireBolt.GrantedRoleId = TEXT("Aura");
	Save->SavedAbilities.Add(SavedFireBolt);
	FString Error;
	const bool bApplied = AuditASC->ApplyRoleGrantSet(TEXT("Aura"), RoleInfo->RoleDefinitionVersion, *AuraDefinition, Save, Error);
	const FGameplayAbilitySpec* FireBoltSpec = AuditASC->GetSpecFromAbilityTag(SavedFireBolt.AbilityTag);
	const bool bPassed = bApplied && FireBoltSpec && FireBoltSpec->Level == 2
		&& AuditASC->GetRoleGrantLedger().AbilitySpecHandles.Num() == 4;
	if (!bPassed)
	{
		OutFailure = Error.IsEmpty() ? TEXT("Existing-save role grant reconciliation audit failed.") : Error;
	}
	AuditOwner->Destroy();
	return bPassed;
}

void AAuraPlayerController::TickRoleBattleDay6NetworkProbe()
{
#if !UE_BUILD_SHIPPING
	if (!bRoleBattleDay6ProbeEnabled) return;
	AAuraCharacter* PlayerCharacter = GetPawn<AAuraCharacter>();
	if (!PlayerCharacter || !PlayerCharacter->GetAppliedRoleState().IsValid() || !PlayerCharacter->HasValidCombatIdentity()) return;

	if (!HasAuthority())
	{
		if (bRoleBattleDay6ClientAuditComplete) return;
		const FName OtherRole = PlayerCharacter->GetAppliedRoleState().RoleId == TEXT("Aura") ? FName(TEXT("BungeeMan")) : FName(TEXT("Aura"));
		const FAuraRoleApplicationResult Mutation = PlayerCharacter->ApplyRoleAtSpawn(OtherRole);
		const bool bRejected = !Mutation.bSuccess && Mutation.Error == EAuraRoleApplicationError::NotAuthority;
		UE_LOG(LogAura, Display,
			TEXT("[Day6NetworkProbe][Client] Role=%s Combat=%s Presentation=1 ClientRoleMutationRejected=%d AuthorityGrantMutation=0"),
			*PlayerCharacter->GetAppliedRoleState().RoleId.ToString(),
			*PlayerCharacter->GetCombatIdentity().CombatProfileTag.ToString(), bRejected);
		bRoleBattleDay6ClientAuditComplete = true;
		bRoleBattleDay6ProbeEnabled = false;
		GetWorldTimerManager().ClearTimer(RoleBattleDay6ProbeTimerHandle);
		return;
	}

	FString Failure;
	if (!ValidateRoleBattleDay6Pawn(PlayerCharacter, Failure)) return;
	const UAuraAttributeSet* Attributes = Cast<UAuraAttributeSet>(PlayerCharacter->GetAttributeSet());
	UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(PlayerCharacter->GetAbilitySystemComponent());
	if (!Attributes || !ASC) return;

	if (!bRoleBattleDay6InitialAuditComplete)
	{
		if (!AuditRoleBattleDay6SaveReconciliation(Failure))
		{
			UE_LOG(LogAura, Error, TEXT("[Day6NetworkProbe][Server] FAIL Role=%s Message=%s"),
				*PlayerCharacter->GetAppliedRoleState().RoleId.ToString(), *Failure);
			bRoleBattleDay6ProbeEnabled = false;
			return;
		}
		const FName OtherRole = PlayerCharacter->GetAppliedRoleState().RoleId == TEXT("Aura") ? FName(TEXT("BungeeMan")) : FName(TEXT("Aura"));
		const FAuraRoleApplicationResult Switch = PlayerCharacter->ApplyRoleAtSpawn(OtherRole);
		if (Switch.bSuccess || Switch.Error != EAuraRoleApplicationError::UnsupportedLiveSwitch)
		{
			UE_LOG(LogAura, Error, TEXT("[Day6NetworkProbe][Server] FAIL Role=%s Message=LiveSwitchAccepted"),
				*PlayerCharacter->GetAppliedRoleState().RoleId.ToString());
			bRoleBattleDay6ProbeEnabled = false;
			return;
		}
		RoleBattleDay6ExpectedMaxHealth = Attributes->GetMaxHealth();
		RoleBattleDay6ExpectedMaxMana = Attributes->GetMaxMana();
		RoleBattleDay6PreviousPawn = PlayerCharacter;
		bRoleBattleDay6InitialAuditComplete = true;
		UE_LOG(LogAura, Display,
			TEXT("[Day6NetworkProbe][Server] Role=%s Combat=%s AppliedProfiles=1 ServerRoleGrants=1 ExistingSaveReconciliation=1 LiveSwitchRejected=1 Respawn=0/2"),
			*PlayerCharacter->GetAppliedRoleState().RoleId.ToString(), *PlayerCharacter->GetCombatIdentity().CombatProfileTag.ToString());
		PlayerCharacter->Die(FVector::ZeroVector);
		return;
	}

	if (RoleBattleDay6PreviousPawn.Get() == PlayerCharacter) return;
	if (!FMath::IsNearlyEqual(Attributes->GetMaxHealth(), RoleBattleDay6ExpectedMaxHealth, 0.1f)
		|| !FMath::IsNearlyEqual(Attributes->GetMaxMana(), RoleBattleDay6ExpectedMaxMana, 0.1f))
	{
		UE_LOG(LogAura, Error, TEXT("[Day6NetworkProbe][Server] FAIL Role=%s Message=AttributeMaximaChanged"),
			*PlayerCharacter->GetAppliedRoleState().RoleId.ToString());
		bRoleBattleDay6ProbeEnabled = false;
		return;
	}
	++RoleBattleDay6Respawns;
	UE_LOG(LogAura, Display, TEXT("[Day6NetworkProbe][Server] Role=%s Respawn=%d/2 LedgerRole=%s RoleSpecs=%d"),
		*PlayerCharacter->GetAppliedRoleState().RoleId.ToString(), RoleBattleDay6Respawns,
		*ASC->GetRoleGrantLedger().GrantedRoleId.ToString(), ASC->GetRoleGrantLedger().AbilitySpecHandles.Num());
	if (RoleBattleDay6Respawns >= 2)
	{
		UE_LOG(LogAura, Display, TEXT("[Day6NetworkProbe][Server] PASS Role=%s Respawns=2 Profiles=1 Idempotent=1"),
			*PlayerCharacter->GetAppliedRoleState().RoleId.ToString());
		bRoleBattleDay6ProbeEnabled = false;
		GetWorldTimerManager().ClearTimer(RoleBattleDay6ProbeTimerHandle);
		return;
	}
	RoleBattleDay6PreviousPawn = PlayerCharacter;
	PlayerCharacter->Die(FVector::ZeroVector);
#endif
}

bool AAuraPlayerController::ValidateRoleBattleDay1Assets(FString& OutFailure) const
{
	const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this);
	const FRoleDefaultInfo* Bungee = RoleInfo ? RoleInfo->RoleInformation.Find(FName("BungeeMan")) : nullptr;
	if (!Bungee)
	{
		OutFailure = TEXT("BungeeMan role is missing from RoleConfig.json.");
		return false;
	}
	if (!Bungee->SkeletalMesh || !Bungee->AnimBlueprintClass || !Bungee->WeaponMesh)
	{
		OutFailure = TEXT("BungeeMan body mesh, animation blueprint, or weapon mesh failed to load.");
		return false;
	}
	if (Bungee->WeaponTipSocketName.IsNone() || !Bungee->WeaponMesh->FindSocket(Bungee->WeaponTipSocketName))
	{
		OutFailure = FString::Printf(TEXT("BungeeMan weapon socket '%s' is missing."), *Bungee->WeaponTipSocketName.ToString());
		return false;
	}

	const UAuraAbilityDefinition* FireGun = Cast<UAuraAbilityDefinition>(Bungee->DefaultLMBAbilityDefinition.Get());
	if (!FireGun || !FireGun->RootNode || !FireGun->AbilityTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"))))
	{
		OutFailure = TEXT("BungeeMan FireGun ability definition failed to load.");
		return false;
	}

	const FAuraProjectileDefinition* Bullet = FAuraGameplayConfig::FindProjectile(TEXT("fireGunBullet"));
	if (!Bullet || Bullet->NativeClass.Get() != AAuraProjectile::StaticClass())
	{
		OutFailure = TEXT("FireGun fireGunBullet projectile definition failed to resolve to AAuraProjectile.");
		return false;
	}

	UE_LOG(LogAura, Display, TEXT("[Day1Smoke] BungeeMan assets pass: mesh=%s anim=%s weapon=%s socket=%s FireGun=%s projectile=%s."),
		*Bungee->SkeletalMesh->GetName(),
		*GetNameSafe(Bungee->AnimBlueprintClass),
		*Bungee->WeaponMesh->GetName(),
		*Bungee->WeaponTipSocketName.ToString(),
		*FireGun->AbilityTag.ToString(),
		*Bullet->Name.ToString());
	return true;
}

bool AAuraPlayerController::ValidateRoleBattleDay1Damage(AAuraCharacter* PlayerCharacter, FString& OutFailure) const
{
	if (!PlayerCharacter || !PlayerCharacter->HasAuthority())
	{
		OutFailure = TEXT("Damage smoke test has no authoritative player avatar.");
		return false;
	}

	AAuraEnemy* Enemy = nullptr;
	UAuraAbilitySystemComponent* EnemyASC = nullptr;
	for (TActorIterator<AAuraEnemy> It(GetWorld()); It; ++It)
	{
		if (AAuraEnemy* Candidate = *It)
		{
			if (UAuraAbilitySystemComponent* CandidateASC = Cast<UAuraAbilitySystemComponent>(Candidate->GetAbilitySystemComponent()))
			{
				Enemy = Candidate;
				EnemyASC = CandidateASC;
				break;
			}
		}
	}

	UAuraAbilitySystemComponent* PlayerASC = const_cast<AAuraPlayerController*>(this)->GetASC();
	const UAuraAttributeSet* PlayerAttributes = PlayerASC ? PlayerASC->GetSet<UAuraAttributeSet>() : nullptr;
	const UAuraAttributeSet* EnemyAttributes = EnemyASC ? EnemyASC->GetSet<UAuraAttributeSet>() : nullptr;
	if (!Enemy || !PlayerASC || !PlayerAttributes || !EnemyAttributes)
	{
		OutFailure = TEXT("Damage smoke test could not resolve player/enemy ASCs and attributes.");
		return false;
	}

	FAuraCombatRuleContext PlayerToEnemyContext;
	PlayerToEnemyContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	PlayerToEnemyContext.TrustedWorldContext = PlayerCharacter;
	PlayerToEnemyContext.SourceActor = PlayerCharacter;
	PlayerToEnemyContext.TargetActor = Enemy;
	FAuraCombatRuleContext PlayerSelfContext = PlayerToEnemyContext;
	PlayerSelfContext.TargetActor = PlayerCharacter;
	FAuraCombatRuleContext EnemySelfContext = PlayerToEnemyContext;
	EnemySelfContext.SourceActor = Enemy;
	EnemySelfContext.TargetActor = Enemy;
	if (!FAuraCombatRules::CanDamage(PlayerCharacter, Enemy, PlayerToEnemyContext).bCanDamage
		|| FAuraCombatRules::CanDamage(PlayerCharacter, PlayerCharacter, PlayerSelfContext).bCanDamage
		|| FAuraCombatRules::CanDamage(Enemy, Enemy, EnemySelfContext).bCanDamage)
	{
		OutFailure = TEXT("Player/enemy relationship policy returned an unexpected result.");
		return false;
	}

	const FGameplayTag PhysicalDamage = FAuraGameplayTags::Get().Damage_Physical;
	const float EnemyHealthBefore = EnemyAttributes->GetHealth();
	FDamageEffectParams PlayerDamage;
	PlayerDamage.SourceAbilitySystemComponent = PlayerASC;
	PlayerDamage.TargetAbilitySystemComponent = EnemyASC;
	PlayerDamage.BaseDamage = 10.f;
	PlayerDamage.AbilityLevel = 1.f;
	PlayerDamage.DamageType = PhysicalDamage;
	UAuraAbilitySystemLibrary::ApplyDamageEffect(PlayerDamage);
	if (!(EnemyAttributes->GetHealth() < EnemyHealthBefore))
	{
		OutFailure = FString::Printf(TEXT("Player->Enemy damage did not reduce enemy health (%.1f -> %.1f)."),
			EnemyHealthBefore, EnemyAttributes->GetHealth());
		return false;
	}

	const float PlayerHealthBefore = PlayerAttributes->GetHealth();
	FDamageEffectParams EnemyDamage;
	EnemyDamage.SourceAbilitySystemComponent = EnemyASC;
	EnemyDamage.TargetAbilitySystemComponent = PlayerASC;
	EnemyDamage.BaseDamage = 10.f;
	EnemyDamage.AbilityLevel = 1.f;
	EnemyDamage.DamageType = PhysicalDamage;
	UAuraAbilitySystemLibrary::ApplyDamageEffect(EnemyDamage);
	if (!(PlayerAttributes->GetHealth() < PlayerHealthBefore))
	{
		OutFailure = FString::Printf(TEXT("Enemy->Player damage did not reduce player health (%.1f -> %.1f)."),
			PlayerHealthBefore, PlayerAttributes->GetHealth());
		return false;
	}

	const float EnemyHealthAfter = EnemyAttributes->GetHealth();
	const float PlayerHealthAfter = PlayerAttributes->GetHealth();
	UAuraAbilitySystemLibrary::TopOffVitalAttributes(PlayerASC, PlayerCharacter);
	UE_LOG(LogAura, Display, TEXT("[Day1Smoke] Damage direction pass: Player->Enemy %.1f->%.1f, Enemy->Player %.1f->%.1f."),
		EnemyHealthBefore, EnemyHealthAfter, PlayerHealthBefore, PlayerHealthAfter);
	return true;
}

bool AAuraPlayerController::ValidateRoleBattleDay1Vitals(const UAuraAttributeSet* InAttributes, FString& OutFailure) const
{
	if (!InAttributes)
	{
		OutFailure = TEXT("Respawn smoke test could not resolve the player AttributeSet.");
		return false;
	}

	const bool bHealthy = FMath::IsNearlyEqual(InAttributes->GetHealth(), InAttributes->GetMaxHealth(), 0.1f)
		&& FMath::IsNearlyEqual(InAttributes->GetMana(), InAttributes->GetMaxMana(), 0.1f)
		&& FMath::IsNearlyEqual(InAttributes->GetMaxHealth(), RoleBattleDay1SmokeExpectedMaxHealth, 0.1f)
		&& FMath::IsNearlyEqual(InAttributes->GetMaxMana(), RoleBattleDay1SmokeExpectedMaxMana, 0.1f);
	if (!bHealthy)
	{
		OutFailure = FString::Printf(
			TEXT("Respawn vitals changed: Health=%.1f/%.1f Mana=%.1f/%.1f expected maxima=%.1f/%.1f."),
			InAttributes->GetHealth(),
			InAttributes->GetMaxHealth(),
			InAttributes->GetMana(),
			InAttributes->GetMaxMana(),
			RoleBattleDay1SmokeExpectedMaxHealth,
			RoleBattleDay1SmokeExpectedMaxMana);
	}
	return bHealthy;
}

void AAuraPlayerController::TickRoleBattleDay1Smoke()
{
	if (!bRoleBattleDay1SmokeEnabled || !HasAuthority())
	{
		return;
	}

	AAuraCharacter* PlayerCharacter = GetPawn<AAuraCharacter>();
	if (!PlayerCharacter)
	{
		return;
	}

	if (!bRoleBattleDay1SmokeAssetsChecked)
	{
		FString Failure;
		if (!ValidateRoleBattleDay1Assets(Failure))
		{
			FinishRoleBattleDay1Smoke(false, Failure);
			return;
		}
		bRoleBattleDay1SmokeAssetsChecked = true;
	}

	if (!bRoleBattleDay1SmokeDamageChecked)
	{
		FString Failure;
		if (!ValidateRoleBattleDay1Damage(PlayerCharacter, Failure))
		{
			FinishRoleBattleDay1Smoke(false, Failure);
			return;
		}
		bRoleBattleDay1SmokeDamageChecked = true;
	}

	const UAuraAttributeSet* Attributes = GetAuraAS();
	if (!Attributes)
	{
		return;
	}

	if (!bRoleBattleDay1SmokeDeathStarted)
	{
		RoleBattleDay1SmokeExpectedMaxHealth = Attributes->GetMaxHealth();
		RoleBattleDay1SmokeExpectedMaxMana = Attributes->GetMaxMana();
		FString Failure;
		if (!ValidateRoleBattleDay1Vitals(Attributes, Failure))
		{
			FinishRoleBattleDay1Smoke(false, Failure);
			return;
		}

		bRoleBattleDay1SmokeDeathStarted = true;
		RoleBattleDay1SmokePreviousPawn = PlayerCharacter;
		UE_LOG(LogAura, Display, TEXT("[Day1Smoke] Initial vitals pass: Health=%.1f/%.1f Mana=%.1f/%.1f. Forcing death 1/2."),
			Attributes->GetHealth(), Attributes->GetMaxHealth(), Attributes->GetMana(), Attributes->GetMaxMana());
		PlayerCharacter->Die(FVector::ZeroVector);
		return;
	}

	if (RoleBattleDay1SmokePreviousPawn == PlayerCharacter)
	{
		return;
	}

	FString Failure;
	if (!ValidateRoleBattleDay1Vitals(Attributes, Failure))
	{
		FinishRoleBattleDay1Smoke(false, Failure);
		return;
	}

	++RoleBattleDay1SmokeRespawns;
	UE_LOG(LogAura, Display, TEXT("[Day1Smoke] Respawn %d/2 vitals pass: Health=%.1f/%.1f Mana=%.1f/%.1f."),
		RoleBattleDay1SmokeRespawns,
		Attributes->GetHealth(), Attributes->GetMaxHealth(), Attributes->GetMana(), Attributes->GetMaxMana());

	if (RoleBattleDay1SmokeRespawns >= 2)
	{
		FinishRoleBattleDay1Smoke(true, TEXT("BungeeMan asset wiring and two respawn vital checks passed."));
		return;
	}

	RoleBattleDay1SmokePreviousPawn = PlayerCharacter;
	UE_LOG(LogAura, Display, TEXT("[Day1Smoke] Forcing death %d/2."), RoleBattleDay1SmokeRespawns + 1);
	PlayerCharacter->Die(FVector::ZeroVector);
}

void AAuraPlayerController::FinishRoleBattleDay1Smoke(bool bPassed, const FString& Message)
{
	bRoleBattleDay1SmokeEnabled = false;
	GetWorldTimerManager().ClearTimer(RoleBattleDay1SmokeTimerHandle);
	UE_LOG(LogAura, Display, TEXT("[Day1Smoke] %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Message);
	if (GLog)
	{
		GLog->Flush();
	}
	FPlatformMisc::RequestExit(false);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UAuraInputComponent* AuraInputComponent = CastChecked<UAuraInputComponent>(InputComponent);
	AuraInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAuraPlayerController::Move);
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, &AAuraPlayerController::ShiftPressed);
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, &AAuraPlayerController::ShiftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, EInputEvent::IE_Pressed, this, &AAuraPlayerController::RightMousePressed);
	InputComponent->BindKey(EKeys::RightMouseButton, EInputEvent::IE_Released, this, &AAuraPlayerController::RightMouseReleased);
	InputComponent->BindKey(EKeys::SpaceBar, EInputEvent::IE_Pressed, this, &AAuraPlayerController::JumpPressed);
	InputComponent->BindKey(EKeys::SpaceBar, EInputEvent::IE_Released, this, &AAuraPlayerController::JumpReleased);
	InputComponent->BindKey(EKeys::LeftControl, EInputEvent::IE_Pressed, this, &AAuraPlayerController::CrouchPressed);
	InputComponent->BindKey(EKeys::LeftControl, EInputEvent::IE_Released, this, &AAuraPlayerController::CrouchReleased);
	if (InteractAction && AuraContext)
	{
		AuraInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AAuraPlayerController::InteractPressed);
	}
	else
	{
		InputComponent->BindKey(EKeys::F, EInputEvent::IE_Pressed, this, &AAuraPlayerController::InteractPressed);
	}
	InputComponent->BindKey(EKeys::One, EInputEvent::IE_Pressed, this, &AAuraPlayerController::InteractionOptionOnePressed);
	InputComponent->BindKey(EKeys::Two, EInputEvent::IE_Pressed, this, &AAuraPlayerController::InteractionOptionTwoPressed);
	// Broom vertical flight: Q ascends, E descends. Held-flags sampled in PlayerTick.
	InputComponent->BindKey(EKeys::Q, EInputEvent::IE_Pressed, this, &AAuraPlayerController::BroomAscendPressed);
	InputComponent->BindKey(EKeys::Q, EInputEvent::IE_Released, this, &AAuraPlayerController::BroomAscendReleased);
	InputComponent->BindKey(EKeys::E, EInputEvent::IE_Pressed, this, &AAuraPlayerController::BroomDescendPressed);
	InputComponent->BindKey(EKeys::E, EInputEvent::IE_Released, this, &AAuraPlayerController::BroomDescendReleased);
	InputComponent->BindKey(EKeys::R, EInputEvent::IE_Pressed, this, &AAuraPlayerController::RequestFirearmReload);
	AuraInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void AAuraPlayerController::ApplyGameAndUIInputMode()
{
	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::RightMousePressed()
{
	bRightMouseDown = true;
	bCachedShowMouseCursor = bShowMouseCursor;
	bShowMouseCursor = false;
	bAutoRunning = false;

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);
}

void AAuraPlayerController::RightMouseReleased()
{
	bRightMouseDown = false;
	bShowMouseCursor = bCachedShowMouseCursor;
	ApplyGameAndUIInputMode();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AAuraPlayerController::RotateCameraFromMouseDelta()
{
	if (!bRightMouseDown)
	{
		return;
	}

	float MouseDeltaX = 0.f;
	float MouseDeltaY = 0.f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (FMath::IsNearlyZero(MouseDeltaX) && FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (USpringArmComponent* CameraBoom = ControlledPawn->FindComponentByClass<USpringArmComponent>())
		{
			FRotator BoomRotation = CameraBoom->GetComponentRotation();
			BoomRotation.Yaw = FRotator::NormalizeAxis(BoomRotation.Yaw + MouseDeltaX * RightMouseYawSpeed);
			BoomRotation.Pitch = FMath::Clamp(BoomRotation.Pitch - MouseDeltaY * RightMousePitchSpeed, CameraPitchMin, CameraPitchMax);
			CameraBoom->SetWorldRotation(BoomRotation);
			SetControlRotation(FRotator(0.f, BoomRotation.Yaw, 0.f));

			// Keep the broom facing the camera's forward direction while mounted.
			UpdateMountedBroomYaw(BoomRotation.Yaw);
		}
	}
}

void AAuraPlayerController::RotateCameraFromScreenEdge(float DeltaTime)
{
	if (!bEnableEdgeScreenCameraRotation || bRightMouseDown)
	{
		return;
	}

	if (EdgeScreenBorderSize <= 0.f || DeltaTime <= 0.f)
	{
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const float MinimumBorderX = ViewportSizeX / 6.f;
	const float MinimumBorderY = ViewportSizeY / 6.f;
	const float BorderX = FMath::Min(FMath::Max(EdgeScreenBorderSize, MinimumBorderX), ViewportSizeX * 0.5f);
	const float BorderY = FMath::Min(FMath::Max(EdgeScreenBorderSize, MinimumBorderY), ViewportSizeY * 0.5f);

	float YawAlpha = 0.f;
	if (MouseX <= BorderX)
	{
		YawAlpha = -(1.f - FMath::Clamp(MouseX / BorderX, 0.f, 1.f));
	}
	else if (MouseX >= ViewportSizeX - BorderX)
	{
		const float DistanceFromRight = FMath::Clamp((ViewportSizeX - MouseX) / BorderX, 0.f, 1.f);
		YawAlpha = 1.f - DistanceFromRight;
	}

	float PitchAlpha = 0.f;
	if (MouseY <= BorderY)
	{
		PitchAlpha = 1.f - FMath::Clamp(MouseY / BorderY, 0.f, 1.f);
	}
	else if (MouseY >= ViewportSizeY - BorderY)
	{
		const float DistanceFromBottom = FMath::Clamp((ViewportSizeY - MouseY) / BorderY, 0.f, 1.f);
		PitchAlpha = -(1.f - DistanceFromBottom);
	}

	if (FMath::IsNearlyZero(YawAlpha) && FMath::IsNearlyZero(PitchAlpha))
	{
		return;
	}

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (USpringArmComponent* CameraBoom = ControlledPawn->FindComponentByClass<USpringArmComponent>())
		{
			FRotator BoomRotation = CameraBoom->GetComponentRotation();
			BoomRotation.Yaw = FRotator::NormalizeAxis(BoomRotation.Yaw + YawAlpha * EdgeScreenYawDegreesPerSecond * DeltaTime);
			BoomRotation.Pitch = FMath::Clamp(
				BoomRotation.Pitch + PitchAlpha * EdgeScreenPitchDegreesPerSecond * DeltaTime,
				CameraPitchMin,
				CameraPitchMax
			);
			CameraBoom->SetWorldRotation(BoomRotation);
			SetControlRotation(FRotator(0.f, BoomRotation.Yaw, 0.f));

			// Keep the broom facing the camera's forward direction while mounted.
			UpdateMountedBroomYaw(BoomRotation.Yaw);
		}
	}
}

void AAuraPlayerController::Move(const FInputActionValue& InputActionValue)
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bCanLogMove = CurrentTime - LastMoveInputLogTime >= MoveInputLogInterval;
	const bool bCanLogMoveBlocked = CurrentTime - LastMoveBlockedLogTime >= MoveInputLogInterval;

	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	// ── Broom flight ───────────────────────────────────────────────────────────
	// Checked BEFORE the Player_Block_InputPressed gate so that broom locomotion
	// is never silenced by ability-system input blocks applied during mounting.
	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		AAuraBroomVehicle* MountedBroom = Cast<AAuraBroomVehicle>(ControlledCharacter->GetAttachParentActor());

		if (bCanLogMove)
		{
			UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] Move called. Controller=%s Pawn=%s AttachParent=%s MountedBroom=%s Input=%s"),
				*GetNameSafe(this),
				*GetNameSafe(ControlledCharacter),
				*GetNameSafe(ControlledCharacter->GetAttachParentActor()),
				*GetNameSafe(MountedBroom),
				*InputAxisVector.ToString());
		}

		if (IsBroomMountedByControlledCharacter(MountedBroom))
		{
			const bool bTagBlocked = GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed);
			UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] On broom. TagBlock=%s Input=%s HasAuthority=%s"),
				bTagBlocked ? TEXT("YES") : TEXT("no"),
				*InputAxisVector.ToString(),
				HasAuthority() ? TEXT("true") : TEXT("false"));

			const FRotator Rotation = GetControlRotation();
			const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			const FVector FlightInput = (ForwardDirection * InputAxisVector.Y) + (RightDirection * InputAxisVector.X);
			const float FlightScale = FMath::Clamp(FlightInput.Size(), 0.f, 1.f);

			UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] FlightInput=%s FlightScale=%.3f"),
				*FlightInput.ToCompactString(), FlightScale);

			if (FlightScale > KINDA_SMALL_NUMBER)
			{
				const FVector FlightDirection = FlightInput / FlightScale;
				if (HasAuthority())
				{
					UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] Authority — calling AddFlightInput directly. Dir=%s Scale=%.3f"),
						*FlightDirection.ToCompactString(), FlightScale);
					MountedBroom->AddFlightInput(FlightDirection, FlightScale);
				}
				else
				{
					UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] Client — sending ServerApplyBroomFlightInput RPC. Dir=%s Scale=%.3f"),
						*FlightDirection.ToCompactString(), FlightScale);
					ServerApplyBroomFlightInput(MountedBroom, FlightDirection, FlightScale);
				}

				LastMoveInputLogTime = CurrentTime;
			}
			return;
		}
	}

	// ── Ground movement ────────────────────────────────────────────────────────
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
	{
		if (bCanLogMoveBlocked)
		{
			UE_LOG(LogAura, Warning, TEXT("Move blocked by gameplay tag. Controller=%s Pawn=%s"), *GetNameSafe(this), *GetNameSafe(GetPawn()));
			LastMoveBlockedLogTime = CurrentTime;
		}
		return;
	}

	const bool bShouldSprint = bShiftKeyDown && !InputAxisVector.IsNearlyZero();
	if (bShouldSprint != bIsSprinting)
	{
		UE_LOG(LogAura, Log, TEXT("Move sprint state changed. Controller=%s NewSprinting=%s Input=%s"),
			*GetNameSafe(this),
			bShouldSprint ? TEXT("true") : TEXT("false"),
			*InputAxisVector.ToString());

		bIsSprinting = bShouldSprint;
		ApplySprintState(bIsSprinting);
		if (!HasAuthority())
		{
			ServerSetSprinting(bIsSprinting);
		}
	}

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		// Feed movement input through in both walking and falling states.
		// While airborne the CharacterMovementComponent applies AirControl
		// (set on AAuraCharacter) instead of ground acceleration, giving the
		// player limited lateral steering and forward-redirect mid-jump while
		// preserving the jump's momentum. Do NOT early-out on IsFalling() here
		// — that would starve air control and pin the player to the jump's
		// initial horizontal velocity.
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);

		if (bCanLogMove)
		{
			UE_LOG(LogAura, Verbose, TEXT("Move applied. Pawn=%s Input=%s Forward=%s Right=%s Velocity=%s Location=%s HasAuthority=%s"),
				*GetNameSafe(ControlledPawn),
				*InputAxisVector.ToString(),
				*ForwardDirection.ToCompactString(),
				*RightDirection.ToCompactString(),
				*ControlledPawn->GetVelocity().ToCompactString(),
				*ControlledPawn->GetActorLocation().ToCompactString(),
				HasAuthority() ? TEXT("true") : TEXT("false"));
			LastMoveInputLogTime = CurrentTime;
		}
	}
	else if (bCanLogMoveBlocked)
	{
		UE_LOG(LogAura, Warning, TEXT("Move ignored: no controlled pawn. Controller=%s Input=%s"),
			*GetNameSafe(this),
			*InputAxisVector.ToString());
		LastMoveBlockedLogTime = CurrentTime;
	}
}

void AAuraPlayerController::JumpPressed()
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
	{
		return;
	}

	bAutoRunning = false;

	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		if (AAuraBroomVehicle* MountedBroom = Cast<AAuraBroomVehicle>(ControlledCharacter->GetAttachParentActor()))
		{
			RequestBroomDismount(MountedBroom);
			return;
		}

		ControlledCharacter->Jump();
	}
}

void AAuraPlayerController::JumpReleased()
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputReleased))
	{
		return;
	}

	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		ControlledCharacter->StopJumping();
	}
}

void AAuraPlayerController::CrouchPressed()
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
	{
		return;
	}

	bAutoRunning = false;

	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		ControlledCharacter->Crouch();
	}
}

void AAuraPlayerController::CrouchReleased()
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputReleased))
	{
		return;
	}

	if (ACharacter* ControlledCharacter = GetPawn<ACharacter>())
	{
		ControlledCharacter->UnCrouch();
	}
}

void AAuraPlayerController::BroomAscendPressed()
{
	bBroomAscendHeld = true;
	bAutoRunning = false;
}

void AAuraPlayerController::BroomAscendReleased()
{
	bBroomAscendHeld = false;
}

void AAuraPlayerController::BroomDescendPressed()
{
	bBroomDescendHeld = true;
	bAutoRunning = false;
}

void AAuraPlayerController::BroomDescendReleased()
{
	bBroomDescendHeld = false;
}

void AAuraPlayerController::ApplyBroomVerticalFlight()
{
	// Bail early when neither key is held so this is a no-op on foot and we don't
	// touch the attach parent lookup every tick for nothing.
	if (!bBroomAscendHeld && !bBroomDescendHeld)
	{
		return;
	}

	ACharacter* ControlledCharacter = GetPawn<ACharacter>();
	if (!ControlledCharacter)
	{
		return;
	}

	// Only act while mounted on a broom — Q/E do nothing on foot. Mirrors the Move
	// path, which also bypasses the Player_Block_InputPressed gate for broom flight
	// so mounting-time input blocks don't silence vertical steering.
	AAuraBroomVehicle* MountedBroom = Cast<AAuraBroomVehicle>(ControlledCharacter->GetAttachParentActor());
	if (!IsBroomMountedByControlledCharacter(MountedBroom))
	{
		return;
	}

	// Net vertical sign: E (ascend) is +1, Q (descend) is -1. Holding both cancels.
	const float VerticalSign = (bBroomAscendHeld ? 1.f : 0.f) - (bBroomDescendHeld ? 1.f : 0.f);
	if (FMath::IsNearlyZero(VerticalSign))
	{
		return;
	}

	// World-up is the natural vertical axis for free flight; the broom's yaw is left
	// untouched (AddFlightInput only re-yaws from the horizontal input component).
	const FVector FlightDirection = FVector::UpVector * VerticalSign;
	const float FlightScale = FMath::Clamp(BroomVerticalFlightScale, 0.f, 1.f);
	if (FlightScale <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (HasAuthority())
	{
		MountedBroom->AddFlightInput(FlightDirection, FlightScale);
	}
	else
	{
		ServerApplyBroomFlightInput(MountedBroom, FlightDirection, FlightScale);
	}
}
