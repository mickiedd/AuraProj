// Copyright Druid Mechanics


#include "AbilitySystem/AuraAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "Aura/AuraLogChannels.h"
#include "Game/LoadScreenSaveGame.h"
#include "Interaction/PlayerInterface.h"

namespace
{
	TArray<FGameplayTag> GetOrderedSlotsForAbilityType(const FGameplayTag& AbilityType)
	{
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

		if (AbilityType.MatchesTagExact(GameplayTags.Abilities_Type_Passive))
		{
			return {
				GameplayTags.InputTag_Passive_1,
				GameplayTags.InputTag_Passive_2
			};
		}

		return {
			GameplayTags.InputTag_LMB,
			GameplayTags.InputTag_RMB,
			GameplayTags.InputTag_1,
			GameplayTags.InputTag_2,
			GameplayTags.InputTag_3,
			GameplayTags.InputTag_4
		};
	}
}

void UAuraAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UAuraAbilitySystemComponent::ClientEffectApplied);
	
}

void UAuraAbilitySystemComponent::AddCharacterAbilitiesFromSaveData(ULoadScreenSaveGame* SaveData)
{
	for (const FSavedAbility& Data : SaveData->SavedAbilities)
	{
		const TSubclassOf<UGameplayAbility> LoadedAbilityClass = Data.GameplayAbility;

		FGameplayAbilitySpec LoadedAbilitySpec = FGameplayAbilitySpec(LoadedAbilityClass, Data.AbilityLevel);

		LoadedAbilitySpec.DynamicAbilityTags.AddTag(Data.AbilitySlot);
		LoadedAbilitySpec.DynamicAbilityTags.AddTag(Data.AbilityStatus);
		if (Data.AbilityType == FAuraGameplayTags::Get().Abilities_Type_Offensive)
		{
			GiveAbility(LoadedAbilitySpec);
		}
		else if (Data.AbilityType == FAuraGameplayTags::Get().Abilities_Type_Passive)
		{
			if (Data.AbilityStatus.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Equipped))
			{
				GiveAbilityAndActivateOnce(LoadedAbilitySpec);
				MulticastActivatePassiveEffect(Data.AbilityTag, true);
			}
			else
			{
				GiveAbility(LoadedAbilitySpec);
			}
		}
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.DynamicAbilityTags.AddTag(AuraAbility->StartupInputTag);
			AbilitySpec.DynamicAbilityTags.AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
			GiveAbility(AbilitySpec);
		}
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		AbilitySpec.DynamicAbilityTags.AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle, AbilitySpec.ActivationInfo.GetActivationPredictionKey());
			}
		}
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag) && AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, AbilitySpec.ActivationInfo.GetActivationPredictionKey());
		}
	}
}

void UAuraAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!Delegate.ExecuteIfBound(AbilitySpec))
		{
			UE_LOG(LogAura, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
		}
	}
}

FGameplayTag UAuraAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	if (AbilitySpec.Ability)
	{
		for (FGameplayTag Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (FGameplayTag Tag : AbilitySpec.DynamicAbilityTags)
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("InputTag"))))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (FGameplayTag StatusTag : AbilitySpec.DynamicAbilityTags)
	{
		if (StatusTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities.Status"))))
		{
			return StatusTag;
		}
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetStatusFromAbilityTag(const FGameplayTag& AbilityTag)
{
	if (const FGameplayAbilitySpec* Spec = GetSpecFromAbilityTag(AbilityTag))
	{
		return GetStatusFromSpec(*Spec);
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetSlotFromAbilityTag(const FGameplayTag& AbilityTag)
{
	if (const FGameplayAbilitySpec* Spec = GetSpecFromAbilityTag(AbilityTag))
	{
		return GetInputTagFromSpec(*Spec);
	}
	return FGameplayTag();
}

bool UAuraAbilitySystemComponent::SlotIsEmpty(const FGameplayTag& Slot)
{
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilityHasSlot(AbilitySpec, Slot))
		{
			return false;
		}
	}
	return true;
}

bool UAuraAbilitySystemComponent::AbilityHasSlot(const FGameplayAbilitySpec& Spec, const FGameplayTag& Slot)
{
	return Spec.DynamicAbilityTags.HasTagExact(Slot);
}

bool UAuraAbilitySystemComponent::AbilityHasAnySlot(const FGameplayAbilitySpec& Spec)
{
	return Spec.DynamicAbilityTags.HasTag(FGameplayTag::RequestGameplayTag(FName("InputTag")));
}

FGameplayAbilitySpec* UAuraAbilitySystemComponent::GetSpecWithSlot(const FGameplayTag& Slot)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.DynamicAbilityTags.HasTagExact(Slot))
		{
			return &AbilitySpec;
		}
	}
	return nullptr;
}

bool UAuraAbilitySystemComponent::IsPassiveAbility(const FGameplayAbilitySpec& Spec) const
{
	const UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
	const FGameplayTag AbilityTag = GetAbilityTagFromSpec(Spec);
	const FAuraAbilityInfo& Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
	const FGameplayTag AbilityType = Info.AbilityType;
	return AbilityType.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Type_Passive);
}

void UAuraAbilitySystemComponent::AssignSlotToAbility(FGameplayAbilitySpec& Spec, const FGameplayTag& Slot)
{
	ClearSlot(&Spec);
	Spec.DynamicAbilityTags.AddTag(Slot);
}

void UAuraAbilitySystemComponent::MulticastActivatePassiveEffect_Implementation(const FGameplayTag& AbilityTag, bool bActivate)
{
	ActivatePassiveEffect.Broadcast(AbilityTag, bActivate);
}

FGameplayAbilitySpec* UAuraAbilitySystemComponent::GetSpecFromAbilityTag(const FGameplayTag& AbilityTag)
{
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		for (FGameplayTag Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(AbilityTag))
			{
				return &AbilitySpec;
			}
		}
	}
	return nullptr;
}

void UAuraAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	if (GetAvatarActor()->Implements<UPlayerInterface>())
	{
		if (IPlayerInterface::Execute_GetAttributePoints(GetAvatarActor()) > 0)
		{
			ServerUpgradeAttribute(AttributeTag);
		}
	}
}

void UAuraAbilitySystemComponent::ServerUpgradeAttribute_Implementation(const FGameplayTag& AttributeTag)
{
	FGameplayEventData Payload;
	Payload.EventTag = AttributeTag;
	Payload.EventMagnitude = 1.f;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(), AttributeTag, Payload);

	if (GetAvatarActor()->Implements<UPlayerInterface>())
	{
		IPlayerInterface::Execute_AddToAttributePoints(GetAvatarActor(), -1);
	}
}

void UAuraAbilitySystemComponent::UpdateAbilityStatuses(int32 Level)
{
	UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
	for (const FAuraAbilityInfo& Info : AbilityInfo->AbilityInformation)
	{
		if (!Info.AbilityTag.IsValid()) continue;
		if (Level < Info.LevelRequirement) continue;
		if (GetSpecFromAbilityTag(Info.AbilityTag) == nullptr)
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Info.Ability, 1);
			AbilitySpec.DynamicAbilityTags.AddTag(FAuraGameplayTags::Get().Abilities_Status_Eligible);
			GiveAbility(AbilitySpec);
			MarkAbilitySpecDirty(AbilitySpec);
			ClientUpdateAbilityStatus(Info.AbilityTag,FAuraGameplayTags::Get().Abilities_Status_Eligible, 1);
		}
	}
}

void UAuraAbilitySystemComponent::GrantAndEquipAllAbilities()
{
	if (!GetAvatarActor() || !GetAvatarActor()->HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("GrantAndEquipAllAbilities aborted. ASC=%s Avatar=%s HasAuthority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActor()),
			(GetAvatarActor() && GetAvatarActor()->HasAuthority()) ? TEXT("true") : TEXT("false"));
		return;
	}

	UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
	if (AbilityInfo == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("GrantAndEquipAllAbilities aborted. AbilityInfo asset missing for avatar %s"), *GetNameSafe(GetAvatarActor()));
		return;
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	TSet<FGameplayTag> ReservedSlots;
	int32 ExistingAbilityCount = 0;
	int32 GrantedAbilityCount = 0;
	int32 EquippedAbilityCount = 0;
	int32 UnlockedOnlyCount = 0;
	int32 PassiveActivationCount = 0;

	UE_LOG(LogAura, Log, TEXT("GrantAndEquipAllAbilities starting for avatar %s with %d ability definitions"),
		*GetNameSafe(GetAvatarActor()),
		AbilityInfo->AbilityInformation.Num());

	FForEachAbility CollectReservedSlotsDelegate;
	CollectReservedSlotsDelegate.BindLambda([this, &ReservedSlots](const FGameplayAbilitySpec& AbilitySpec)
	{
		const FGameplayTag Slot = GetInputTagFromSpec(AbilitySpec);
		if (Slot.IsValid())
		{
			ReservedSlots.Add(Slot);
		}
	});
	ForEachAbility(CollectReservedSlotsDelegate);
	ExistingAbilityCount = GetActivatableAbilities().Num();
	UE_LOG(LogAura, Log, TEXT("GrantAndEquipAllAbilities initial state: existing abilities=%d reserved slots=%d"), ExistingAbilityCount, ReservedSlots.Num());

	for (const FAuraAbilityInfo& Info : AbilityInfo->AbilityInformation)
	{
		if (!Info.AbilityTag.IsValid() || !Info.Ability) continue;
		if (!Info.AbilityType.MatchesTagExact(GameplayTags.Abilities_Type_Offensive) &&
			!Info.AbilityType.MatchesTagExact(GameplayTags.Abilities_Type_Passive))
		{
			continue;
		}

		FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(Info.AbilityTag);
		const bool bHadAbilityAlready = AbilitySpec != nullptr;
		if (AbilitySpec == nullptr)
		{
			FGameplayAbilitySpec NewAbilitySpec = FGameplayAbilitySpec(Info.Ability, 1);
			SetAbilityStatus(NewAbilitySpec, GameplayTags.Abilities_Status_Unlocked);
			GiveAbility(NewAbilitySpec);
			AbilitySpec = GetSpecFromAbilityTag(Info.AbilityTag);
			if (AbilitySpec)
			{
				GrantedAbilityCount++;
				UE_LOG(LogAura, Log, TEXT("FullAbilities granted ability %s class=%s"),
					*Info.AbilityTag.ToString(),
					*GetNameSafe(Info.Ability));
			}
		}

		if (AbilitySpec == nullptr)
		{
			UE_LOG(LogAura, Warning, TEXT("FullAbilities failed to resolve spec for ability %s after grant attempt"), *Info.AbilityTag.ToString());
			continue;
		}

		FGameplayTag PreviousSlot;
		FGameplayTag Slot = GetInputTagFromSpec(*AbilitySpec);
		if (Slot.IsValid())
		{
			if (IsPassiveAbility(*AbilitySpec) && !AbilitySpec->IsActive())
			{
				TryActivateAbility(AbilitySpec->Handle);
				MulticastActivatePassiveEffect(Info.AbilityTag, true);
				PassiveActivationCount++;
				UE_LOG(LogAura, Log, TEXT("FullAbilities activated passive ability %s"), *Info.AbilityTag.ToString());
			}

			SetAbilityStatus(*AbilitySpec, GameplayTags.Abilities_Status_Equipped);
			ReservedSlots.Add(Slot);
		}
		else
		{
			SetAbilityStatus(*AbilitySpec, GameplayTags.Abilities_Status_Unlocked);

			const FGameplayTag DesiredSlot = FindSlotForAbility(Info, *AbilitySpec, ReservedSlots);
			if (DesiredSlot.IsValid() && EquipAbilityToSlot(*AbilitySpec, DesiredSlot, PreviousSlot))
			{
				Slot = DesiredSlot;
				ReservedSlots.Add(Slot);
				if (IsPassiveAbility(*AbilitySpec) && AbilitySpec->IsActive())
				{
					PassiveActivationCount++;
				}
			}
		}

		const FGameplayTag Status = Slot.IsValid() ? GameplayTags.Abilities_Status_Equipped : GameplayTags.Abilities_Status_Unlocked;
		SetAbilityStatus(*AbilitySpec, Status);
		MarkAbilitySpecDirty(*AbilitySpec);
		ClientUpdateAbilityStatus(Info.AbilityTag, Status, AbilitySpec->Level);

		if (Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped))
		{
			EquippedAbilityCount++;
		}
		else
		{
			UnlockedOnlyCount++;
		}

		UE_LOG(LogAura, Log, TEXT("FullAbilities processed ability=%s existing=%s status=%s slot=%s level=%d prevSlot=%s"),
			*Info.AbilityTag.ToString(),
			bHadAbilityAlready ? TEXT("true") : TEXT("false"),
			*Status.ToString(),
			Slot.IsValid() ? *Slot.ToString() : TEXT("None"),
			AbilitySpec->Level,
			PreviousSlot.IsValid() ? *PreviousSlot.ToString() : TEXT("None"));

		if (Slot.IsValid())
		{
			ClientEquipAbility(Info.AbilityTag, Status, Slot, PreviousSlot);
		}
	}

	UE_LOG(LogAura, Log, TEXT("GrantAndEquipAllAbilities complete for avatar %s. ExistingBefore=%d Granted=%d Equipped=%d UnlockedOnly=%d PassiveActivations=%d FinalAbilities=%d"),
		*GetNameSafe(GetAvatarActor()),
		ExistingAbilityCount,
		GrantedAbilityCount,
		EquippedAbilityCount,
		UnlockedOnlyCount,
		PassiveActivationCount,
		GetActivatableAbilities().Num());

	bStartupAbilitiesGiven = true;
	UE_LOG(LogAura, Log, TEXT("GrantAndEquipAllAbilities marked startup abilities as given for ASC=%s"), *GetNameSafe(this));

	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::ServerSpendSpellPoint_Implementation(const FGameplayTag& AbilityTag)
{
	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		if (GetAvatarActor()->Implements<UPlayerInterface>())
		{
			IPlayerInterface::Execute_AddToSpellPoints(GetAvatarActor(), -1);
		}
		
		const FAuraGameplayTags GameplayTags = FAuraGameplayTags::Get();
		FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);
		if (Status.MatchesTagExact(GameplayTags.Abilities_Status_Eligible))
		{
			AbilitySpec->DynamicAbilityTags.RemoveTag(GameplayTags.Abilities_Status_Eligible);
			AbilitySpec->DynamicAbilityTags.AddTag(GameplayTags.Abilities_Status_Unlocked);
			Status = GameplayTags.Abilities_Status_Unlocked;
		}
		else if (Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped) || Status.MatchesTagExact(GameplayTags.Abilities_Status_Unlocked))
		{
			AbilitySpec->Level += 1;
		}
		ClientUpdateAbilityStatus(AbilityTag, Status, AbilitySpec->Level);
		MarkAbilitySpecDirty(*AbilitySpec);
	}
}

void UAuraAbilitySystemComponent::SetAbilityStatus(FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& StatusTag)
{
	const FGameplayTag CurrentStatus = GetStatusFromSpec(AbilitySpec);
	if (CurrentStatus.IsValid())
	{
		AbilitySpec.DynamicAbilityTags.RemoveTag(CurrentStatus);
	}

	if (StatusTag.IsValid())
	{
		AbilitySpec.DynamicAbilityTags.AddTag(StatusTag);
	}
}

bool UAuraAbilitySystemComponent::EquipAbilityToSlot(FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& Slot, FGameplayTag& OutPreviousSlot)
{
	if (!Slot.IsValid()) return false;

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const FGameplayTag AbilityTag = GetAbilityTagFromSpec(AbilitySpec);
	OutPreviousSlot = GetInputTagFromSpec(AbilitySpec);

	const FGameplayTag Status = GetStatusFromSpec(AbilitySpec);
	const bool bStatusValid = Status == GameplayTags.Abilities_Status_Equipped || Status == GameplayTags.Abilities_Status_Unlocked;
	if (!bStatusValid)
	{
		return false;
	}

	if (!SlotIsEmpty(Slot))
	{
		FGameplayAbilitySpec* SpecWithSlot = GetSpecWithSlot(Slot);
		if (SpecWithSlot)
		{
			if (AbilityTag.MatchesTagExact(GetAbilityTagFromSpec(*SpecWithSlot)))
			{
				SetAbilityStatus(AbilitySpec, GameplayTags.Abilities_Status_Equipped);
				return true;
			}

			if (IsPassiveAbility(*SpecWithSlot))
			{
				MulticastActivatePassiveEffect(GetAbilityTagFromSpec(*SpecWithSlot), false);
				DeactivatePassiveAbility.Broadcast(GetAbilityTagFromSpec(*SpecWithSlot));
			}

			ClearSlot(SpecWithSlot);
			MarkAbilitySpecDirty(*SpecWithSlot);
		}
	}

	if (!AbilityHasAnySlot(AbilitySpec) && IsPassiveAbility(AbilitySpec))
	{
		TryActivateAbility(AbilitySpec.Handle);
		MulticastActivatePassiveEffect(AbilityTag, true);
	}

	SetAbilityStatus(AbilitySpec, GameplayTags.Abilities_Status_Equipped);
	AssignSlotToAbility(AbilitySpec, Slot);
	MarkAbilitySpecDirty(AbilitySpec);

	return true;
}

FGameplayTag UAuraAbilitySystemComponent::FindSlotForAbility(const FAuraAbilityInfo& AbilityInfo, const FGameplayAbilitySpec& AbilitySpec, const TSet<FGameplayTag>& ReservedSlots) const
{
	const TArray<FGameplayTag> OrderedSlots = GetOrderedSlotsForAbilityType(AbilityInfo.AbilityType);

	if (const UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilitySpec.Ability))
	{
		if (AuraAbility->StartupInputTag.IsValid() && OrderedSlots.Contains(AuraAbility->StartupInputTag) && !ReservedSlots.Contains(AuraAbility->StartupInputTag))
		{
			return AuraAbility->StartupInputTag;
		}
	}

	for (const FGameplayTag& CandidateSlot : OrderedSlots)
	{
		if (!ReservedSlots.Contains(CandidateSlot))
		{
			return CandidateSlot;
		}
	}

	return FGameplayTag();
}

void UAuraAbilitySystemComponent::ServerEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Slot)
{
	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		const FGameplayTag& PrevSlot = GetInputTagFromSpec(*AbilitySpec);
		const FGameplayTag& Status = GetStatusFromSpec(*AbilitySpec);

		const bool bStatusValid = Status == GameplayTags.Abilities_Status_Equipped || Status == GameplayTags.Abilities_Status_Unlocked;
		if (bStatusValid)
		{

			// Handle activation/deactivation for passive abilities

			if (!SlotIsEmpty(Slot)) // There is an ability in this slot already. Deactivate and clear its slot.
			{
				FGameplayAbilitySpec* SpecWithSlot = GetSpecWithSlot(Slot);
				if (SpecWithSlot)
				{
					// is that ability the same as this ability? If so, we can return early.
					if (AbilityTag.MatchesTagExact(GetAbilityTagFromSpec(*SpecWithSlot)))
					{
						ClientEquipAbility(AbilityTag, GameplayTags.Abilities_Status_Equipped, Slot, PrevSlot);
						return;
					}

					if (IsPassiveAbility(*SpecWithSlot))
					{
						MulticastActivatePassiveEffect(GetAbilityTagFromSpec(*SpecWithSlot), false);
						DeactivatePassiveAbility.Broadcast(GetAbilityTagFromSpec(*SpecWithSlot));
					}

					ClearSlot(SpecWithSlot);
				}
			}

			if (!AbilityHasAnySlot(*AbilitySpec)) // Ability doesn't yet have a slot (it's not active)
			{
				if (IsPassiveAbility(*AbilitySpec))
				{
					TryActivateAbility(AbilitySpec->Handle);
					MulticastActivatePassiveEffect(AbilityTag, true);
				}
				AbilitySpec->DynamicAbilityTags.RemoveTag(GetStatusFromSpec(*AbilitySpec));
				AbilitySpec->DynamicAbilityTags.AddTag(GameplayTags.Abilities_Status_Equipped);
			}
			AssignSlotToAbility(*AbilitySpec, Slot);
			MarkAbilitySpecDirty(*AbilitySpec);
		}
		ClientEquipAbility(AbilityTag, GameplayTags.Abilities_Status_Equipped, Slot, PrevSlot);
	}
}

void UAuraAbilitySystemComponent::ClientEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Status, const FGameplayTag& Slot, const FGameplayTag& PreviousSlot)
{
	AbilityEquipped.Broadcast(AbilityTag, Status, Slot, PreviousSlot);
}

bool UAuraAbilitySystemComponent::GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, FString& OutDescription,FString& OutNextLevelDescription)
{
	if (const FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		if(UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilitySpec->Ability))
		{
			OutDescription = AuraAbility->GetDescription(AbilitySpec->Level);
			OutNextLevelDescription = AuraAbility->GetNextLevelDescription(AbilitySpec->Level + 1);
			return true;
		}
	}
	const UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
	if (!AbilityTag.IsValid() || AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_None))
	{
		OutDescription = FString();
	}
	else
	{
		OutDescription = UAuraGameplayAbility::GetLockedDescription(AbilityInfo->FindAbilityInfoForTag(AbilityTag).LevelRequirement);
	}
	OutNextLevelDescription = FString();
	return false;
}

void UAuraAbilitySystemComponent::ClearSlot(FGameplayAbilitySpec* Spec)
{
	const FGameplayTag Slot = GetInputTagFromSpec(*Spec);
	Spec->DynamicAbilityTags.RemoveTag(Slot);
}

void UAuraAbilitySystemComponent::ClearAbilitiesOfSlot(const FGameplayTag& Slot)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (AbilityHasSlot(&Spec, Slot))
		{
			ClearSlot(&Spec);
		}
	}
}

bool UAuraAbilitySystemComponent::AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot)
{
	for (FGameplayTag Tag : Spec->DynamicAbilityTags)
	{
		if (Tag.MatchesTagExact(Slot))
		{
			return true;
		}
	}
	return false;
}

void UAuraAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	if (!bStartupAbilitiesGiven)
	{
		bStartupAbilitiesGiven = true;
	}

	UE_LOG(LogAura, Log, TEXT("OnRep_ActivateAbilities on ASC=%s Avatar=%s AbilityCount=%d StartupGiven=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActor()),
		GetActivatableAbilities().Num(),
		bStartupAbilitiesGiven ? TEXT("true") : TEXT("false"));

	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::ClientUpdateAbilityStatus_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& StatusTag, int32 AbilityLevel)
{
	UE_LOG(LogAura, Log, TEXT("ClientUpdateAbilityStatus ASC=%s Ability=%s Status=%s Level=%d"),
		*GetNameSafe(this),
		*AbilityTag.ToString(),
		*StatusTag.ToString(),
		AbilityLevel);

	AbilityStatusChanged.Broadcast(AbilityTag, StatusTag, AbilityLevel);
}

void UAuraAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent,
                                                                     const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);

	EffectAssetTags.Broadcast(TagContainer);
}
