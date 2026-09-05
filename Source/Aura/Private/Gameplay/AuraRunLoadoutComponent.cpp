// Copyright Druid Mechanics

#include "Gameplay/AuraRunLoadoutComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UAuraRunLoadoutComponent::UAuraRunLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAuraRunLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UAuraRunLoadoutComponent, ActiveRunId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraRunLoadoutComponent, ActiveRunEpoch, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraRunLoadoutComponent, SelectedAugments, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraRunLoadoutComponent, EmergencyRefillCharges, COND_OwnerOnly);
}

bool UAuraRunLoadoutComponent::BeginRun(const FGuid& RunId, int32 Epoch)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !RunId.IsValid() || Epoch <= 0 || HasActiveRun()) return false;
	ActiveRunId = RunId;
	ActiveRunEpoch = Epoch;
	SelectedAugments.Reset();
	EmergencyRefillCharges = 1;
	GetOwner()->ForceNetUpdate();
	return true;
}
bool UAuraRunLoadoutComponent::AddAugment(FName AugmentId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !HasActiveRun() || AugmentId.IsNone()
		|| SelectedAugments.Num() >= 2 || SelectedAugments.Contains(AugmentId)) return false;
	SelectedAugments.Add(AugmentId);
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UAuraRunLoadoutComponent::ConsumeEmergencyRefill()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !HasActiveRun() || EmergencyRefillCharges <= 0) return false;
	--EmergencyRefillCharges;
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UAuraRunLoadoutComponent::EndRun()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !HasActiveRun()) return false;
	ActiveRunId.Invalidate();
	ActiveRunEpoch = 0;
	SelectedAugments.Reset();
	EmergencyRefillCharges = 0;
	GetOwner()->ForceNetUpdate();
	return true;
}
