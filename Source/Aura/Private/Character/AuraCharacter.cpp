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
	UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress enter: Character=%s HasAuthority=%s"),
		*GetNameSafe(this), HasAuthority() ? TEXT("true") : TEXT("false"));

	// Role config is incomplete (e.g. placeholder Bonman/Ironman with empty mesh/anim): refuse to
	// log in with a broken role — log an Error and send the client back to the Login level with an
	// alert via the existing server-lost path (reused by KickOutSelf / mid-game server loss).
	auto RejectLogin = [this](const FString& Reason)
	{
		UE_LOG(LogAura, Error, TEXT("[Role][Login] %s"), *Reason);
		if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(GetController()))
		{
			AuraPC->ClientRejectLogin(Reason);
		}
		else
		{
			UE_LOG(LogAura, Error, TEXT("[Role][Login] No PlayerController for Character=%s to reject login."), *GetNameSafe(this));
		}
	};

	AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetPlayerState());
	const bool bAttributesAlreadyInitialized = AuraPlayerState && AuraPlayerState->HasInitializedDefaultAttributes();

	// The player ASC and AttributeSet are owned by PlayerState and survive pawn replacement.
	// Reapplying additive default GameplayEffects here would increase MaxHealth/MaxMana on every
	// respawn. A respawn only needs to refill the existing vital attributes.
	if (bAttributesAlreadyInitialized)
	{
		UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress: persistent attributes already initialized; refilling vitals for respawn."));
		UAuraAbilitySystemLibrary::TopOffVitalAttributes(AbilitySystemComponent, this);
	}

	auto InitializeFallbackDefaults = [this, AuraPlayerState, bAttributesAlreadyInitialized]()
	{
		// No save data (e.g. direct connect to a dedicated server with no slot): load the default
		// role from RoleConfig.json ("defaultRole"). Replicate it via PlayerState so clients render
		// the same role, apply visuals/abilities, and apply attributes from JSON.
		const FName DefaultRole = UAuraAbilitySystemLibrary::GetDefaultRole(this);
		UE_LOG(LogAura, Log, TEXT("[Character][Server] Fallback: loading default role = '%s'."), *DefaultRole.ToString());

		if (AuraPlayerState)
		{
			AuraPlayerState->SetRole(DefaultRole);
		}
		ApplyRole(DefaultRole);
		if (!bAttributesAlreadyInitialized)
		{
			InitializeDefaultAttributesForRole(DefaultRole);
			if (AuraPlayerState) AuraPlayerState->MarkDefaultAttributesInitialized();
			AddCharacterAbilities();
		}

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

			// Refuse login if the default role is not fully configured (empty mesh/animation).
			if (URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this))
			{
				const FName DefaultRole = UAuraAbilitySystemLibrary::GetDefaultRole(this);
				if (!RoleInfo->IsRoleConfigured(DefaultRole))
				{
					RejectLogin(FString::Printf(TEXT("Default role '%s' is not fully configured in RoleConfig.json (empty mesh or animation). Login refused."),
						*DefaultRole.ToString()));
					return false;
				}
			}

			if (!bAttributesAlreadyInitialized)
			{
				InitializeFallbackDefaults();
			}
			return true;
		}

		UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress: SaveData found FirstTime=%s Level=%d"),
			SaveData->bFirstTimeLoadIn ? TEXT("true") : TEXT("false"), SaveData->PlayerLevel);

		// Refuse login if the saved role is not fully configured (empty mesh/animation).
		if (URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(this))
		{
			if (!RoleInfo->IsRoleConfigured(SaveData->Role))
			{
				RejectLogin(FString::Printf(TEXT("Role '%s' is not fully configured in RoleConfig.json (empty mesh or animation). Login refused."),
					*SaveData->Role.ToString()));
				return false;
			}
		}

		// Apply the saved Role before gameplay init: visuals + (server) replicate via PlayerState.
		// Copying the role's DefaultPrimaryAttributes / StartupAbilities onto this character makes
		// the InitializeDefaultAttributes() / AddCharacterAbilities() calls below consume role data.
		UE_LOG(LogAura, Log, TEXT("[Character][Server] LoadProgress: SaveData Role='%s' — applying role + replicating via PlayerState."), *SaveData->Role.ToString());
		if (AuraPlayerState)
		{
			AuraPlayerState->SetRole(SaveData->Role);
		}
		ApplyRole(SaveData->Role);

		if (SaveData->bFirstTimeLoadIn && !bAttributesAlreadyInitialized)
		{
			InitializeDefaultAttributesForRole(SaveData->Role);
			if (AuraPlayerState) AuraPlayerState->MarkDefaultAttributesInitialized();
			AddCharacterAbilities();

			if (const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(GetAttributeSet()))
			{
				UE_LOG(LogAura, Log, TEXT("[Character][Server] FirstLoad attributes initialized: Health=%.1f/%.1f Mana=%.1f/%.1f"),
					AuraAS->GetHealth(), AuraAS->GetMaxHealth(), AuraAS->GetMana(), AuraAS->GetMaxMana());
			}
		}
		else if (!bAttributesAlreadyInitialized)
		{
			if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
			{
				AuraASC->AddCharacterAbilitiesFromSaveData(SaveData);
			}
			
			if (AuraPlayerState)
			{
				AuraPlayerState->SetLevel(SaveData->PlayerLevel);
				AuraPlayerState->SetXP(SaveData->XP);
				AuraPlayerState->SetAttributePoints(SaveData->AttributePoints);
				AuraPlayerState->SetSpellPoints(SaveData->SpellPoints);
			}
			
			UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(this, AbilitySystemComponent, SaveData);
			if (AuraPlayerState) AuraPlayerState->MarkDefaultAttributesInitialized();

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
		if (!bAttributesAlreadyInitialized)
		{
			InitializeFallbackDefaults();
		}
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

	// Apply this player's chosen Role on the client (visuals only; gameplay is server-granted).
	if (const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>())
	{
		ApplyRole(AuraPlayerState->GetRole());
	}

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

	if (GetCombatLifeState() == EAuraCombatLifeState::Dead)
	{
		if (AAuraGameModeBase* AuraGM = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(this)))
		{
			AuraGM->PlayerDied(this, DeathTime);
		}
	}

	TopDownCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
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
	ApplyRole(NewRole);
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
