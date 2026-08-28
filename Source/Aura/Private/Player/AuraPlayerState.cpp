// Copyright Druid Mechanics


#include "Player/AuraPlayerState.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraInventoryComponent.h"
#include "Game/AuraPlayerSaveGame.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "Net/UnrealNetwork.h"
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
	OnRoleChangedDelegate.Broadcast(CharacterRole);
	ForceNetUpdate();
	return true;
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
