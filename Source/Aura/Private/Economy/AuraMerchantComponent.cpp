// Copyright Druid Mechanics

#include "Economy/AuraMerchantComponent.h"

#include "Economy/AuraCommerceSubsystem.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UAuraMerchantComponent::UAuraMerchantComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAuraMerchantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority() && GetWorld())
	{
		if (UAuraCommerceSubsystem* Commerce = GetWorld()->GetSubsystem<UAuraCommerceSubsystem>())
		{
			Commerce->UnregisterMerchant(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UAuraMerchantComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAuraMerchantComponent, Presentation);
}

void UAuraMerchantComponent::InitializeAuthorityBinding(FName InPopulationMemberId, FName InMerchantDefinitionId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || InPopulationMemberId.IsNone() || InMerchantDefinitionId.IsNone()) return;
	Presentation.PopulationMemberId = InPopulationMemberId;
	Presentation.MerchantDefinitionId = InMerchantDefinitionId;
	Presentation.bAvailable = false;
	Presentation.Offers.Reset();
	Presentation.StockRevision = 0;
	if (GetWorld())
	{
		if (UAuraCommerceSubsystem* Commerce = GetWorld()->GetSubsystem<UAuraCommerceSubsystem>())
		{
			Commerce->RegisterMerchant(this);
		}
	}
}

void UAuraMerchantComponent::ApplyAuthorityPresentation(const FAuraMerchantPresentation& InPresentation)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	Presentation = InPresentation;
	OnPresentationChanged.Broadcast(Presentation);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

void UAuraMerchantComponent::SetAuthorityUnavailable()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	Presentation.bAvailable = false;
	OnPresentationChanged.Broadcast(Presentation);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

void UAuraMerchantComponent::OnRep_Presentation()
{
	UE_LOG(LogAura, Display, TEXT("[Commerce][Client] MerchantPresentation member=%s definition=%s available=%d offers=%d stockRevision=%u."),
		*Presentation.PopulationMemberId.ToString(), *Presentation.MerchantDefinitionId.ToString(), Presentation.bAvailable ? 1 : 0,
		Presentation.Offers.Num(), Presentation.StockRevision);
	OnPresentationChanged.Broadcast(Presentation);
}
