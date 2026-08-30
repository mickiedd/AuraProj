// Copyright Druid Mechanics


#include "Player/AuraPlayerState.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraInventoryComponent.h"
#include "Game/AuraPlayerSaveGame.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/Crc.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Aura/AuraLogChannels.h"

AAuraPlayerState::AAuraPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAuraAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAuraAttributeSet>("AttributeSet");
	CurrencyComponent = CreateDefaultSubobject<UAuraCurrencyComponent>(TEXT("CurrencyComponent"));
	InventoryComponent = CreateDefaultSubobject<UAuraInventoryComponent>(TEXT("InventoryComponent"));
	
	SetNetUpdateFrequency(100.f);
}

void AAuraPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelFirearmReload(TEXT("EndPlay"));
	Super::EndPlay(EndPlayReason);
}

void AAuraPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AAuraPlayerState, Level);
	DOREPLIFETIME(AAuraPlayerState, XP);
	DOREPLIFETIME(AAuraPlayerState, AttributePoints);
	DOREPLIFETIME(AAuraPlayerState, SpellPoints);
	DOREPLIFETIME(AAuraPlayerState, CharacterRole);
	DOREPLIFETIME_CONDITION(AAuraPlayerState, EconomyInitializationState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAuraPlayerState, EconomyInitializationCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAuraPlayerState, FirearmState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAuraPlayerState, TutorialCompletionMask, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAuraPlayerState, RecoveryState, COND_OwnerOnly);
}

UAbilitySystemComponent* AAuraPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAuraPlayerState::SetPlayerName(const FString& S)
{
	Super::SetPlayerName(S);
	OnPlayerNameChangedDelegate.Broadcast(GetPlayerName());
}

void AAuraPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	OnPlayerNameChangedDelegate.Broadcast(GetPlayerName());
}

void AAuraPlayerState::AddToXP(int32 InXP)
{
	XP += InXP;
	OnXPChangedDelegate.Broadcast(XP);
}

void AAuraPlayerState::AddToLevel(int32 InLevel)
{
	Level += InLevel;
	OnLevelChangedDelegate.Broadcast(Level, true);
}

void AAuraPlayerState::SetXP(int32 InXP)
{
	XP = InXP;
	OnXPChangedDelegate.Broadcast(XP);
}

void AAuraPlayerState::SetLevel(int32 InLevel)
{
	Level = InLevel;
	OnLevelChangedDelegate.Broadcast(Level, false);
}

void AAuraPlayerState::SetAttributePoints(int32 InPoints)
{
	AttributePoints = InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAuraPlayerState::SetSpellPoints(int32 InPoints)
{
	SpellPoints = InPoints;
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}

bool AAuraPlayerState::SetRole(FName InRole)
{
	if (!HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[Role][PlayerState] Rejected non-authority mutation to '%s' on %s."),
			*InRole.ToString(), *GetNameSafe(this));
		return false;
	}
	if (CharacterRole == InRole)
	{
		return true;
	}
	CharacterRole = InRole;
	InitializeFirearmForRole(InRole);
	OnRoleChangedDelegate.Broadcast(CharacterRole);
	ForceNetUpdate();
	return true;
}

bool AAuraPlayerState::InitializeFirearmForRole(FName InRole, int32 InMagazineCapacity, int32 InReserveCapacity,
	float InReloadDuration, float InMinimumShotInterval)
{
	if (!HasAuthority() || InMagazineCapacity <= 0 || InReserveCapacity <= 0 || InReloadDuration < 0.f || InMinimumShotInterval <= 0.f)
	{
		return false;
	}
	CancelFirearmReload(TEXT("RoleInitialize"));
	const bool bIsBungeeMan = InRole == TEXT("BungeeMan");
	FirearmState = FAuraFirearmState();
	FirearmState.bApplicable = bIsBungeeMan;
	if (bIsBungeeMan)
	{
		FirearmState.MagazineCapacity = InMagazineCapacity;
		FirearmState.MagazineRounds = InMagazineCapacity;
		FirearmState.ReserveCapacity = InReserveCapacity;
		FirearmState.ReserveRounds = InReserveCapacity;
		FirearmState.ReloadDuration = InReloadDuration;
		FirearmState.FireMode = TEXT("SemiAuto");
		FirearmState.MinimumShotInterval = InMinimumShotInterval;
		FirearmState.AmmoRevision = 1;
	}
	ForceNetUpdate();
	OnFirearmStateChanged.Broadcast(FirearmState);
	return true;
}

bool AAuraPlayerState::TryConsumeFirearmRound(FName AbilityId, AActor* AvatarActor, FName& OutResultCode)
{
	OutResultCode = NAME_None;
	if (!HasAuthority()) { OutResultCode = TEXT("NotAuthority"); return false; }
	if (AbilityId != TEXT("FireGun")) { OutResultCode = TEXT("InvalidAbility"); return false; }
	if (!FirearmState.bApplicable) { OutResultCode = TEXT("NotApplicable"); return false; }
	if (const UAuraCombatStateComponent* Life = UAuraCombatStateComponent::FindForActor(AvatarActor); !Life || !Life->IsAlive())
	{
		OutResultCode = TEXT("NotAlive");
		return false;
	}
	if (FirearmState.bReloading) { OutResultCode = TEXT("Reloading"); return false; }
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastAcceptedFirearmShotTime >= 0.0 && Now - LastAcceptedFirearmShotTime + KINDA_SMALL_NUMBER < FirearmState.MinimumShotInterval)
	{
		OutResultCode = TEXT("CadenceLimited");
		return false;
	}
	if (FirearmState.MagazineRounds < 1) { OutResultCode = TEXT("EmptyMagazine"); return false; }
	--FirearmState.MagazineRounds;
	++FirearmState.AmmoRevision;
	LastAcceptedFirearmShotTime = Now;
	ForceNetUpdate();
	OnFirearmStateChanged.Broadcast(FirearmState);
	return true;
}

void AAuraPlayerState::NotifyFirearmShotAccepted(FName AbilityId)
{
	if (HasAuthority() && AbilityId == TEXT("FireGun"))
	{
		UE_LOG(LogAura, Display, TEXT("[Firearm][Server] Accepted ability=%s magazine=%d reserve=%d revision=%u."),
			*AbilityId.ToString(), FirearmState.MagazineRounds, FirearmState.ReserveRounds, FirearmState.AmmoRevision);
	}
}

void AAuraPlayerState::NotifyAuthoritativeAbilityCommitted(FName AbilityId)
{
	if (!HasAuthority() || AbilityId.IsNone()) return;
	SetTutorialStepCompleted(2, true);
}

bool AAuraPlayerState::RestoreCompletedFirearmState(bool bInApplicable, int32 InMagazineCapacity, int32 InMagazineRounds,
	int32 InReserveCapacity, int32 InReserveRounds, float InReloadDuration, uint32 InAmmoRevision, FString& OutError)
{
	OutError.Reset();
	if (!HasAuthority() || InMagazineCapacity < 0 || InReserveCapacity < 0 || InMagazineRounds < 0
		|| InReserveRounds < 0 || InMagazineRounds > InMagazineCapacity || InReserveRounds > InReserveCapacity || InReloadDuration < 0.f)
	{
		OutError = TEXT("Completed firearm state failed authority or bounds validation.");
		return false;
	}
	CancelFirearmReload(TEXT("Restore"));
	FirearmState.bApplicable = bInApplicable;
	FirearmState.MagazineCapacity = bInApplicable ? InMagazineCapacity : 0;
	FirearmState.MagazineRounds = bInApplicable ? InMagazineRounds : 0;
	FirearmState.ReserveCapacity = bInApplicable ? InReserveCapacity : 0;
	FirearmState.ReserveRounds = bInApplicable ? InReserveRounds : 0;
	FirearmState.ReloadDuration = bInApplicable ? InReloadDuration : 0.f;
	FirearmState.FireMode = bInApplicable ? FName(TEXT("SemiAuto")) : NAME_None;
	FirearmState.MinimumShotInterval = bInApplicable ? 0.2f : 0.f;
	FirearmState.AmmoRevision = bInApplicable ? FMath::Max(1u, InAmmoRevision) : 0;
	ForceNetUpdate();
	OnFirearmStateChanged.Broadcast(FirearmState);
	return true;
}

bool AAuraPlayerState::SetTutorialStepCompleted(uint8 StepIndex, bool bCompleted)
{
	if (!HasAuthority() || StepIndex >= 32) return false;
	const uint32 Bit = (1u << StepIndex);
	const uint32 PreviousMask = TutorialCompletionMask;
	if (bCompleted) TutorialCompletionMask |= Bit; else TutorialCompletionMask &= ~Bit;
	if (TutorialCompletionMask == PreviousMask) return true;
	ForceNetUpdate();
	OnTutorialProgressChanged.Broadcast(TutorialCompletionMask);
	return true;
}

bool AAuraPlayerState::ResetTutorialProgress()
{
	if (!HasAuthority()) return false;
	if (TutorialCompletionMask == 0) return true;
	TutorialCompletionMask = 0;
	ForceNetUpdate();
	OnTutorialProgressChanged.Broadcast(TutorialCompletionMask);
	return true;
}

bool AAuraPlayerState::SetRecoveryState(FName InState)
{
	if (!HasAuthority() || (InState != TEXT("Alive") && InState != TEXT("Dying") && InState != TEXT("Dead") && InState != TEXT("Recovering"))) return false;
	RecoveryState = InState;
	ForceNetUpdate();
	return true;
}

bool AAuraPlayerState::BeginFirearmReload(FName& OutResultCode)
{
	OutResultCode = NAME_None;
	if (!HasAuthority()) { OutResultCode = TEXT("NotAuthority"); return false; }
	if (!FirearmState.bApplicable) { OutResultCode = TEXT("NotApplicable"); return false; }
	if (const APawn* Avatar = GetPawn(); !Avatar || !UAuraCombatStateComponent::FindForActor(Avatar)
		|| !UAuraCombatStateComponent::FindForActor(Avatar)->IsAlive())
	{
		OutResultCode = TEXT("NotAlive");
		return false;
	}
	if (FirearmState.bReloading) { OutResultCode = TEXT("AlreadyReloading"); return false; }
	if (FirearmState.MagazineRounds >= FirearmState.MagazineCapacity) { OutResultCode = TEXT("MagazineFull"); return false; }
	if (FirearmState.ReserveRounds <= 0) { OutResultCode = TEXT("NoReserve"); return false; }
	FirearmState.bReloading = true;
	++FirearmState.ReloadSerial;
	const uint32 Serial = FirearmState.ReloadSerial;
	ForceNetUpdate();
	OnFirearmStateChanged.Broadcast(FirearmState);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FirearmReloadTimerHandle,
			FTimerDelegate::CreateUObject(this, &AAuraPlayerState::FinishFirearmReload, Serial),
			FirearmState.ReloadDuration, false);
	}
	return true;
}

void AAuraPlayerState::CancelFirearmReload(const TCHAR* Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FirearmReloadTimerHandle);
	if (FirearmState.bReloading)
	{
		FirearmState.bReloading = false;
		++FirearmState.ReloadSerial;
		ForceNetUpdate();
		OnFirearmStateChanged.Broadcast(FirearmState);
		UE_LOG(LogAura, Display, TEXT("[Firearm][Server] Reload canceled reason=%s serial=%u."), Reason, FirearmState.ReloadSerial);
	}
}

void AAuraPlayerState::FinishFirearmReload(uint32 ExpectedSerial)
{
	if (!HasAuthority() || !FirearmState.bReloading || ExpectedSerial != FirearmState.ReloadSerial) return;
	const int32 Needed = FMath::Max(0, FirearmState.MagazineCapacity - FirearmState.MagazineRounds);
	const int32 Loaded = FMath::Min(Needed, FirearmState.ReserveRounds);
	FirearmState.MagazineRounds += Loaded;
	FirearmState.ReserveRounds -= Loaded;
	FirearmState.bReloading = false;
	++FirearmState.AmmoRevision;
	ForceNetUpdate();
	OnFirearmStateChanged.Broadcast(FirearmState);
	UE_LOG(LogAura, Display, TEXT("[Firearm][Server] Reload completed loaded=%d magazine=%d reserve=%d revision=%u."),
		Loaded, FirearmState.MagazineRounds, FirearmState.ReserveRounds, FirearmState.AmmoRevision);
}

bool AAuraPlayerState::IsReadyForPersistentSave() const
{
	return EconomyInitializationState != EAuraEconomyInitializationState::NewEphemeralSession
		&& !CharacterRole.IsNone() && HasInitializedDefaultAttributes();
}

bool AAuraPlayerState::InitializeEconomyForNewProfileOnce()
{
	if (!HasAuthority()) return false;
	if (EconomyInitializationState != EAuraEconomyInitializationState::NewEphemeralSession) return true;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UAuraEconomyRegistrySubsystem* Registry = GameInstance ? GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>() : nullptr;
	if (!Registry || !Registry->IsReady() || !CurrencyComponent || !InventoryComponent)
	{
		UE_LOG(LogAura, Error, TEXT("[Economy][PlayerState] Refused initialization for %s because the authority registry is not ready."), *GetNameSafe(this));
		return false;
	}
	const FAuraEconomySettings& Settings = Registry->GetSnapshot()->Settings;
	if (!CurrencyComponent->InitializeCurrency(Settings.CurrencyId, Settings.StartingBalance))
	{
		UE_LOG(LogAura, Error, TEXT("[Economy][PlayerState] Starting balance initialization failed for %s."), *GetNameSafe(this));
		return false;
	}
	EconomyInitializationState = EAuraEconomyInitializationState::Initialized;
	++EconomyInitializationCount;
	ForceNetUpdate();
	UE_LOG(LogAura, Display, TEXT("[Economy][PlayerState] Initialized new ephemeral session player=%s currency=%s balance=%lld count=%d."),
		*GetNameSafe(this), *Settings.CurrencyId.ToString(), Settings.StartingBalance, EconomyInitializationCount);
	return true;
}

bool AAuraPlayerState::ApplyPersistentProfile(const UAuraPlayerSaveGame& SaveData, FString& OutError)
{
	OutError.Reset();
	if (!HasAuthority() || SaveData.bFirstTimeLoadIn || SaveData.CurrencyId.IsNone() || SaveData.CurrencyBalance < 0
		|| !CurrencyComponent || !InventoryComponent)
	{
		OutError = TEXT("Persistent profile is not a complete authority-owned record.");
		return false;
	}
	const TArray<FAuraInventorySlot> PreviousInventorySlots = InventoryComponent->GetSlots();
	const uint32 PreviousInventoryRevision = InventoryComponent->GetRevision();
	if (!InventoryComponent->RestoreInventoryState(SaveData.InventorySlots, SaveData.InventoryRevision, false))
	{
		OutError = TEXT("Persistent inventory failed authoritative validation.");
		return false;
	}
	if (!CurrencyComponent->RestoreCurrencyState(SaveData.CurrencyId, SaveData.CurrencyBalance, SaveData.CurrencyRevision, false))
	{
		const bool bRolledBack = InventoryComponent->RestoreInventoryState(PreviousInventorySlots, PreviousInventoryRevision, false);
		OutError = bRolledBack
			? TEXT("Persistent wallet failed authoritative validation; profile application was rolled back.")
			: TEXT("Persistent wallet failed and inventory rollback also failed; profile state is unsafe.");
		return false;
	}
	FString FirearmError;
	if (!RestoreCompletedFirearmState(SaveData.bFirearmApplicable, SaveData.FirearmMagazineCapacity, SaveData.FirearmMagazineRounds,
		SaveData.FirearmReserveCapacity, SaveData.FirearmReserveRounds, SaveData.FirearmReloadDuration, SaveData.FirearmAmmoRevision, FirearmError))
	{
		CurrencyComponent->RestoreCurrencyState(SaveData.CurrencyId, SaveData.CurrencyBalance, SaveData.CurrencyRevision, false);
		InventoryComponent->RestoreInventoryState(PreviousInventorySlots, PreviousInventoryRevision, false);
		OutError = FirearmError;
		return false;
	}
	TutorialCompletionMask = SaveData.TutorialCompletionMask;
	OnTutorialProgressChanged.Broadcast(TutorialCompletionMask);
	RecoveryState = SaveData.RecoveryState.IsNone() ? FName(TEXT("Alive")) : SaveData.RecoveryState;
	if (RecoveryState == TEXT("Dead") || RecoveryState == TEXT("Recovering"))
	{
		// A persisted life state is an input to one server-owned recovery
		// transition, never a client-controlled direct respawn.
		RecoveryState = TEXT("Recovering");
	}
	CurrencyComponent->PublishChanged();
	InventoryComponent->PublishChanged();
	Level = FMath::Max(1, SaveData.PlayerLevel);
	XP = FMath::Max(0, SaveData.XP);
	AttributePoints = FMath::Max(0, SaveData.AttributePoints);
	SpellPoints = FMath::Max(0, SaveData.SpellPoints);
	EconomyInitializationState = EAuraEconomyInitializationState::LoadedPersistent;
	EconomyInitializationCount = 1;
	UE_LOG(LogAura, Display, TEXT("[Persistence][Profile] Applied identityHash=%08X role=%s level=%d balance=%lld inventorySlots=%d."),
		FCrc::StrCrc32(*ProfileIdentity.ToCanonicalString()), *SaveData.Role.ToString(), Level,
		SaveData.CurrencyBalance, SaveData.InventorySlots.Num());
	OnXPChangedDelegate.Broadcast(XP);
	OnLevelChangedDelegate.Broadcast(Level, false);
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
	return true;
}

bool AAuraPlayerState::HasInitializedDefaultAttributes() const
{
	const UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	return AuraASC && AuraASC->GetRoleGrantLedger().bAttributesInitialized;
}

void AAuraPlayerState::MarkDefaultAttributesInitialized()
{
	if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
	{
		AuraASC->MarkRoleAttributesInitialized();
	}
}

void AAuraPlayerState::OnRep_Level(int32 OldLevel)
{
	OnLevelChangedDelegate.Broadcast(Level, true);
}

void AAuraPlayerState::OnRep_XP(int32 OldXP)
{
	OnXPChangedDelegate.Broadcast(XP);
}

void AAuraPlayerState::OnRep_AttributePoints(int32 OldAttributePoints)
{
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAuraPlayerState::OnRep_SpellPoints(int32 OldSpellPoints)
{
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}

void AAuraPlayerState::OnRep_Role()
{
	UE_LOG(LogAura, Log, TEXT("[Role][Client] OnRep_Role: received Role='%s' for %s."), *CharacterRole.ToString(), *GetNameSafe(this));
	OnRoleChangedDelegate.Broadcast(CharacterRole);
}

void AAuraPlayerState::OnRep_EconomyInitializationState()
{
	UE_LOG(LogAura, Log, TEXT("[Economy][Client] PlayerState=%s initialization state=%d."), *GetNameSafe(this), static_cast<int32>(EconomyInitializationState));
}

void AAuraPlayerState::OnRep_FirearmState()
{
	OnFirearmStateChanged.Broadcast(FirearmState);
}

void AAuraPlayerState::OnRep_TutorialCompletionMask()
{
	OnTutorialProgressChanged.Broadcast(TutorialCompletionMask);
}

void AAuraPlayerState::AddToAttributePoints(int32 InPoints)
{
	AttributePoints += InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void AAuraPlayerState::AddToSpellPoints(int32 InPoints)
{
	SpellPoints += InPoints;
	OnSpellPointsChangedDelegate.Broadcast(SpellPoints);
}
