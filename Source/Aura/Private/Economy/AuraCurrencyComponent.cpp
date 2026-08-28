// Copyright Druid Mechanics

#include "Economy/AuraCurrencyComponent.h"

#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Aura/AuraLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

namespace AuraCurrencyComponentPrivate
{
	const UAuraEconomyRegistrySubsystem* GetRegistry(const UObject* Object)
	{
		const UWorld* World = Object ? Object->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>() : nullptr;
	}
}

UAuraCurrencyComponent::UAuraCurrencyComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAuraCurrencyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UAuraCurrencyComponent, CurrencyId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraCurrencyComponent, Balance, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraCurrencyComponent, Revision, COND_OwnerOnly);
}

bool UAuraCurrencyComponent::IsValidCurrency(FName InCurrencyId) const
{
	const UAuraEconomyRegistrySubsystem* Registry = AuraCurrencyComponentPrivate::GetRegistry(this);
	return Registry && Registry->IsReady() && !InCurrencyId.IsNone()
		&& Registry->GetSnapshot()->CurrencyIds.Contains(InCurrencyId);
}

bool UAuraCurrencyComponent::CanUseAmount(int64 Amount) const
{
	return Amount > 0;
}

bool UAuraCurrencyComponent::CanCreditCurrency(FName InCurrencyId, int64 Amount) const
{
	if (!CanUseAmount(Amount) || !IsValidCurrency(InCurrencyId) || (!CurrencyId.IsNone() && CurrencyId != InCurrencyId)) return false;
	const UAuraEconomyRegistrySubsystem* Registry = AuraCurrencyComponentPrivate::GetRegistry(this);
	const int64 Maximum = Registry->GetSnapshot()->Settings.MaximumWalletBalance;
	return Balance <= Maximum && Amount <= Maximum - Balance;
}

bool UAuraCurrencyComponent::CanDebitCurrency(FName InCurrencyId, int64 Amount) const
{
	return CanUseAmount(Amount) && IsValidCurrency(InCurrencyId) && InCurrencyId == CurrencyId && Amount <= Balance;
}

EAuraEconomyMutationResult UAuraCurrencyComponent::CommitCreditCurrency(FName InCurrencyId, int64 Amount, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return EAuraEconomyMutationResult::NotAuthority;
	if (!CanUseAmount(Amount)) return EAuraEconomyMutationResult::InvalidAmount;
	if (!IsValidCurrency(InCurrencyId)) return EAuraEconomyMutationResult::InvalidDefinition;
	if (CurrencyId.IsNone()) CurrencyId = InCurrencyId;
	if (CurrencyId != InCurrencyId) return EAuraEconomyMutationResult::InvalidDefinition;
	if (!CanCreditCurrency(InCurrencyId, Amount)) return EAuraEconomyMutationResult::Overflow;
	Balance += Amount;
	++Revision;
	if (bBroadcastEvents) BroadcastChanged();
	return EAuraEconomyMutationResult::Success;
}

EAuraEconomyMutationResult UAuraCurrencyComponent::CommitDebitCurrency(FName InCurrencyId, int64 Amount, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return EAuraEconomyMutationResult::NotAuthority;
	if (!CanUseAmount(Amount)) return EAuraEconomyMutationResult::InvalidAmount;
	if (!IsValidCurrency(InCurrencyId) || CurrencyId != InCurrencyId) return EAuraEconomyMutationResult::InvalidDefinition;
	if (!CanDebitCurrency(InCurrencyId, Amount)) return EAuraEconomyMutationResult::InsufficientFunds;
	Balance -= Amount;
	++Revision;
	if (bBroadcastEvents) BroadcastChanged();
	return EAuraEconomyMutationResult::Success;
}

bool UAuraCurrencyComponent::InitializeCurrency(FName InCurrencyId, int64 InitialBalance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrencyId.IsNone() || Revision != 0 || !IsValidCurrency(InCurrencyId) || InitialBalance < 0) return false;
	const UAuraEconomyRegistrySubsystem* Registry = AuraCurrencyComponentPrivate::GetRegistry(this);
	if (InitialBalance > Registry->GetSnapshot()->Settings.MaximumWalletBalance) return false;
	CurrencyId = InCurrencyId;
	Balance = InitialBalance;
	++Revision;
	BroadcastChanged();
	return true;
}

bool UAuraCurrencyComponent::RestoreCurrencyState(FName InCurrencyId, int64 InBalance, uint32 InRevision, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidCurrency(InCurrencyId) || InBalance < 0) return false;
	const UAuraEconomyRegistrySubsystem* Registry = AuraCurrencyComponentPrivate::GetRegistry(this);
	if (InBalance > Registry->GetSnapshot()->Settings.MaximumWalletBalance) return false;
	CurrencyId = InCurrencyId;
	Balance = InBalance;
	Revision = InRevision;
	if (bBroadcastEvents) BroadcastChanged();
	return true;
}

void UAuraCurrencyComponent::BroadcastChanged()
{
	OnCurrencyChanged.Broadcast(Balance, Revision);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

void UAuraCurrencyComponent::OnRep_CurrencyId() {}
void UAuraCurrencyComponent::OnRep_Balance() {}
void UAuraCurrencyComponent::OnRep_Revision()
{
	const APlayerState* OwnerPlayerState = Cast<APlayerState>(GetOwner());
	const FString OwnerPlayerName = OwnerPlayerState ? OwnerPlayerState->GetPlayerName() : FString(TEXT("<unknown>"));
	UE_LOG(LogAura, Log, TEXT("[Economy][Client] Player=%s Currency replicated currency=%s balance=%lld revision=%u."),
		*OwnerPlayerName, *CurrencyId.ToString(), Balance, Revision);
	BroadcastChanged();
}
