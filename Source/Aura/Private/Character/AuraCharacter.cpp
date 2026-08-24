// Copyright Druid Mechanics


#include "Character/AuraCharacter.h"

#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Game/AuraGameModeBase.h"
#include "Game/LoadScreenSaveGame.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/HUD/AuraHUD.h"
#include "Aura/AuraLogChannels.h"
#include "Building/AuraBuildingComponent.h"

AAuraCharacter::AAuraCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->SetUsingAbsoluteRotation(true);

	// Retract the camera toward the player when buildings/terrain block the boom's sight line.
	// Uses ECC_Camera so Pawns (other players/enemies) do not retract the camera. Fade-actor
	// walls are excluded by setting their mesh Camera response to Ignore (see BP_FadeActor).
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeChannel = ECC_Camera; // explicit; matches engine default
	CameraBoom->ProbeSize = 16.f;          // sphere-sweep clearance; tunable on the component in BP

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>("TopDownCameraComponent");
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;
	
	LevelUpNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("LevelUpNiagaraComponent");
	LevelUpNiagaraComponent->SetupAttachment(GetRootComponent());
	LevelUpNiagaraComponent->bAutoActivate = false;

	BuildingComponent = CreateDefaultSubobject<UAuraBuildingComponent>(TEXT("BuildingComponent"));

	OverheadNameText = CreateDefaultSubobject<UTextRenderComponent>("OverheadNameText");
	OverheadNameText->SetupAttachment(GetRootComponent());
	OverheadNameText->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	OverheadNameText->SetUsingAbsoluteRotation(true);
	OverheadNameText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	OverheadNameText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	OverheadNameText->SetWorldSize(28.f);
	OverheadNameText->SetTextRenderColor(FColor::White);
	OverheadNameText->SetText(FText::FromString(TEXT("Player")));
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = false;
	GetCharacterMovement()->bSnapToPlaneAtStart = false;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->JumpZVelocity = 600.f;
	// Allow limited lateral steering while airborne so the player can redirect or
	// add forward motion during a jump (e.g. jump from a standstill while holding
	// W). Momentum is still preserved — AirControl < 1 keeps the jump arc weighty
	// rather than feeling like free flight. Tunable per-BP on the movement comp.
	GetCharacterMovement()->AirControl = 0.5f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	Tags.AddUnique(FName("Player"));

	CharacterClass = ECharacterClass::Elementalist;
}

FAuraCombatIdentity AAuraCharacter::BuildDefaultCombatIdentity() const
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	FAuraCombatIdentity Identity;
	Identity.FactionTag = GameplayTags.Faction_Player;
	Identity.ControlTypeTag = GameplayTags.Control_Player;
	Identity.CombatProfileTag = GameplayTags.Combat_Unassigned;
	Identity.DeathPolicyTag = GameplayTags.Death_PlayerRespawn;
	Identity.bTargetable = true;
	Identity.bCanAttack = true;
	Identity.bCanBeDamaged = true;
	Identity.bAllowFriendlyFire = false;
	return Identity;
}

FGameplayTag AAuraCharacter::GetRequiredRoleEntityType() const
{
	return FAuraGameplayTags::Get().Entity_Player;
}

FGameplayTag AAuraCharacter::GetRequiredRoleControlType() const
{
	return FAuraGameplayTags::Get().Control_Player;
}

void AAuraCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFatalFallState(DeltaSeconds);
	UpdateOverheadNameFacingCamera();
}

void AAuraCharacter::Landed(const FHitResult& Hit)
{
	if (HasAuthority() && CurrentFallDuration > 0.f)
	{
		UE_LOG(LogAura, Log, TEXT("[Character][Server] Fatal fall canceled by landing: Character=%s FallDuration=%.2fs Location=%s"),
			*GetNameSafe(this), CurrentFallDuration, *GetActorLocation().ToCompactString());
	}

	Super::Landed(Hit);
	ResetFatalFallState();
}

void AAuraCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindPlayerNameDelegate();
	BindRoleDelegate();
	UpdateOverheadPlayerName();

	UE_LOG(LogAura, Log, TEXT("[Character][Server] PossessedBy called: Character=%s Controller=%s HasAuthority=%s"),
		*GetNameSafe(this), *GetNameSafe(NewController), HasAuthority() ? TEXT("true") : TEXT("false"));

	// Init ability actor info for the Server
	InitAbilityActorInfo();
	if (LoadProgress())
	{
		MarkCombatReady();
	}

	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		AuraGameMode->LoadWorldState(GetWorld());
	}
}

bool AAuraCharacter::LoadProgress()
{
	UE_LOG(LogAura, Log, TEXT("[Character][Server] Transactional LoadProgress: Character=%s Authority=%d"),
		*GetNameSafe(this), HasAuthority());
	if (!HasAuthority())
	{
		return false;
	}

	auto RejectLogin = [this](const FString& Reason)
	{
		UE_LOG(LogAura, Error, TEXT("[Role][Login] %s"), *Reason);
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetController()))
		{
			AuraPC->ClientRejectLogin(Reason);
		}
	};

	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	if (!AuraPlayerState || !Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
	{
		RejectLogin(TEXT("The persistent PlayerState ability system is unavailable."));
		return false;
	}

	AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this));
	const ULoadScreenSaveGame* SaveData = nullptr;
	FName AuthorizedRole = NAME_None;
	if (GetNetMode() != NM_Standalone)
	{
		if (!AuraPlayerState->HasPendingAcceptedRoleId())
		{
			RejectLogin(TEXT("The server did not retain an accepted role for this connection."));
			return false;
		}
		AuthorizedRole = AuraPlayerState->GetPendingAcceptedRoleId();
	}
	else
	{
		SaveData = AuraGameMode ? AuraGameMode->RetrieveInGameSaveData() : nullptr;
		AuthorizedRole = SaveData && !SaveData->Role.IsNone()
			? SaveData->Role
			: UAuraAbilitySystemLibrary::GetDefaultRole(this);
	}

	FString RoleError;
	if (!UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(
		UAuraAbilitySystemLibrary::GetRoleInfo(this), AuthorizedRole, RoleError))
	{
		RejectLogin(RoleError.IsEmpty() ? TEXT("The requested role is not available.") : RoleError);
		return false;
	}

	if (SaveData && !SaveData->bFirstTimeLoadIn)
	{
		AuraPlayerState->SetLevel(SaveData->PlayerLevel);
		AuraPlayerState->SetXP(SaveData->XP);
		AuraPlayerState->SetAttributePoints(SaveData->AttributePoints);
		AuraPlayerState->SetSpellPoints(SaveData->SpellPoints);
	}

	const FAuraRoleApplicationResult Result = ApplyRoleAtSpawn(AuthorizedRole, SaveData);
	if (!Result.bSuccess)
	{
		RejectLogin(Result.Message.IsEmpty()
			? FString::Printf(TEXT("Role '%s' could not be applied."), *AuthorizedRole.ToString())
			: Result.Message);
		return false;
	}
	return true;
}

void AAuraCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindPlayerNameDelegate();
	BindRoleDelegate();
	UpdateOverheadPlayerName();

	// Init ability actor info for the Client
	InitAbilityActorInfo();

	// Presentation is driven only by AAuraCharacterBase::AppliedRoleState replication.
	// PlayerState role is a persistent save/UI mirror and never authorizes a client fallback.

	if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(GetAttributeSet()))
	{
		UE_LOG(LogAura, Log, TEXT("[Character][Client] OnRep_PlayerState after InitAbilityActorInfo: Health=%.1f/%.1f Mana=%.1f/%.1f"),
			AuraAS->GetHealth(), AuraAS->GetMaxHealth(), AuraAS->GetMana(), AuraAS->GetMaxMana());
	}
}

void AAuraCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(BoundPlayerState))
	{
		BoundPlayerState->OnPlayerNameChangedDelegate.RemoveAll(this);
		BoundPlayerState->OnRoleChangedDelegate.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAuraCharacter::AddToXP_Implementation(int32 InXP)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToXP(InXP);
}

void AAuraCharacter::LevelUp_Implementation()
{
	MulticastLevelUpParticles();
}

void AAuraCharacter::MulticastLevelUpParticles_Implementation() const
{
	if (IsValid(LevelUpNiagaraComponent))
	{
		const FVector CameraLocation = TopDownCameraComponent->GetComponentLocation();
		const FVector NiagaraSystemLocation = LevelUpNiagaraComponent->GetComponentLocation();
		const FRotator ToCameraRotation = (CameraLocation - NiagaraSystemLocation).Rotation();
		LevelUpNiagaraComponent->SetWorldRotation(ToCameraRotation);
		LevelUpNiagaraComponent->Activate(true);
	}
}

int32 AAuraCharacter::GetXP_Implementation() const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetXP();
}

int32 AAuraCharacter::FindLevelForXP_Implementation(int32 InXP) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->LevelUpInfo->FindLevelForXP(InXP);
}

int32 AAuraCharacter::GetAttributePointsReward_Implementation(int32 Level) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->LevelUpInfo->LevelUpInformation[Level].AttributePointAward;
}

int32 AAuraCharacter::GetSpellPointsReward_Implementation(int32 Level) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->LevelUpInfo->LevelUpInformation[Level].SpellPointAward;
}

void AAuraCharacter::AddToPlayerLevel_Implementation(int32 InPlayerLevel)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToLevel(InPlayerLevel);

	if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		AuraASC->UpdateAbilityStatuses(AuraPlayerState->GetPlayerLevel());
	}
}

void AAuraCharacter::AddToAttributePoints_Implementation(int32 InAttributePoints)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToAttributePoints(InAttributePoints);
}

void AAuraCharacter::AddToSpellPoints_Implementation(int32 InSpellPoints)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToSpellPoints(InSpellPoints);
}

int32 AAuraCharacter::GetAttributePoints_Implementation() const
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetAttributePoints();
}

int32 AAuraCharacter::GetSpellPoints_Implementation() const
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetSpellPoints();
}

void AAuraCharacter::ShowMagicCircle_Implementation(UMaterialInterface* DecalMaterial)
{
	if (AAuraPlayerController* AuraPlayerController = Cast<AAuraPlayerController>(GetController()))
	{
		AuraPlayerController->ShowMagicCircle(DecalMaterial);
		AuraPlayerController->bShowMouseCursor = false;
	}
}

void AAuraCharacter::HideMagicCircle_Implementation()
{
	if (AAuraPlayerController* AuraPlayerController = Cast<AAuraPlayerController>(GetController()))
	{
		AuraPlayerController->HideMagicCircle();
		AuraPlayerController->bShowMouseCursor = true;
	}
}

void AAuraCharacter::SaveProgress_Implementation(const FName& CheckpointTag)
{
	AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (AuraGameMode)
	{
		ULoadScreenSaveGame* SaveData = AuraGameMode->RetrieveInGameSaveData();
		if (SaveData == nullptr) return;

		SaveData->PlayerStartTag = CheckpointTag;

		if (AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetPlayerState()))
		{
			SaveData->PlayerLevel = AuraPlayerState->GetPlayerLevel();
			SaveData->XP = AuraPlayerState->GetXP();
			SaveData->AttributePoints = AuraPlayerState->GetAttributePoints();
			SaveData->SpellPoints = AuraPlayerState->GetSpellPoints();
		}
		SaveData->Strength = UAuraAttributeSet::GetStrengthAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Intelligence = UAuraAttributeSet::GetIntelligenceAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Resilience = UAuraAttributeSet::GetResilienceAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Vigor = UAuraAttributeSet::GetVigorAttribute().GetNumericValue(GetAttributeSet());

		SaveData->bFirstTimeLoadIn = false;

		if (const AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetPlayerState()))
		{
			SaveData->Role = AuraPlayerState->GetRole();
		}

		if (!HasAuthority()) return;

		UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
		FForEachAbility SaveAbilityDelegate;
		SaveData->SavedAbilities.Empty();
		SaveAbilityDelegate.BindLambda([AuraASC, SaveData](const FGameplayAbilitySpec& AbilitySpec)
		{
			const FGameplayTag AbilityTag = AuraASC->GetAbilityTagFromSpec(AbilitySpec);

			FSavedAbility SavedAbility;
			SavedAbility.AbilityLevel = AbilitySpec.Level;
			SavedAbility.AbilitySlot = AuraASC->GetSlotFromAbilityTag(AbilityTag);
			SavedAbility.AbilityStatus = AuraASC->GetStatusFromAbilityTag(AbilityTag);
			SavedAbility.AbilityTag = AbilityTag;
			FName GrantedRoleId;
			SavedAbility.GrantSource = static_cast<uint8>(AuraASC->GetGrantSourceForSpec(AbilitySpec, GrantedRoleId));
			SavedAbility.GrantedRoleId = GrantedRoleId;
			SavedAbility.ProvenanceVersion = 1;
			SaveData->SavedAbilities.AddUnique(SavedAbility);

		});
		AuraASC->ForEachAbility(SaveAbilityDelegate);
		
		AuraGameMode->SaveInGameProgressData(SaveData);
	}
}

int32 AAuraCharacter::GetPlayerLevel_Implementation()
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetPlayerLevel();
}

void AAuraCharacter::Die(const FVector& DeathImpulse)
{
	if (!HasAuthority() || !TryBeginCombatDeath())
	{
		return;
	}

	ResetFatalFallState();
	Super::Die(DeathImpulse);

	TopDownCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

FGameplayTag AAuraCharacter::GetAuraTargetKind() const
{
	return FAuraGameplayTags::Get().Target_Kind_Player;
}

FText AAuraCharacter::GetAuraTargetDisplayName() const
{
	const APlayerState* State = GetPlayerState();
	return FText::FromString(State && !State->GetPlayerName().IsEmpty() ? State->GetPlayerName() : TEXT("Player"));
}

void AAuraCharacter::UpdateFatalFallState(float DeltaSeconds)
{
	if (!HasAuthority() || !IsCombatAlive() || !bEnableFallDeath)
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		return;
	}

	const bool bIsInFatalFallWindow = MovementComponent->IsFalling() && GetVelocity().Z < 0.f;
	if (!bIsInFatalFallWindow)
	{
		ResetFatalFallState();
		return;
	}

	CurrentFallDuration += DeltaSeconds;
	if (!bWasInFatalFallWindow)
	{
		bWasInFatalFallWindow = true;
		UE_LOG(LogAura, Log, TEXT("[Character][Server] Fatal fall tracking started: Character=%s Threshold=%.2fs Location=%s Velocity=%s"),
			*GetNameSafe(this), FatalFallDelay, *GetActorLocation().ToCompactString(), *GetVelocity().ToCompactString());
	}

	if (CurrentFallDuration >= FatalFallDelay)
	{
		UE_LOG(LogAura, Warning, TEXT("[Character][Server] Fatal fall threshold reached: Character=%s FallDuration=%.2fs Location=%s Velocity=%s"),
			*GetNameSafe(this), CurrentFallDuration, *GetActorLocation().ToCompactString(), *GetVelocity().ToCompactString());
		ResetFatalFallState();
		Die(FVector::ZeroVector);
	}
}

void AAuraCharacter::ResetFatalFallState()
{
	CurrentFallDuration = 0.f;
	bWasInFatalFallWindow = false;
}

void AAuraCharacter::OnRep_Stunned()
{
	if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
	{
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		FGameplayTagContainer BlockedTags;
		BlockedTags.AddTag(GameplayTags.Player_Block_CursorTrace);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputHeld);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputPressed);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputReleased);
		if (bIsStunned)
		{
			AuraASC->AddLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Activate();
		}
		else
		{
			AuraASC->RemoveLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Deactivate();
		}
	}
}

void AAuraCharacter::OnRep_Burned()
{
	if (bIsBurned)
	{
		BurnDebuffComponent->Activate();
	}
	else
	{
		BurnDebuffComponent->Deactivate();
	}
}

void AAuraCharacter::BindPlayerNameDelegate()
{
	AAuraPlayerState* CurrentPlayerState = GetPlayerState<AAuraPlayerState>();
	if (CurrentPlayerState == BoundPlayerState)
	{
		return;
	}

	if (IsValid(BoundPlayerState))
	{
		BoundPlayerState->OnPlayerNameChangedDelegate.RemoveAll(this);
	}

	BoundPlayerState = CurrentPlayerState;
	if (IsValid(BoundPlayerState))
	{
		BoundPlayerState->OnPlayerNameChangedDelegate.AddUObject(this, &AAuraCharacter::HandlePlayerNameChanged);
	}
}

void AAuraCharacter::UpdateOverheadPlayerName()
{
	if (!IsValid(OverheadNameText))
	{
		return;
	}

	FString PlayerName = TEXT("Player");
	if (const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>())
	{
		if (!AuraPlayerState->GetPlayerName().IsEmpty())
		{
			PlayerName = AuraPlayerState->GetPlayerName();
		}
	}

	OverheadNameText->SetText(FText::FromString(PlayerName));
}

void AAuraCharacter::UpdateOverheadNameFacingCamera()
{
	if (!IsValid(OverheadNameText) || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = CameraManager->GetCameraLocation() - OverheadNameText->GetComponentLocation();
		if (!ToCamera.IsNearlyZero())
		{
			const FRotator LookAtRotation = ToCamera.Rotation();
			OverheadNameText->SetWorldRotation(FRotator(0.f, LookAtRotation.Yaw, 0.f));
		}
	}
}

void AAuraCharacter::HandlePlayerNameChanged(const FString& NewName)
{
	if (!IsValid(OverheadNameText))
	{
		return;
	}

	const FString SafeName = NewName.IsEmpty() ? TEXT("Player") : NewName;
	OverheadNameText->SetText(FText::FromString(SafeName));
}

void AAuraCharacter::BindRoleDelegate()
{
	// BoundPlayerState is maintained by BindPlayerNameDelegate (called just before this).
	// RemoveAll + Add avoids duplicate bindings if this runs more than once.
	if (IsValid(BoundPlayerState))
	{
		BoundPlayerState->OnRoleChangedDelegate.RemoveAll(this);
		BoundPlayerState->OnRoleChangedDelegate.AddUObject(this, &AAuraCharacter::HandleRoleChanged);
	}
}

void AAuraCharacter::HandleRoleChanged(FName NewRole)
{
	if (GetAppliedRoleState().IsValid() && GetAppliedRoleState().RoleId != NewRole)
	{
		UE_LOG(LogAura, Error, TEXT("[Role][Mirror] PlayerState role '%s' does not match pawn applied role '%s'."),
			*NewRole.ToString(), *GetAppliedRoleState().RoleId.ToString());
	}
}

void AAuraCharacter::InitAbilityActorInfo()
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(AuraPlayerState, this);
	Cast<UAuraAbilitySystemComponent>(AuraPlayerState->GetAbilitySystemComponent())->AbilityActorInfoSet();
	AbilitySystemComponent = AuraPlayerState->GetAbilitySystemComponent();
	AttributeSet = AuraPlayerState->GetAttributeSet();
	OnAscRegistered.Broadcast(AbilitySystemComponent);
	AbilitySystemComponent->RegisterGameplayTagEvent(FAuraGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAuraCharacter::StunTagChanged);

	if (AAuraPlayerController* AuraPlayerController = Cast<AAuraPlayerController>(GetController()))
	{
		if (AAuraHUD* AuraHUD = Cast<AAuraHUD>(AuraPlayerController->GetHUD()))
		{
			AuraHUD->InitOverlay(AuraPlayerController, AuraPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}
}
