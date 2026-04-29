// Copyright Druid Mechanics


#include "Player/AuraPlayerController.h"
#include "UI/HUD/AuraHUD.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Actor/MagicCircle.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "Components/DecalComponent.h"
#include "Components/SplineComponent.h"
#include "Input/AuraInputComponent.h"
#include "Interaction/EnemyInterface.h"
#include "GameFramework/Character.h"
#include "Interaction/HighlightInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWidgetController.h"
#include "UI/Widget/DamageTextComponent.h"
#include "InputCoreTypes.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
	Spline = CreateDefaultSubobject<USplineComponent>("Spline");
}

void AAuraPlayerController::FullAbilities()
{
	UE_LOG(LogAura, Log, TEXT("FullAbilities command invoked on %s. HasAuthority=%s Pawn=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetPawn()));

	if (HasAuthority())
	{
		ExecuteFullAbilities();
		return;
	}

	ServerFullAbilities();
}

void AAuraPlayerController::ShowLocation()
{
	if (AAuraHUD* AuraHUD = GetHUD<AAuraHUD>())
	{
		AuraHUD->ToggleLocationDisplay();
	}
}

void AAuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	CursorTrace();
	AutoRun();
	UpdateMagicCircleLocation();
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
		return;
	}
	const ECollisionChannel TraceChannel = IsValid(MagicCircle) ? ECC_ExcludePlayers : ECC_Visibility;
	GetHitResultUnderCursor(TraceChannel, false, CursorHit);
	if (!CursorHit.bBlockingHit) return;

	LastActor = ThisActor;
	if (IsValid(CursorHit.GetActor()) && CursorHit.GetActor()->Implements<UHighlightInterface>())
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
	}
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
	UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagPressed: Tag=%s ASC=%s"), *InputTag.ToString(), GetASC() ? TEXT("valid") : TEXT("null"));
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (IsValid(ThisActor))
		{
			TargetingStatus = ThisActor->Implements<UEnemyInterface>() ? ETargetingStatus::TargetingEnemy : ETargetingStatus::TargetingNonEnemy;
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
	UE_LOG(LogAura, Log, TEXT("[PC] AbilityInputTagReleased: Tag=%s TargetingStatus=%d FollowTime=%.3f"),
		*InputTag.ToString(), (int32)TargetingStatus, FollowTime);
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
		return;
	}

	if (GetASC()) GetASC()->AbilityInputTagReleased(InputTag);
	
	if (TargetingStatus != ETargetingStatus::TargetingEnemy && !bShiftKeyDown)
	{
		const APawn* ControlledPawn = GetPawn();
		if (FollowTime <= ShortPressThreshold && ControlledPawn)
		{
			if (IsValid(ThisActor) && ThisActor->Implements<UHighlightInterface>())
			{
				IHighlightInterface::Execute_SetMoveToLocation(ThisActor, CachedDestination);
			}
			else if (GetASC() && !GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ClickNiagaraSystem, CachedDestination);
			}
			if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ControlledPawn->GetActorLocation(), CachedDestination))
			{
				Spline->ClearSplinePoints();
				for (const FVector& PointLoc : NavPath->PathPoints)
				{
					Spline->AddSplinePoint(PointLoc, ESplineCoordinateSpace::World);
				}
				if (NavPath->PathPoints.Num() > 0)
				{
					CachedDestination = NavPath->PathPoints[NavPath->PathPoints.Num() - 1];
					bAutoRunning = true;
				}
			}
		}
		FollowTime = 0.f;
		TargetingStatus = ETargetingStatus::NotTargeting;
	}
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
		const bool bHasLMBAbility = GetASC() && GetASC()->GetSpecWithSlot(FAuraGameplayTags::Get().InputTag_LMB) != nullptr;
		if (bHasLMBAbility)
		{
			UE_LOG(LogAura, Log, TEXT("[PC] LMB Held: LMB ability equipped and not targeting enemy — routing to ASC"));
			GetASC()->AbilityInputTagHeld(InputTag);
		}
		else
		{
			// No ability assigned to LMB — do movement
			FollowTime += GetWorld()->GetDeltaSeconds();
			if (CursorHit.bBlockingHit) CachedDestination = CursorHit.ImpactPoint;

			if (APawn* ControlledPawn = GetPawn())
			{
				const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
				ControlledPawn->AddMovementInput(WorldDirection);
			}
		}
	}
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
	const UAuraAttributeSet* LocalAS = GetAuraAS();
	if (LocalAS == nullptr)
	{
		return false;
	}

	// Temporary safety gate: if replicated attributes are still zero on client, block skill input.
	// This avoids noisy failed prediction attempts until initial attribute replication finishes.
	const float MaxMana = LocalAS->GetMaxMana();
	return MaxMana > 0.f;
}

void AAuraPlayerController::ServerFullAbilities_Implementation()
{
	UE_LOG(LogAura, Log, TEXT("ServerFullAbilities received for controller %s Pawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()));

	ExecuteFullAbilities();
}

void AAuraPlayerController::ExecuteFullAbilities()
{
	UAuraAbilitySystemComponent* ASC = GetASC();
	UE_LOG(LogAura, Log, TEXT("ExecuteFullAbilities starting on controller %s ASC=%s Avatar=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ASC),
		ASC ? *GetNameSafe(ASC->GetAvatarActor()) : TEXT("None"));

	if (ASC == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("FullAbilities failed: missing ASC for controller %s"), *GetNameSafe(this));
		ClientMessage(TEXT("FullAbilities failed: missing AbilitySystemComponent."));
		return;
	}

	ASC->GrantAndEquipAllAbilities();
	UE_LOG(LogAura, Log, TEXT("ExecuteFullAbilities finished grant/equip for controller %s. Requesting client UI refresh."), *GetNameSafe(this));
	ClientRefreshAbilityUI();
	ClientMessage(TEXT("FullAbilities applied."));
}

void AAuraPlayerController::ClientRefreshAbilityUI_Implementation()
{
	UE_LOG(LogAura, Log, TEXT("ClientRefreshAbilityUI on controller %s"), *GetNameSafe(this));

	if (UOverlayWidgetController* OverlayWidgetController = UAuraAbilitySystemLibrary::GetOverlayWidgetController(this))
	{
		UE_LOG(LogAura, Log, TEXT("ClientRefreshAbilityUI: refreshing overlay widget controller %s"), *GetNameSafe(OverlayWidgetController));
		OverlayWidgetController->BroadcastAbilityInfo();
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("ClientRefreshAbilityUI: overlay widget controller not available for %s"), *GetNameSafe(this));
	}

	if (USpellMenuWidgetController* SpellMenuWidgetController = UAuraAbilitySystemLibrary::GetSpellMenuWidgetController(this))
	{
		UE_LOG(LogAura, Log, TEXT("ClientRefreshAbilityUI: refreshing spell menu widget controller %s"), *GetNameSafe(SpellMenuWidgetController));
		SpellMenuWidgetController->BroadcastInitialValues();
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("ClientRefreshAbilityUI: spell menu widget controller not available for %s"), *GetNameSafe(this));
	}
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(AuraContext);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(AuraContext, 0);
	}

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UAuraInputComponent* AuraInputComponent = CastChecked<UAuraInputComponent>(InputComponent);
	AuraInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAuraPlayerController::Move);
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, &AAuraPlayerController::ShiftPressed);
	AuraInputComponent->BindAction(ShiftAction, ETriggerEvent::Completed, this, &AAuraPlayerController::ShiftReleased);
	InputComponent->BindKey(EKeys::SpaceBar, EInputEvent::IE_Pressed, this, &AAuraPlayerController::JumpPressed);
	InputComponent->BindKey(EKeys::SpaceBar, EInputEvent::IE_Released, this, &AAuraPlayerController::JumpReleased);
	InputComponent->BindKey(EKeys::LeftControl, EInputEvent::IE_Pressed, this, &AAuraPlayerController::CrouchPressed);
	InputComponent->BindKey(EKeys::LeftControl, EInputEvent::IE_Released, this, &AAuraPlayerController::CrouchReleased);
	AuraInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void AAuraPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FAuraGameplayTags::Get().Player_Block_InputPressed))
	{
		return;
	}
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
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
