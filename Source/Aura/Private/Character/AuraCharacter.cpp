// Copyright Druid Mechanics


#include "Character/AuraCharacter.h"

#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
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

AAuraCharacter::AAuraCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>("TopDownCameraComponent");
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;
	
	LevelUpNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("LevelUpNiagaraComponent");
	LevelUpNiagaraComponent->SetupAttachment(GetRootComponent());
	LevelUpNiagaraComponent->bAutoActivate = false;

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
	// Keep jump motion momentum-driven: no lateral steering while airborne.
	GetCharacterMovement()->AirControl = 0.f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	CharacterClass = ECharacterClass::Elementalist;
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
	UpdateOverheadPlayerName();

	UE_LOG(LogAura, Log, TEXT("[Character][Server] PossessedBy called: Character=%s Controller=%s HasAuthority=%s"),
		*GetNameSafe(this), *GetNameSafe(NewController), HasAuthority() ? TEXT("true") : TEXT("false"));

	// Init ability actor info for the Server
	InitAbilityActorInfo();
	LoadProgress();

	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		AuraGameMode->LoadWorldState(GetWorld());
	}
}

void AAuraCharacter::LoadProgress()
{
	UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress enter: Character=%s HasAuthority=%s"),
		*GetNameSafe(this), HasAuthority() ? TEXT("true") : TEXT("false"));

	auto InitializeFallbackDefaults = [this]()
	{
		InitializeDefaultAttributes();
		AddCharacterAbilities();

		if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(GetAttributeSet()))
		{
			UE_LOG(LogAura, Warning, TEXT("[Character][Server] Fallback defaults applied: Health=%.1f/%.1f Mana=%.1f/%.1f"),
				AuraAS->GetHealth(), AuraAS->GetMaxHealth(), AuraAS->GetMana(), AuraAS->GetMaxMana());
		}
	};

	AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (AuraGameMode)
	{
		ULoadScreenSaveGame* SaveData = AuraGameMode->RetrieveInGameSaveData();
		if (SaveData == nullptr)
		{
			UE_LOG(LogAura, Warning, TEXT("[Character][Server] LoadProgress: SaveData is null for Character=%s. Applying fallback defaults."), *GetNameSafe(this));
			InitializeFallbackDefaults();
			return;
		}

		UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress: SaveData found FirstTime=%s Level=%d"),
			SaveData->bFirstTimeLoadIn ? TEXT("true") : TEXT("false"), SaveData->PlayerLevel);

		if (SaveData->bFirstTimeLoadIn)
		{
			InitializeDefaultAttributes();
			AddCharacterAbilities();

			if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(GetAttributeSet()))
			{
				UE_LOG(LogAura, Log, TEXT("[Character][Server] FirstLoad attributes initialized: Health=%.1f/%.1f Mana=%.1f/%.1f"),
					AuraAS->GetHealth(), AuraAS->GetMaxHealth(), AuraAS->GetMana(), AuraAS->GetMaxMana());
			}
		}
		else
		{
			if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
			{
				AuraASC->AddCharacterAbilitiesFromSaveData(SaveData);
			}
			
			if (AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetPlayerState()))
			{
				AuraPlayerState->SetLevel(SaveData->PlayerLevel);
				AuraPlayerState->SetXP(SaveData->XP);
				AuraPlayerState->SetAttributePoints(SaveData->AttributePoints);
				AuraPlayerState->SetSpellPoints(SaveData->SpellPoints);
			}
			
			UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(this, AbilitySystemComponent, SaveData);

			if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(GetAttributeSet()))
			{
				UE_LOG(LogAura, Log, TEXT("[Character][Server] SaveData attributes initialized: Health=%.1f/%.1f Mana=%.1f/%.1f Level=%d"),
					AuraAS->GetHealth(), AuraAS->GetMaxHealth(), AuraAS->GetMana(), AuraAS->GetMaxMana(), SaveData->PlayerLevel);
			}
		}
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("[Character][Server] LoadProgress: AuraGameMode is null for Character=%s. Applying fallback defaults."), *GetNameSafe(this));
		InitializeFallbackDefaults();
	}
}

void AAuraCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindPlayerNameDelegate();
	UpdateOverheadPlayerName();

	// Init ability actor info for the Client
	InitAbilityActorInfo();

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

		if (!HasAuthority()) return;

		UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
		FForEachAbility SaveAbilityDelegate;
		SaveData->SavedAbilities.Empty();
		SaveAbilityDelegate.BindLambda([this, AuraASC, SaveData](const FGameplayAbilitySpec& AbilitySpec)
		{
			const FGameplayTag AbilityTag = AuraASC->GetAbilityTagFromSpec(AbilitySpec);
			UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(this);
			FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);

			FSavedAbility SavedAbility;
			SavedAbility.GameplayAbility = Info.Ability;
			SavedAbility.AbilityLevel = AbilitySpec.Level;
			SavedAbility.AbilitySlot = AuraASC->GetSlotFromAbilityTag(AbilityTag);
			SavedAbility.AbilityStatus = AuraASC->GetStatusFromAbilityTag(AbilityTag);
			SavedAbility.AbilityTag = AbilityTag;
			SavedAbility.AbilityType = Info.AbilityType;

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
	if (bDead)
	{
		return;
	}

	ResetFatalFallState();
	Super::Die(DeathImpulse);

	if (AAuraGameModeBase* AuraGM = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		AuraGM->PlayerDied(this, DeathTime);
	}

	TopDownCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

void AAuraCharacter::UpdateFatalFallState(float DeltaSeconds)
{
	if (!HasAuthority() || bDead || !bEnableFallDeath)
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
