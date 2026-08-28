// Copyright Druid Mechanics

#include "Interaction/AuraInteractionComponent.h"

#include "Economy/AuraCommerceSubsystem.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraTargetableInterface.h"
#include "Interaction/AuraInteractionPolicy.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/PlayerController.h"
#include "Player/AuraPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UAuraInteractionComponent::UAuraInteractionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAuraInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UAuraInteractionComponent, CommerceSessionNonce, COND_OwnerOnly);
}

void UAuraInteractionComponent::RequestInteraction(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId)
{
	if (ClientRequestId <= 0) return;
	ServerRequestInteraction(TargetActor, InteractionOptionTag, ClientRequestId);
}

void UAuraInteractionComponent::RequestPurchase(AActor* MerchantActor, FName OfferId)
{
	if (!IsValid(MerchantActor) || OfferId.IsNone() || !CommerceSessionNonce.IsValid() || NextCommerceRequestId == 0 || NextCommerceRequestId == MAX_uint64) return;
	ServerRequestPurchase(CommerceSessionNonce, NextCommerceRequestId++, MerchantActor, OfferId);
}

#if !UE_BUILD_SHIPPING
void UAuraInteractionComponent::RequestPurchaseWithRequestIdForDevelopmentProbe(AActor* MerchantActor, FName OfferId, uint64 RequestId)
{
	if (!IsValid(MerchantActor) || OfferId.IsNone() || !CommerceSessionNonce.IsValid() || RequestId == 0 || RequestId == MAX_uint64) return;
	ServerRequestPurchase(CommerceSessionNonce, RequestId, MerchantActor, OfferId);
}

void UAuraInteractionComponent::RequestPurchaseWithSessionNonceForDevelopmentProbe(AActor* MerchantActor, FName OfferId,
	const FGuid& SessionNonce, uint64 RequestId)
{
	if (!IsValid(MerchantActor) || OfferId.IsNone() || !SessionNonce.IsValid() || RequestId == 0 || RequestId == MAX_uint64) return;
	ServerRequestPurchase(SessionNonce, RequestId, MerchantActor, OfferId);
}
#endif

void UAuraInteractionComponent::InitializeCommerceSessionNonce(const FGuid& InSessionNonce)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InSessionNonce.IsValid()) return;
	CommerceSessionNonce = InSessionNonce;
	NextCommerceRequestId = 1;
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

void UAuraInteractionComponent::ServerRequestInteraction_Implementation(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Requester = Controller ? Controller->GetPawn() : nullptr;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (ClientRequestId <= LastAcceptedRequestId)
	{
		ReturnResult(ClientRequestId, EAuraInteractionResultCode::StaleRequest); return;
	}
	if (LastAcceptedRequestTime >= 0.0 && Now - LastAcceptedRequestTime < 0.08)
	{
		ReturnResult(ClientRequestId, EAuraInteractionResultCode::RateLimited); return;
	}
	// Consume the monotonic ID before policy validation so a failed terminal
	// request cannot be replayed after the target or world state changes.
	LastAcceptedRequestId = ClientRequestId;
	LastAcceptedRequestTime = Now;
	if (!Controller || !Requester || !UAuraCombatStateComponent::FindForActor(Requester)
		|| !UAuraCombatStateComponent::FindForActor(Requester)->IsAlive())
	{
		ReturnResult(ClientRequestId, EAuraInteractionResultCode::InvalidRequester); return;
	}

	FAuraResolvedInteractionPolicy Policy;
	EAuraInteractionResultCode Failure;
	if (!FAuraInteractionPolicy::Resolve(Requester, TargetActor, InteractionOptionTag, Policy, Failure))
	{
		ReturnResult(ClientRequestId, Failure); return;
	}
	if (FVector::DistSquared(Requester->GetActorLocation(), TargetActor->GetActorLocation()) > FMath::Square(Policy.MaxRangeCm))
	{
		ReturnResult(ClientRequestId, EAuraInteractionResultCode::OutOfRange); return;
	}
	if (Policy.bRequiresLineOfSight)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AuraInteractionLOS), true, Requester);
		FHitResult Hit;
		const FVector Start = Requester->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		const FVector End = TargetActor->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() != TargetActor)
		{
			ReturnResult(ClientRequestId, EAuraInteractionResultCode::LineOfSightBlocked); return;
		}
	}
	const IAuraTargetableInterface* Targetable = Cast<IAuraTargetableInterface>(TargetActor);
	if (!Targetable || !Targetable->ExecuteAuraInteraction(Requester, Policy.OptionTag))
	{
		ReturnResult(ClientRequestId, EAuraInteractionResultCode::FeatureUnavailable); return;
	}
	UE_LOG(LogAura, Display, TEXT("[Interaction][Server] Handler=%s requester=%s target=%s."),
		*Policy.HandlerId.ToString(), *GetNameSafe(Requester), *GetNameSafe(TargetActor));
	ReturnResult(ClientRequestId, EAuraInteractionResultCode::Success);
}

void UAuraInteractionComponent::ReturnResult(int32 ClientRequestId, EAuraInteractionResultCode ResultCode)
{
	ClientInteractionResult(ClientRequestId, ResultCode);
}

void UAuraInteractionComponent::ClientInteractionResult_Implementation(int32 ClientRequestId, EAuraInteractionResultCode ResultCode)
{
	OnInteractionResult.Broadcast(ClientRequestId, ResultCode);
}

void UAuraInteractionComponent::ServerRequestPurchase_Implementation(FGuid SessionNonce, uint64 RequestId, AActor* MerchantActor, FName OfferId)
{
	FAuraPurchaseResult Result;
	if (UAuraCommerceSubsystem* Commerce = GetWorld() ? GetWorld()->GetSubsystem<UAuraCommerceSubsystem>() : nullptr)
	{
		Commerce->ProcessPurchase(Cast<AAuraPlayerController>(GetOwner()), SessionNonce, RequestId, MerchantActor, OfferId, Result);
	}
	else
	{
		Result.SessionNonce = SessionNonce;
		Result.RequestId = RequestId;
		Result.ResultCode = EAuraCommerceResultCode::InvalidSession;
	}
	UE_LOG(LogAura, Display, TEXT("[Commerce][Server] PurchaseResult player=%s request=%llu result=%d replay=%d walletRevision=%u inventoryRevision=%u stockRevision=%u."),
		*GetNameSafe(GetOwner()), Result.RequestId, static_cast<int32>(Result.ResultCode), Result.bReplay ? 1 : 0,
		Result.WalletRevision, Result.InventoryRevision, Result.StockRevision);
	ClientPurchaseResult(Result.SessionNonce, Result.RequestId, Result.ResultCode, Result.WalletRevision, Result.InventoryRevision, Result.StockRevision);
}

void UAuraInteractionComponent::ClientPurchaseResult_Implementation(FGuid SessionNonce, uint64 RequestId, EAuraCommerceResultCode ResultCode,
	uint32 WalletRevision, uint32 InventoryRevision, uint32 StockRevision)
{
	UE_LOG(LogAura, Display, TEXT("[Commerce][Client] PurchaseResult request=%llu result=%d walletRevision=%u inventoryRevision=%u stockRevision=%u."),
		RequestId, static_cast<int32>(ResultCode), WalletRevision, InventoryRevision, StockRevision);
	OnPurchaseResult.Broadcast(SessionNonce, RequestId, ResultCode, WalletRevision, InventoryRevision, StockRevision);
}
