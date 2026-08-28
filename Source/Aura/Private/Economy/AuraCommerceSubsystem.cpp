// Copyright Druid Mechanics

#include "Economy/AuraCommerceSubsystem.h"

#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Character/AuraCharacter.h"
#include "Character/AuraCharacterBase.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraInventoryComponent.h"
#include "Economy/AuraMerchantComponent.h"
#include "Game/AuraGameModeBase.h"
#include "Game/AuraPersistenceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Interaction/AuraInteractionPolicy.h"
#include "Interaction/AuraInteractionTypes.h"
#include "Misc/Guid.h"
#include "Net/UnrealNetwork.h"
#include "Player/AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "World/AuraPopulationManager.h"

namespace AuraCommerceSubsystemPrivate
{
	const UAuraEconomyRegistrySubsystem* GetRegistry(const UObject* Object)
	{
		const UWorld* World = Object ? Object->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>() : nullptr;
	}

	bool IsLineOfSightClear(const UWorld* World, const APawn* Requester, const AActor* Target)
	{
		if (!World || !Requester || !Target) return false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AuraCommerceLOS), true, Requester);
		// Other player pawns are not world geometry. Ignoring them keeps a
		// nearby teammate from making an otherwise valid merchant request fail
		// nondeterministically under replicated movement.
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (*It && *It != Requester && *It != Target) Params.AddIgnoredActor(*It);
		}
		FHitResult Hit;
		const FVector Start = Requester->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.GetActor() == Target;
	}
}

void UAuraCommerceSubsystem::Deinitialize()
{
	for (TPair<FName, FRuntimeMerchant>& Pair : Merchants)
	{
		if (UAuraMerchantComponent* Component = Pair.Value.Component.Get()) Component->SetAuthorityUnavailable();
	}
	Merchants.Reset();
	PendingMerchantRestores.Reset();
	Sessions.Reset();
	Super::Deinitialize();
}

void UAuraCommerceSubsystem::CapturePersistenceState(TArray<FAuraPersistedMerchantStock>& OutStocks) const
{
	OutStocks.Reset();
	for (const TPair<FName, FRuntimeMerchant>& Pair : Merchants)
	{
		const FRuntimeMerchant& Runtime = Pair.Value;
		FAuraPersistedMerchantStock& Saved = OutStocks.AddDefaulted_GetRef();
		Saved.PopulationMemberId = Runtime.PopulationMemberId;
		Saved.MerchantDefinitionId = Runtime.MerchantDefinitionId;
		Saved.StockRevision = Runtime.StockRevision;
		Saved.bAvailable = Runtime.bAvailable;
		for (const TPair<FName, FRuntimeOffer>& OfferPair : Runtime.Offers)
		{
			FAuraPersistedOfferStock& Offer = Saved.Offers.AddDefaulted_GetRef();
			Offer.OfferId = OfferPair.Key;
			Offer.CurrentStock = OfferPair.Value.CurrentStock;
		}
		Saved.Offers.Sort([](const FAuraPersistedOfferStock& A, const FAuraPersistedOfferStock& B) { return A.OfferId.LexicalLess(B.OfferId); });
	}
	for (const TPair<FName, FAuraPersistedMerchantStock>& Pair : PendingMerchantRestores)
	{
		if (!Merchants.Contains(Pair.Key)) OutStocks.Add(Pair.Value);
	}
	OutStocks.Sort([](const FAuraPersistedMerchantStock& A, const FAuraPersistedMerchantStock& B) { return A.PopulationMemberId.LexicalLess(B.PopulationMemberId); });
}

bool UAuraCommerceSubsystem::RestorePersistenceState(const TArray<FAuraPersistedMerchantStock>& Stocks, FString& OutError)
{
	OutError.Reset();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) { OutError = TEXT("Merchant restore requires authority world."); return false; }
	PendingMerchantRestores.Reset();
	const UAuraEconomyRegistrySubsystem* Registry = AuraCommerceSubsystemPrivate::GetRegistry(this);
	const TSharedPtr<const FAuraEconomySnapshot> Snapshot = Registry ? Registry->GetSnapshot() : nullptr;
	AAuraGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AAuraGameModeBase>();
	UAuraPopulationManager* PopulationManager = GameMode ? GameMode->GetPopulationManagerMutable() : nullptr;
	struct FValidatedMerchantRestore
	{
		FName PopulationMemberId = NAME_None;
		uint32 StockRevision = 0;
		bool bAvailable = false;
		TMap<FName, int64> OfferStocks;
	};
	TArray<FValidatedMerchantRestore> Validated;
	Validated.Reserve(Stocks.Num());
	FName PreviousMember = NAME_None;
	for (const FAuraPersistedMerchantStock& Saved : Stocks)
	{
		if (Saved.PopulationMemberId.IsNone() || (!PreviousMember.IsNone() && !PreviousMember.LexicalLess(Saved.PopulationMemberId)))
		{
			OutError = TEXT("Merchant persistence entries are not sorted and unique."); return false;
		}
		PreviousMember = Saved.PopulationMemberId;
		FRuntimeMerchant* Runtime = Merchants.Find(Saved.PopulationMemberId);
		const FAuraMerchantDefinition* Definition = Snapshot.IsValid() ? Snapshot->Merchants.Find(Saved.MerchantDefinitionId) : nullptr;
		if (!Definition)
		{
			OutError = FString::Printf(TEXT("Saved merchant '%s' references an unknown merchant definition."), *Saved.PopulationMemberId.ToString()); return false;
		}
		if (Runtime && Runtime->MerchantDefinitionId != Saved.MerchantDefinitionId)
		{
			OutError = FString::Printf(TEXT("Saved merchant '%s' is not present in the current authoritative registry."), *Saved.PopulationMemberId.ToString()); return false;
		}
		const bool bDeferUntilRespawn = !Runtime && PopulationManager
			&& PopulationManager->IsDormantMerchantMember(Saved.PopulationMemberId, Saved.MerchantDefinitionId);
		if (!Runtime && !bDeferUntilRespawn)
		{
			OutError = FString::Printf(TEXT("Saved merchant '%s' is not present in the current authoritative registry."), *Saved.PopulationMemberId.ToString()); return false;
		}
		FValidatedMerchantRestore Candidate;
		Candidate.PopulationMemberId = Saved.PopulationMemberId;
		Candidate.StockRevision = Saved.StockRevision;
		Candidate.bAvailable = Saved.bAvailable;
		TSet<FName> SeenOffers;
		FName PreviousOffer = NAME_None;
		for (const FAuraPersistedOfferStock& SavedOffer : Saved.Offers)
		{
			if (SavedOffer.OfferId.IsNone() || SeenOffers.Contains(SavedOffer.OfferId)
				|| (!PreviousOffer.IsNone() && !PreviousOffer.LexicalLess(SavedOffer.OfferId)) || SavedOffer.CurrentStock < -1)
			{
				OutError = TEXT("Saved merchant offer stock is invalid or not sorted."); return false;
			}
			PreviousOffer = SavedOffer.OfferId;
			SeenOffers.Add(SavedOffer.OfferId);
			const FRuntimeOffer* RuntimeOffer = Runtime ? Runtime->Offers.Find(SavedOffer.OfferId) : nullptr;
			const FAuraOfferDefinition* CurrentDefinition = Runtime
				? (RuntimeOffer ? &RuntimeOffer->Definition : nullptr)
				: (Snapshot.IsValid() ? Snapshot->Offers.Find(SavedOffer.OfferId) : nullptr);
			if (!CurrentDefinition || !Definition->OfferIds.Contains(SavedOffer.OfferId)
				|| (CurrentDefinition->StockPolicy == EAuraStockPolicy::Finite && SavedOffer.CurrentStock < 0))
			{
				OutError = TEXT("Saved merchant offer does not match the current immutable definition."); return false;
			}
			Candidate.OfferStocks.Add(SavedOffer.OfferId, SavedOffer.CurrentStock);
		}
		const int32 ExpectedOfferCount = Runtime ? Runtime->Offers.Num() : Definition->OfferIds.Num();
		if (SeenOffers.Num() != ExpectedOfferCount)
		{
			OutError = FString::Printf(TEXT("Saved merchant '%s' does not contain the complete current offer set."),
				*Saved.PopulationMemberId.ToString());
			return false;
		}
		if (bDeferUntilRespawn) PendingMerchantRestores.Add(Saved.PopulationMemberId, Saved);
		else Validated.Add(MoveTemp(Candidate));
	}

	for (const FValidatedMerchantRestore& Candidate : Validated)
	{
		FRuntimeMerchant* Runtime = Merchants.Find(Candidate.PopulationMemberId);
		check(Runtime);
		for (const TPair<FName, int64>& OfferStock : Candidate.OfferStocks)
		{
			Runtime->Offers.FindChecked(OfferStock.Key).CurrentStock = OfferStock.Value;
		}
		Runtime->StockRevision = FMath::Max<uint32>(1, Candidate.StockRevision);
		Runtime->bAvailable = Candidate.bAvailable;
	}
	for (const FValidatedMerchantRestore& Candidate : Validated)
	{
		if (FRuntimeMerchant* Runtime = Merchants.Find(Candidate.PopulationMemberId))
		{
			PublishMerchantPresentation(*Runtime);
		}
	}
	return true;
}

FGuid UAuraCommerceSubsystem::RotateSession(AAuraPlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->HasAuthority()) return FGuid();
	FSession& Session = Sessions.FindOrAdd(PlayerController);
	Session.Nonce = FGuid::NewGuid();
	while (!Session.Nonce.IsValid()) Session.Nonce = FGuid::NewGuid();
	Session.LastAcceptedRequestId = 0;
	Session.ReplayCache.Reset();
	Session.ReplayOrder.Reset();
	return Session.Nonce;
}

void UAuraCommerceSubsystem::InvalidateSession(AAuraPlayerController* PlayerController)
{
	if (PlayerController) Sessions.Remove(PlayerController);
}

bool UAuraCommerceSubsystem::RegisterMerchant(UAuraMerchantComponent* MerchantComponent)
{
	if (!MerchantComponent || !MerchantComponent->GetOwner() || !MerchantComponent->GetOwner()->HasAuthority()) return false;
	const FName MemberId = MerchantComponent->GetPopulationMemberId();
	const FName MerchantDefinitionId = MerchantComponent->GetMerchantDefinitionId();
	const UAuraEconomyRegistrySubsystem* Registry = AuraCommerceSubsystemPrivate::GetRegistry(this);
	const TSharedPtr<const FAuraEconomySnapshot> Snapshot = Registry ? Registry->GetSnapshot() : nullptr;
	const FAuraMerchantDefinition* Definition = Snapshot.IsValid() ? Snapshot->Merchants.Find(MerchantDefinitionId) : nullptr;
	if (!Registry || !Registry->IsReady() || MemberId.IsNone() || MerchantDefinitionId.IsNone() || !Definition) return false;

	FRuntimeMerchant& Runtime = Merchants.FindOrAdd(MemberId);
	if (!Runtime.MerchantDefinitionId.IsNone() && Runtime.MerchantDefinitionId != MerchantDefinitionId)
	{
		return false;
	}
	Runtime.PopulationMemberId = MemberId;
	Runtime.MerchantDefinitionId = MerchantDefinitionId;
	Runtime.Component = MerchantComponent;
	Runtime.bAvailable = true;
	if (Runtime.Offers.IsEmpty())
	{
		for (const FName OfferId : Definition->OfferIds)
		{
			const FAuraOfferDefinition* Offer = Snapshot->Offers.Find(OfferId);
			if (!Offer || Offer->OfferId.IsNone() || Offer->ItemId.IsNone() || Offer->GrantQuantity <= 0 || Offer->BuyPrice <= 0
				|| !Snapshot->Items.Contains(Offer->ItemId))
			{
				Runtime.bAvailable = false;
				Runtime.Offers.Reset();
				return false;
			}
			FRuntimeOffer& RuntimeOffer = Runtime.Offers.Add(OfferId);
			RuntimeOffer.Definition = *Offer;
			RuntimeOffer.CurrentStock = Offer->StockPolicy == EAuraStockPolicy::Unlimited ? -1 : Offer->InitialStock;
		}
	}
	if (const FAuraPersistedMerchantStock* Pending = PendingMerchantRestores.Find(MemberId))
	{
		for (const FAuraPersistedOfferStock& SavedOffer : Pending->Offers)
		{
			if (FRuntimeOffer* RuntimeOffer = Runtime.Offers.Find(SavedOffer.OfferId)) RuntimeOffer->CurrentStock = SavedOffer.CurrentStock;
		}
		Runtime.StockRevision = FMath::Max<uint32>(1, Pending->StockRevision);
		// A new actor means the dormant population slot has respawned; its
		// merchant becomes available again while retaining persisted stock.
		Runtime.bAvailable = true;
		PendingMerchantRestores.Remove(MemberId);
	}
	PublishMerchantPresentation(Runtime);
	UE_LOG(LogAura, Display, TEXT("[Commerce][Merchant] Registered member=%s definition=%s offers=%d stockRevision=%u."),
		*MemberId.ToString(), *MerchantDefinitionId.ToString(), Runtime.Offers.Num(), Runtime.StockRevision);
	return Runtime.bAvailable;
}

void UAuraCommerceSubsystem::MarkMerchantUnavailable(UAuraMerchantComponent* MerchantComponent)
{
	if (!MerchantComponent || !MerchantComponent->GetOwner() || !MerchantComponent->GetOwner()->HasAuthority()) return;
	const FName MemberId = MerchantComponent->GetPopulationMemberId();
	if (FRuntimeMerchant* Runtime = Merchants.Find(MemberId); Runtime && Runtime->Component.Get() == MerchantComponent)
	{
		Runtime->bAvailable = false;
		++Runtime->StockRevision;
		PublishMerchantPresentation(*Runtime);
		return;
	}
	MerchantComponent->SetAuthorityUnavailable();
}

void UAuraCommerceSubsystem::UnregisterMerchant(UAuraMerchantComponent* MerchantComponent)
{
	if (!MerchantComponent) return;
	const FName MemberId = MerchantComponent->GetPopulationMemberId();
	FRuntimeMerchant* Runtime = Merchants.Find(MemberId);
	if (!Runtime || Runtime->Component.Get() != MerchantComponent) return;
	Runtime->Component.Reset();
	Runtime->bAvailable = false;
	MerchantComponent->SetAuthorityUnavailable();
}

void UAuraCommerceSubsystem::PublishMerchantPresentation(FRuntimeMerchant& Merchant)
{
	UAuraMerchantComponent* Component = Merchant.Component.Get();
	if (!Component) return;
	FAuraMerchantPresentation Presentation;
	Presentation.PopulationMemberId = Merchant.PopulationMemberId;
	Presentation.MerchantDefinitionId = Merchant.MerchantDefinitionId;
	Presentation.bAvailable = Merchant.bAvailable;
	Presentation.StockRevision = Merchant.StockRevision;
	const UAuraEconomyRegistrySubsystem* Registry = AuraCommerceSubsystemPrivate::GetRegistry(this);
	const TSharedPtr<const FAuraEconomySnapshot> Snapshot = Registry ? Registry->GetSnapshot() : nullptr;
	if (!Snapshot.IsValid()) return;
	const FAuraMerchantDefinition* Definition = Snapshot->Merchants.Find(Merchant.MerchantDefinitionId);
	if (!Definition) return;
	for (const FName OfferId : Definition->OfferIds)
	{
		const FRuntimeOffer* RuntimeOffer = Merchant.Offers.Find(OfferId);
		if (!RuntimeOffer) continue;
		FAuraMerchantOfferPresentation& Offer = Presentation.Offers.AddDefaulted_GetRef();
		Offer.OfferId = RuntimeOffer->Definition.OfferId;
		Offer.ItemId = RuntimeOffer->Definition.ItemId;
		Offer.GrantQuantity = RuntimeOffer->Definition.GrantQuantity;
		Offer.BuyPrice = RuntimeOffer->Definition.BuyPrice;
		Offer.StockPolicy = RuntimeOffer->Definition.StockPolicy;
		Offer.CurrentStock = RuntimeOffer->CurrentStock;
		Offer.bAvailable = Merchant.bAvailable && (Offer.StockPolicy == EAuraStockPolicy::Unlimited || Offer.CurrentStock > 0);
		Offer.StockRevision = Merchant.StockRevision;
		if (const FAuraItemDefinition* Item = Snapshot->Items.Find(Offer.ItemId)) Offer.ItemDisplayName = Item->DisplayName;
	}
	Component->ApplyAuthorityPresentation(Presentation);
}

void UAuraCommerceSubsystem::FillRevisions(AAuraPlayerController* PlayerController, const FRuntimeMerchant* Merchant,
	FName OfferId, FAuraPurchaseResult& Result) const
{
	const AAuraPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAuraPlayerState>() : nullptr;
	Result.WalletRevision = PlayerState && PlayerState->GetCurrencyComponent() ? PlayerState->GetCurrencyComponent()->GetRevision() : 0;
	Result.InventoryRevision = PlayerState && PlayerState->GetInventoryComponent() ? PlayerState->GetInventoryComponent()->GetRevision() : 0;
	if (Merchant)
	{
		Result.StockRevision = Merchant->StockRevision;
	}
}

void UAuraCommerceSubsystem::CacheResult(FSession& Session, const FAuraPurchaseResult& Result)
{
	if (Result.RequestId == 0 || Session.ReplayCache.Contains(Result.RequestId)) return;
	Session.ReplayCache.Add(Result.RequestId, Result);
	Session.ReplayOrder.Add(Result.RequestId);
	while (Session.ReplayOrder.Num() > 256)
	{
		const uint64 Oldest = Session.ReplayOrder[0];
		Session.ReplayOrder.RemoveAt(0);
		Session.ReplayCache.Remove(Oldest);
	}
}

EAuraCommerceResultCode UAuraCommerceSubsystem::MapInteractionFailure(EAuraInteractionResultCode Failure)
{
	switch (Failure)
	{
	case EAuraInteractionResultCode::InvalidRequester:
	case EAuraInteractionResultCode::InvalidTarget: return EAuraCommerceResultCode::InvalidRequester;
	case EAuraInteractionResultCode::WrongLifeState:
	case EAuraInteractionResultCode::FeatureUnavailable:
	case EAuraInteractionResultCode::OptionUnavailable: return EAuraCommerceResultCode::MerchantUnavailable;
	case EAuraInteractionResultCode::OutOfRange: return EAuraCommerceResultCode::OutOfRange;
	case EAuraInteractionResultCode::LineOfSightBlocked: return EAuraCommerceResultCode::LineOfSightBlocked;
	case EAuraInteractionResultCode::PhaseDenied:
	case EAuraInteractionResultCode::ZoneDenied: return EAuraCommerceResultCode::EligibilityDenied;
	default: return EAuraCommerceResultCode::MerchantUnavailable;
	}
}

FAuraPurchaseResult UAuraCommerceSubsystem::ExecutePurchase(AAuraPlayerController* PlayerController, FRuntimeMerchant& Merchant,
	AActor* MerchantActor, FName OfferId)
{
	FAuraPurchaseResult Result;
	Result.ResultCode = EAuraCommerceResultCode::InvalidDefinition;
	AAuraPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AAuraPlayerState>() : nullptr;
	APawn* Requester = PlayerController ? PlayerController->GetPawn() : nullptr;
	FRuntimeOffer* RuntimeOffer = Merchant.Offers.Find(OfferId);
	FillRevisions(PlayerController, &Merchant, OfferId, Result);
	if (!PlayerState || !Requester || !Merchant.bAvailable || !MerchantActor || !RuntimeOffer)
	{
		Result.ResultCode = !Merchant.bAvailable ? EAuraCommerceResultCode::MerchantUnavailable : EAuraCommerceResultCode::InvalidRequester;
		return Result;
	}
	const FAuraOfferDefinition& Offer = RuntimeOffer->Definition;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FAuraResolvedInteractionPolicy Policy;
	EAuraInteractionResultCode InteractionFailure = EAuraInteractionResultCode::FeatureUnavailable;
	if (!FAuraInteractionPolicy::Resolve(Requester, MerchantActor, Tags.Interaction_Trade, Policy, InteractionFailure))
	{
		UE_LOG(LogAura, Warning, TEXT("[Commerce][Server] ValidationRejected stage=Interaction failure=%d requester=%s merchant=%s phasePolicy=%s."),
			static_cast<int32>(InteractionFailure), *GetNameSafe(Requester), *GetNameSafe(MerchantActor), *Policy.HandlerId.ToString());
		Result.ResultCode = MapInteractionFailure(InteractionFailure);
		return Result;
	}
	if (FVector::DistSquared(Requester->GetActorLocation(), MerchantActor->GetActorLocation()) > FMath::Square(Policy.MaxRangeCm))
	{
		UE_LOG(LogAura, Warning, TEXT("[Commerce][Server] ValidationRejected stage=Range requester=%s requesterLocation=%s merchant=%s merchantLocation=%s distanceCm=%.1f maxRangeCm=%.1f."),
			*GetNameSafe(Requester), *Requester->GetActorLocation().ToCompactString(), *GetNameSafe(MerchantActor),
			*MerchantActor->GetActorLocation().ToCompactString(), FVector::Dist(Requester->GetActorLocation(), MerchantActor->GetActorLocation()), Policy.MaxRangeCm);
		Result.ResultCode = EAuraCommerceResultCode::OutOfRange;
		return Result;
	}
	if (!AuraCommerceSubsystemPrivate::IsLineOfSightClear(GetWorld(), Requester, MerchantActor))
	{
		UE_LOG(LogAura, Warning, TEXT("[Commerce][Server] ValidationRejected stage=LineOfSight requester=%s merchant=%s."),
			*GetNameSafe(Requester), *GetNameSafe(MerchantActor));
		Result.ResultCode = EAuraCommerceResultCode::LineOfSightBlocked;
		return Result;
	}
	const AAuraCharacterBase* Character = Cast<AAuraCharacterBase>(Requester);
	if (!Offer.RequiredRoleId.IsNone() && (!Character || Character->GetAppliedRoleState().RoleId != Offer.RequiredRoleId))
	{
		Result.ResultCode = EAuraCommerceResultCode::EligibilityDenied;
		return Result;
	}
	if (Offer.RequiredInteractionTag.IsValid() && !Offer.RequiredInteractionTag.MatchesTagExact(Tags.Interaction_Trade))
	{
		Result.ResultCode = EAuraCommerceResultCode::EligibilityDenied;
		return Result;
	}
	if (Offer.GrantQuantity <= 0 || Offer.BuyPrice <= 0)
	{
		Result.ResultCode = EAuraCommerceResultCode::InvalidDefinition;
		return Result;
	}
	if (Offer.StockPolicy == EAuraStockPolicy::Finite && RuntimeOffer->CurrentStock < 1)
	{
		Result.ResultCode = EAuraCommerceResultCode::SoldOut;
		return Result;
	}
	UAuraCurrencyComponent* Currency = PlayerState->GetCurrencyComponent();
	UAuraInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	if (!Currency || !Inventory)
	{
		Result.ResultCode = EAuraCommerceResultCode::InvalidRequester;
		return Result;
	}
	if (!Currency->CanDebitCurrency(Currency->GetCurrencyId(), Offer.BuyPrice))
	{
		Result.ResultCode = EAuraCommerceResultCode::InsufficientFunds;
		return Result;
	}
	if (!Inventory->CanAddItem(Offer.ItemId, Offer.GrantQuantity))
	{
		Result.ResultCode = EAuraCommerceResultCode::FullInventory;
		return Result;
	}

	const FName CurrencyIdBefore = Currency->GetCurrencyId();
	const int64 BalanceBefore = Currency->GetBalance();
	const uint32 WalletRevisionBefore = Currency->GetRevision();
	const TArray<FAuraInventorySlot> InventoryBefore = Inventory->GetSlots();
	const uint32 InventoryRevisionBefore = Inventory->GetRevision();
	const int64 StockBefore = RuntimeOffer->CurrentStock;
	const uint32 StockRevisionBefore = Merchant.StockRevision;
	const EAuraEconomyMutationResult DebitResult = Currency->CommitDebitCurrency(CurrencyIdBefore, Offer.BuyPrice, false);
	const EAuraEconomyMutationResult AddResult = DebitResult == EAuraEconomyMutationResult::Success
		? Inventory->CommitAddItem(Offer.ItemId, Offer.GrantQuantity, false) : EAuraEconomyMutationResult::InvalidState;
	if (DebitResult != EAuraEconomyMutationResult::Success || AddResult != EAuraEconomyMutationResult::Success)
	{
		const bool bRestored = Currency->RestoreCurrencyState(CurrencyIdBefore, BalanceBefore, WalletRevisionBefore, false)
			&& Inventory->RestoreInventoryState(InventoryBefore, InventoryRevisionBefore, false);
		Result.ResultCode = bRestored
			? (DebitResult != EAuraEconomyMutationResult::Success ? EAuraCommerceResultCode::InsufficientFunds : EAuraCommerceResultCode::FullInventory)
			: EAuraCommerceResultCode::RollbackFailed;
		FillRevisions(PlayerController, &Merchant, OfferId, Result);
		return Result;
	}
	if (Offer.StockPolicy == EAuraStockPolicy::Finite)
	{
		if (RuntimeOffer->CurrentStock <= 0)
		{
			const bool bRestored = Currency->RestoreCurrencyState(CurrencyIdBefore, BalanceBefore, WalletRevisionBefore, false)
				&& Inventory->RestoreInventoryState(InventoryBefore, InventoryRevisionBefore, false);
			RuntimeOffer->CurrentStock = StockBefore;
			Merchant.StockRevision = StockRevisionBefore;
			Result.ResultCode = bRestored ? EAuraCommerceResultCode::SoldOut : EAuraCommerceResultCode::RollbackFailed;
			FillRevisions(PlayerController, &Merchant, OfferId, Result);
			return Result;
		}
		--RuntimeOffer->CurrentStock;
	}
	++Merchant.StockRevision;
	PublishMerchantPresentation(Merchant);
	Currency->PublishChanged();
	Inventory->PublishChanged();
	Result.ResultCode = EAuraCommerceResultCode::Success;
	FillRevisions(PlayerController, &Merchant, OfferId, Result);
	UE_LOG(LogAura, Display, TEXT("[Commerce][Server] Purchase success member=%s offer=%s price=%lld grant=%lld stock=%lld request transaction committed."),
		*Merchant.PopulationMemberId.ToString(), *OfferId.ToString(), Offer.BuyPrice, Offer.GrantQuantity, RuntimeOffer->CurrentStock);
	return Result;
}

bool UAuraCommerceSubsystem::ProcessPurchase(AAuraPlayerController* PlayerController, const FGuid& SessionNonce, uint64 RequestId,
	AActor* MerchantActor, FName OfferId, FAuraPurchaseResult& OutResult)
{
	OutResult = FAuraPurchaseResult();
	OutResult.SessionNonce = SessionNonce;
	OutResult.RequestId = RequestId;
	FSession* Session = PlayerController ? Sessions.Find(PlayerController) : nullptr;
	if (!PlayerController || !PlayerController->HasAuthority() || !Session || !Session->Nonce.IsValid() || Session->Nonce != SessionNonce)
	{
		OutResult.ResultCode = EAuraCommerceResultCode::InvalidSession;
		return false;
	}
	if (RequestId == 0 || RequestId == MAX_uint64)
	{
		OutResult.ResultCode = EAuraCommerceResultCode::InvalidRequest;
		return false;
	}
	if (const FAuraPurchaseResult* Cached = Session->ReplayCache.Find(RequestId))
	{
		OutResult = *Cached;
		OutResult.bReplay = true;
		return true;
	}
	if (RequestId <= Session->LastAcceptedRequestId)
	{
		OutResult.ResultCode = EAuraCommerceResultCode::StaleRequest;
		return false;
	}
	if (RequestId != Session->LastAcceptedRequestId + 1)
	{
		OutResult.ResultCode = EAuraCommerceResultCode::RequestGap;
		return false;
	}
	Session->LastAcceptedRequestId = RequestId;
	AAuraPlayerState* PlayerState = PlayerController->GetPlayerState<AAuraPlayerState>();
	FRuntimeMerchant* Merchant = nullptr;
	if (UAuraMerchantComponent* Component = MerchantActor ? MerchantActor->FindComponentByClass<UAuraMerchantComponent>() : nullptr)
	{
		Merchant = Merchants.Find(Component->GetPopulationMemberId());
		if (!Merchant || Merchant->Component.Get() != Component) Merchant = nullptr;
	}
	UAuraCurrencyComponent* Currency = PlayerState ? PlayerState->GetCurrencyComponent() : nullptr;
	UAuraInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	FRuntimeOffer* Offer = Merchant ? Merchant->Offers.Find(OfferId) : nullptr;
	const bool bHasRollbackState = Currency && Inventory && Merchant && Offer;
	const FName CurrencyIdBefore = bHasRollbackState ? Currency->GetCurrencyId() : NAME_None;
	const int64 BalanceBefore = bHasRollbackState ? Currency->GetBalance() : 0;
	const uint32 WalletRevisionBefore = bHasRollbackState ? Currency->GetRevision() : 0;
	const TArray<FAuraInventorySlot> InventoryBefore = bHasRollbackState ? Inventory->GetSlots() : TArray<FAuraInventorySlot>();
	const uint32 InventoryRevisionBefore = bHasRollbackState ? Inventory->GetRevision() : 0;
	const int64 StockBefore = bHasRollbackState ? Offer->CurrentStock : 0;
	const uint32 StockRevisionBefore = bHasRollbackState ? Merchant->StockRevision : 0;
	FRuntimeMerchant EmptyMerchant;
	OutResult = ExecutePurchase(PlayerController, Merchant ? *Merchant : EmptyMerchant, MerchantActor, OfferId);
	OutResult.SessionNonce = SessionNonce;
	OutResult.RequestId = RequestId;
	if (OutResult.ResultCode == EAuraCommerceResultCode::Success)
	{
		AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr;
		UAuraPersistenceSubsystem* Persistence = GameMode ? GameMode->GetPersistenceSubsystemMutable() : nullptr;
		const bool bPersistenceRequired = Persistence
			? Persistence->IsPersistentLoginRequired()
			: (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone);
		if (!bPersistenceRequired)
		{
			CacheResult(*Session, OutResult);
			return true;
		}
		if (Persistence && PlayerState && GameMode && GameMode->GetPopulationManagerMutable() && bHasRollbackState)
		{
			FString PersistenceError;
			TArray<FAuraPersistedMerchantStock> MerchantStocks;
			CapturePersistenceState(MerchantStocks);
			const bool bCheckpoint = Persistence->SavePlayerAndWorldCheckpoint(PlayerState,
				Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()),
				Cast<UAuraAttributeSet>(PlayerState->GetAttributeSet()),
				GameMode->GetPopulationManagerMutable()->BuildDebugSnapshot(), MerchantStocks, PersistenceError);
			UE_LOG(LogAura, Display, TEXT("[Persistence][Purchase] Checkpoint=%d player=%d world=%d atomic=1 request=%llu error=%s."),
				bCheckpoint ? 1 : 0, bCheckpoint ? 1 : 0, bCheckpoint ? 1 : 0, RequestId, *PersistenceError);
			if (bCheckpoint)
			{
				CacheResult(*Session, OutResult);
				return true;
			}
		}
		else
		{
			UE_LOG(LogAura, Error, TEXT("[Persistence][Purchase] Checkpoint unavailable for persistent profile request=%llu."), RequestId);
		}

		const bool bWalletRestored = bHasRollbackState
			&& Currency->RestoreCurrencyState(CurrencyIdBefore, BalanceBefore, WalletRevisionBefore, false);
		const bool bInventoryRestored = bHasRollbackState
			&& Inventory->RestoreInventoryState(InventoryBefore, InventoryRevisionBefore, false);
		if (bHasRollbackState)
		{
			Offer->CurrentStock = StockBefore;
			Merchant->StockRevision = StockRevisionBefore;
			PublishMerchantPresentation(*Merchant);
		}
		if (bWalletRestored) Currency->PublishChanged();
		if (bInventoryRestored) Inventory->PublishChanged();
		OutResult.ResultCode = bWalletRestored && bInventoryRestored && bHasRollbackState
			? EAuraCommerceResultCode::PersistenceFailed
			: EAuraCommerceResultCode::RollbackFailed;
		FillRevisions(PlayerController, Merchant, OfferId, OutResult);
	}
	CacheResult(*Session, OutResult);
	return true;
}

bool UAuraCommerceSubsystem::GetOfferStock(FName PopulationMemberId, FName OfferId, int64& OutStock, uint32& OutRevision) const
{
	const FRuntimeMerchant* Merchant = Merchants.Find(PopulationMemberId);
	const FRuntimeOffer* Offer = Merchant ? Merchant->Offers.Find(OfferId) : nullptr;
	if (!Merchant || !Offer) return false;
	OutStock = Offer->CurrentStock;
	OutRevision = Merchant->StockRevision;
	return true;
}

#if !UE_BUILD_SHIPPING
bool UAuraCommerceSubsystem::SetOfferStockForDevelopmentProbe(FName PopulationMemberId, FName OfferId, int64 NewStock)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || NewStock < 0) return false;
	FRuntimeMerchant* Merchant = Merchants.Find(PopulationMemberId);
	FRuntimeOffer* Offer = Merchant ? Merchant->Offers.Find(OfferId) : nullptr;
	if (!Merchant || !Offer || Offer->Definition.StockPolicy != EAuraStockPolicy::Finite || !Merchant->bAvailable) return false;
	Offer->CurrentStock = NewStock;
	++Merchant->StockRevision;
	PublishMerchantPresentation(*Merchant);
	return true;
}

bool UAuraCommerceSubsystem::SetMerchantAvailabilityForDevelopmentProbe(FName PopulationMemberId, bool bInAvailable)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return false;
	FRuntimeMerchant* Merchant = Merchants.Find(PopulationMemberId);
	if (!Merchant) return false;
	Merchant->bAvailable = bInAvailable;
	++Merchant->StockRevision;
	PublishMerchantPresentation(*Merchant);
	return true;
}
#endif
