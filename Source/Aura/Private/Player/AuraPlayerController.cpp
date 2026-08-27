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
#include "Actor/MagicCircle.h"
#include "AuraAbilityTypes.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraEnemy.h"
#include "Character/AuraCharacter.h"
#include "Components/DecalComponent.h"
#include "Components/SplineComponent.h"
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
#include "Player/AuraCheatManager.h"
#include "Actor/AuraProjectile.h"
#include "Data/AuraGameplayConfig.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Game/LoadScreenSaveGame.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
	Spline = CreateDefaultSubobject<USplineComponent>("Spline");
	ServerTravelComponent = CreateDefaultSubobject<UServerTravelComponent>(TEXT("ServerTravelComponent"));
	ClientDisconnectHandler = CreateDefaultSubobject<UAuraClientDisconnectHandler>(TEXT("ClientDisconnectHandler"));
	HeartbeatComponent = CreateDefaultSubobject<UAuraHeartbeatComponent>(TEXT("HeartbeatComponent"));
	InteractionComponent = CreateDefaultSubobject<UAuraInteractionComponent>(TEXT("InteractionComponent"));

	// Route Aura-specific cheat commands through UAuraCheatManager. Its Exec
	// functions are only reachable while cheats are enabled (standalone / listen
	// host / after `enablecheats` on a dedicated server).
	CheatClass = UAuraCheatManager::StaticClass();
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
		ACharacter* ControlledCharacter = GetPawn<ACharacter>();
		if (IsValid(ControlledCharacter))
		{
			BroomToMount->RequestMount(ControlledCharacter);
		}
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
	if (!IsValid(BroomToDismount))
	{
		UE_LOG(LogAura, Warning, TEXT("ServerRequestBroomDismount ignored: invalid broom. Controller=%s"), *GetNameSafe(this));
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

	UE_LOG(LogAura, Log, TEXT("ServerRequestBroomMount processing. Controller=%s Character=%s Broom=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ControlledCharacter),
		*GetNameSafe(BroomToMount));

	BroomToMount->RequestMount(ControlledCharacter);
}

void AAuraPlayerController::ServerSetBroomYaw_Implementation(AAuraBroomVehicle* Broom, float WorldYaw)
{
	if (!IsValid(Broom))
	{
		return;
	}

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
	if (!IsValid(MountedBroom))
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

	if (!IsValid(Broom))
	{
		return;
	}

	Broom->AddFlightInput(WorldDirection, ScaleValue);
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

void AAuraPlayerController::RequestTransferToRandomPlayer()
{
	UE_LOG(LogAura, Log, TEXT("TransferToRandomPlayer invoked on %s. HasAuthority=%s Pawn=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetPawn()));

	if (HasAuthority())
	{
		ExecuteTransferToRandomPlayer();
		return;
	}

	ServerTransferToRandomPlayer();
}

void AAuraPlayerController::RequestAddMonster(int32 MonsterId)
{
	UE_LOG(LogAura, Log, TEXT("AddMonster invoked on %s. MonsterId=%d HasAuthority=%s Pawn=%s"),
		*GetNameSafe(this),
		MonsterId,
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetPawn()));

	if (HasAuthority())
	{
		ExecuteAddMonster(MonsterId);
		return;
	}

	ServerAddMonster(MonsterId);
}

void AAuraPlayerController::ServerAddMonster_Implementation(int32 MonsterId)
{
	ExecuteAddMonster(MonsterId);
}

void AAuraPlayerController::ExecuteAddMonster(int32 MonsterId)
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		UE_LOG(LogAura, Warning, TEXT("AddMonster failed: no controlled pawn for monster id %d."), MonsterId);
		ClientMessage(TEXT("AddMonster: no controlled pawn."));
		return;
	}

	AAuraGameModeBase* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr;
	if (!IsValid(GameMode))
	{
		UE_LOG(LogAura, Warning, TEXT("AddMonster failed: no authoritative AuraGameMode for monster id %d."), MonsterId);
		ClientMessage(TEXT("AddMonster: authoritative game mode unavailable."));
		return;
	}

	// Keep the spawn close to the player while avoiding placement directly inside
	// the player's capsule. The GameMode still applies the table row's level,
	// class, scale, and respawn settings.
	const float SpawnDistance = FMath::FRandRange(150.f, 250.f);
	const float SpawnAngleRadians = FMath::FRandRange(0.f, UE_TWO_PI);
	const FVector SpawnOffset(
		FMath::Cos(SpawnAngleRadians) * SpawnDistance,
		FMath::Sin(SpawnAngleRadians) * SpawnDistance,
		0.f);
	const FVector MonsterSpawnLocation = MyPawn->GetActorLocation() + SpawnOffset;

	AAuraEnemy* SpawnedEnemy = GameMode->SpawnMonsterByIdAtLocation(MonsterId, MonsterSpawnLocation);
	if (!IsValid(SpawnedEnemy))
	{
		ClientMessage(FString::Printf(TEXT("AddMonster: failed to spawn monster id %d."), MonsterId));
		return;
	}

	UE_LOG(LogAura, Log, TEXT("AddMonster spawned monster id %d near %s at %s."),
		MonsterId,
		*GetNameSafe(MyPawn),
		*MonsterSpawnLocation.ToCompactString());
	ClientMessage(FString::Printf(TEXT("AddMonster: spawned monster id %d nearby."), MonsterId));
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

void AAuraPlayerController::WebInteractPressed()
{
	InteractPressed();
}

void AAuraPlayerController::ServerTransferToRandomPlayer_Implementation()
{
	ExecuteTransferToRandomPlayer();
}

void AAuraPlayerController::ExecuteTransferToRandomPlayer()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		UE_LOG(LogAura, Warning, TEXT("TransferToRandomPlayer: no controlled pawn for %s."), *GetNameSafe(this));
		ClientMessage(TEXT("TransferToRandomPlayer: no local pawn to move."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Run on the server so every player pawn is a candidate: a far-away player's
	// pawn may be net-culled on the invoking client and not even exist there, but
	// the authoritative world still has it. Filter to pawns whose controller owns a
	// live NetConnection (real remote players), excluding our own pawn.
	TArray<APawn*> Candidates;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* CandidatePawn = *It;
		if (!CandidatePawn || CandidatePawn == MyPawn)
		{
			continue;
		}

		AController* CandidateController = CandidatePawn->GetController();
		if (!CandidateController || !CandidateController->GetNetConnection())
		{
			continue;
		}

		Candidates.Add(CandidatePawn);
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(LogAura, Log, TEXT("TransferToRandomPlayer: no other connected player pawns found."));
		ClientMessage(TEXT("TransferToRandomPlayer: no other connected players to teleport to."));
		return;
	}

	APawn* TargetPawn = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	const FVector TargetLocation = TargetPawn->GetActorLocation();
	const FRotator TargetRotation = TargetPawn->GetActorRotation();

	// Teleport (no sweep) so we don't collide-slide on arrival; bNoCheck=true
	// resets velocity/kinematics cleanly at the destination.
	MyPawn->TeleportTo(TargetLocation, TargetRotation, /*bIsTest=*/false, /*bNoCheck=*/true);

	UE_LOG(LogAura, Log, TEXT("TransferToRandomPlayer: moved %s -> %s (loc=%s)."),
		*MyPawn->GetName(), *TargetPawn->GetName(), *TargetLocation.ToString());
	ClientMessage(FString::Printf(TEXT("TransferToRandomPlayer: teleported to %s."), *TargetPawn->GetName()));
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
	CursorTrace();
	RotateCameraFromMouseDelta();
	RotateCameraFromScreenEdge(DeltaTime);
	AutoRun();
	UpdateMagicCircleLocation();
	ApplyBroomVerticalFlight();
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

void AAuraPlayerController::ClientRejectLogin_Implementation(const FString& Reason)
{
	UE_LOG(LogAura, Error, TEXT("[Role][Login] ClientRejectLogin: %s"), *Reason);

	// Reuse the same server-lost path as KickOutSelf / mid-game server loss: stash the reason as
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
#endif
	if (!AuraContext)
	{
		UE_LOG(LogAura, Warning, TEXT("AAuraPlayerController::BeginPlay: AuraContext is not set; using native input fallback where available."));
	}
	if (!InteractAction)
	{
		InteractAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Blueprints/Input/InputActions/IA_Interact.IA_Interact"));
	}

#if !UE_BUILD_SHIPPING
	// Auto-enable cheats in non-shipping builds so the UAuraCheatManager console
	// commands are available without typing `enablecheats` each session. Only the
	// local controller drives console input, so we only need the cheat manager there.
	if (IsLocalController())
	{
		EnableCheats();
	}
#endif

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

	if (!UAuraAbilitySystemLibrary::IsNotFriend(PlayerCharacter, Enemy)
		|| UAuraAbilitySystemLibrary::IsNotFriend(PlayerCharacter, PlayerCharacter)
		|| UAuraAbilitySystemLibrary::IsNotFriend(Enemy, Enemy))
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

		if (MountedBroom)
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
			if (HasAuthority())
			{
				MountedBroom->RequestDismount();
			}
			else
			{
				ServerRequestBroomDismount(MountedBroom);
			}
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
	if (!MountedBroom)
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
