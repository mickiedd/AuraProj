// Copyright Druid Mechanics


#include "AbilitySystem/AuraAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Abilities/AuraPassiveAbility.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "Aura/AuraLogChannels.h"
#include "Game/LoadScreenSaveGame.h"
#include "Interaction/PlayerInterface.h"

#include "AuraAbilityGraph/Public/DataAbility.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"

namespace
{
	struct FResolvedAbilitySource
	{
		TSubclassOf<UGameplayAbility> AbilityClass;
		const UAuraAbilityDefinition* Definition = nullptr;
		FGameplayTag AbilityType;
		FGameplayTag InputTag;
	};

	struct FRoleGrantCandidate
	{
		TSubclassOf<UGameplayAbility> AbilityClass;
		UAuraAbilityDefinition* Definition = nullptr;
		FGameplayTag AbilityTag;
		FGameplayTag InputTag;
		bool bPassive = false;
	};

	FGameplayTag GetAbilityTagFromClass(const TSubclassOf<UGameplayAbility>& AbilityClass)
	{
		const UGameplayAbility* DefaultAbility = AbilityClass ? AbilityClass.GetDefaultObject() : nullptr;
		if (!DefaultAbility) return FGameplayTag();
		const FGameplayTag AbilitiesRoot = FGameplayTag::RequestGameplayTag(FName("Abilities"));
		for (const FGameplayTag& Tag : DefaultAbility->AbilityTags)
		{
			if (Tag.MatchesTag(AbilitiesRoot)) return Tag;
		}
		return FGameplayTag();
	}

	FGameplayTag InferAbilityType(const UGameplayAbility* Ability)
	{
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		return Ability && Ability->IsA<UAuraPassiveAbility>() ? Tags.Abilities_Type_Passive : Tags.Abilities_Type_Offensive;
	}

	bool ResolveAbilitySource(const UObject* WorldContextObject, const FGameplayTag& AbilityTag,
		const FGameplayAbilitySpec* LiveSpec, FResolvedAbilitySource& OutSource)
	{
		if (!AbilityTag.IsValid()) return false;

		if (const UAuraAbilityDefinition* Definition = UAuraAbilitySystemLibrary::FindAbilityDefinitionByTag(AbilityTag))
		{
			OutSource.AbilityClass = UAuraDataAbility::StaticClass();
			OutSource.Definition = Definition;
			OutSource.AbilityType = Definition->AbilityType;
			OutSource.InputTag = Definition->InputTag;
			return true;
		}

		if (LiveSpec && LiveSpec->Ability)
		{
			OutSource.AbilityClass = LiveSpec->Ability->GetClass();
			OutSource.AbilityType = InferAbilityType(LiveSpec->Ability);
			OutSource.InputTag = UAuraAbilitySystemComponent::GetInputTagFromSpec(*LiveSpec);
			return true;
		}

		const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(WorldContextObject);
		if (!RoleInfo) return false;
		for (const TPair<FName, FRoleDefaultInfo>& Pair : RoleInfo->RoleInformation)
		{
			TArray<TSubclassOf<UGameplayAbility>> CandidateClasses = Pair.Value.UnlockableAbilities;
			CandidateClasses.Append(Pair.Value.StartupAbilities);
			CandidateClasses.Append(Pair.Value.StartupPassiveAbilities);
			if (Pair.Value.DefaultLMBAbility) CandidateClasses.Add(Pair.Value.DefaultLMBAbility);
			for (const TSubclassOf<UGameplayAbility>& Candidate : CandidateClasses)
			{
				if (GetAbilityTagFromClass(Candidate).MatchesTagExact(AbilityTag))
				{
					OutSource.AbilityClass = Candidate;
					OutSource.AbilityType = InferAbilityType(Candidate.GetDefaultObject());
					if (const UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(Candidate.GetDefaultObject()))
					{
						OutSource.InputTag = AuraAbility->StartupInputTag;
					}
					return true;
				}
			}
		}
		return false;
	}

	FGameplayAbilitySpec MakeAbilitySpec(const FResolvedAbilitySource& Source, int32 Level)
	{
		FGameplayAbilitySpec Spec(Source.AbilityClass, Level);
		if (Source.Definition)
		{
			Spec.SourceObject = const_cast<UAuraAbilityDefinition*>(Source.Definition);
			Spec.GetDynamicSpecSourceTags().AddTag(Source.Definition->AbilityTag);
		}
		return Spec;
	}

	TSubclassOf<UGameplayAbility> GetDataAbilityClass(const UAuraAbilityDefinition* Definition)
	{
		if (!Definition)
		{
			return nullptr;
		}
		if (Definition->AbilityTags.HasTagExact(FAuraGameplayTags::Get().Abilities_Attack))
		{
			return UAuraEnemyAttackDataAbility::StaticClass();
		}
		if (Definition->AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Effects_HitReact))
		{
			return UAuraEnemyHitReactDataAbility::StaticClass();
		}
		return UAuraDataAbility::StaticClass();
	}

	bool BuildRoleGrantCandidates(const FRoleDefaultInfo& Role, TArray<FRoleGrantCandidate>& OutCandidates, FString& OutError)
	{
		TSet<FGameplayTag> StableTags;
		auto AddCandidate = [&OutCandidates, &StableTags, &OutError](TSubclassOf<UGameplayAbility> AbilityClass,
			UAuraAbilityDefinition* Definition, bool bPassive) -> bool
		{
			FRoleGrantCandidate Candidate;
			Candidate.AbilityClass = Definition ? GetDataAbilityClass(Definition) : AbilityClass;
			Candidate.Definition = Definition;
			Candidate.bPassive = bPassive;
			if (Definition)
			{
				Candidate.AbilityTag = Definition->AbilityTag;
				Candidate.InputTag = Definition->InputTag;
			}
			else
			{
				Candidate.AbilityTag = GetAbilityTagFromClass(AbilityClass);
				if (const UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilityClass ? AbilityClass.GetDefaultObject() : nullptr))
				{
					Candidate.InputTag = AuraAbility->StartupInputTag;
				}
			}

			if (!Candidate.AbilityClass || !Candidate.AbilityTag.IsValid())
			{
				OutError = TEXT("Role loadout contains an unresolved ability or a grant without a stable ability tag.");
				return false;
			}
			if (StableTags.Contains(Candidate.AbilityTag))
			{
				OutError = FString::Printf(TEXT("Role loadout duplicates stable ability tag '%s'."), *Candidate.AbilityTag.ToString());
				return false;
			}
			StableTags.Add(Candidate.AbilityTag);
			OutCandidates.Add(MoveTemp(Candidate));
			return true;
		};

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Role.StartupAbilities)
		{
			if (!AddCandidate(AbilityClass, nullptr, false)) return false;
		}
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Role.StartupPassiveAbilities)
		{
			if (!AddCandidate(AbilityClass, nullptr, true)) return false;
		}
		for (const TObjectPtr<UObject>& Object : Role.StartupAbilityDefinitions)
		{
			if (!AddCandidate(nullptr, Cast<UAuraAbilityDefinition>(Object.Get()), false)) return false;
		}
		for (const TObjectPtr<UObject>& Object : Role.StartupPassiveAbilityDefinitions)
		{
			if (!AddCandidate(nullptr, Cast<UAuraAbilityDefinition>(Object.Get()), true)) return false;
		}
		if (Role.DefaultLMBAbilityDefinition)
		{
			if (!AddCandidate(nullptr, Cast<UAuraAbilityDefinition>(Role.DefaultLMBAbilityDefinition.Get()), false)) return false;
		}
		else if (Role.DefaultLMBAbility)
		{
			if (!AddCandidate(Role.DefaultLMBAbility, nullptr, false)) return false;
		}
		return true;
	}

	bool IsLegacyShippedRoleGrant(FName RoleId, const FGameplayTag& AbilityTag)
	{
		TSet<FString> ShippedTags;
		if (RoleId == TEXT("Aura"))
		{
			ShippedTags = {
				TEXT("Abilities.Fire.FireBolt"),
				TEXT("Abilities.Fire.FireBlast"),
				TEXT("Abilities.Arcane.ArcaneShards"),
				TEXT("Abilities.Lightning.Electrocute")
			};
		}
		else if (RoleId == TEXT("BungeeMan"))
		{
			ShippedTags = { TEXT("Abilities.Gun.Fire") };
		}
		return ShippedTags.Contains(AbilityTag.ToString());
	}

	bool IsKnownProgressionGrant(const FRoleDefaultInfo& Role, const FGameplayTag& AbilityTag)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Role.UnlockableAbilities)
		{
			if (GetAbilityTagFromClass(AbilityClass).MatchesTagExact(AbilityTag))
			{
				return true;
			}
		}
		return false;
	}

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

void UAuraAbilitySystemComponent::CaptureRoleGrantState(FAuraRoleGrantTransactionSnapshot& OutSnapshot) const
{
	OutSnapshot.Ledger = RoleGrantLedger;
	OutSnapshot.bStartupAbilitiesGiven = bStartupAbilitiesGiven;
	OutSnapshot.AbilitySpecHandles.Reset();
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		OutSnapshot.AbilitySpecHandles.Add(Spec.Handle);
	}
	OutSnapshot.GrantedAbilityDefinitions = GrantedAbilityDefinitions;
	OutSnapshot.GrantedRoleBySpecHandle = GrantedRoleBySpecHandle;
}

void UAuraAbilitySystemComponent::RollbackRoleGrantState(const FAuraRoleGrantTransactionSnapshot& Snapshot)
{
	TArray<FGameplayAbilitySpecHandle> HandlesToClear;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Snapshot.AbilitySpecHandles.Contains(Spec.Handle))
		{
			HandlesToClear.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : HandlesToClear)
	{
		ClearAbility(Handle);
		GrantedRoleBySpecHandle.Remove(Handle);
	}

	RoleGrantLedger = Snapshot.Ledger;
	bStartupAbilitiesGiven = Snapshot.bStartupAbilitiesGiven;
	GrantedAbilityDefinitions = Snapshot.GrantedAbilityDefinitions;
	GrantedRoleBySpecHandle = Snapshot.GrantedRoleBySpecHandle;
}

int32 UAuraAbilitySystemComponent::CountAbilitySpecsByTag(const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid()) return 0;
	int32 Count = 0;
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (GetAbilityTagFromSpec(Spec).MatchesTagExact(AbilityTag))
		{
			++Count;
		}
	}
	return Count;
}

EAuraAbilityGrantSource UAuraAbilitySystemComponent::GetGrantSourceForSpec(
	const FGameplayAbilitySpec& AbilitySpec, FName& OutGrantedRoleId) const
{
	OutGrantedRoleId = NAME_None;
	if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(FAuraGameplayTags::Get().GrantSource_Role))
	{
		if (const FName* RoleId = GrantedRoleBySpecHandle.Find(AbilitySpec.Handle))
		{
			OutGrantedRoleId = *RoleId;
		}
		else if (RoleGrantLedger.AbilitySpecHandles.Contains(AbilitySpec.Handle))
		{
			OutGrantedRoleId = RoleGrantLedger.GrantedRoleId;
		}
		return EAuraAbilityGrantSource::Role;
	}
	return EAuraAbilityGrantSource::Progression;
}

bool UAuraAbilitySystemComponent::ValidateRoleGrantSet(FName RoleId, const FRoleDefaultInfo& Role,
	const ULoadScreenSaveGame* SaveData, FString& OutError) const
{
	OutError.Reset();
	if (!IsOwnerActorAuthoritative())
	{
		OutError = TEXT("Only the authoritative ASC may reconcile role grants.");
		return false;
	}
	if (RoleId.IsNone())
	{
		OutError = TEXT("A stable role ID is required for role grants.");
		return false;
	}
	if (RoleGrantLedger.bInitialized && RoleGrantLedger.GrantedRoleId != RoleId)
	{
		OutError = FString::Printf(TEXT("Live role switching from '%s' to '%s' is unsupported."),
			*RoleGrantLedger.GrantedRoleId.ToString(), *RoleId.ToString());
		return false;
	}

	TArray<FRoleGrantCandidate> Candidates;
	if (!BuildRoleGrantCandidates(Role, Candidates, OutError))
	{
		return false;
	}
	for (const FRoleGrantCandidate& Candidate : Candidates)
	{
		if (CountAbilitySpecsByTag(Candidate.AbilityTag) > 1)
		{
			OutError = FString::Printf(TEXT("ASC already contains duplicate specs for '%s'."), *Candidate.AbilityTag.ToString());
			return false;
		}
	}

	if (SaveData && SaveData->Role != RoleId)
	{
		OutError = FString::Printf(TEXT("Save role '%s' does not match authorized role '%s'."),
			*SaveData->Role.ToString(), *RoleId.ToString());
		return false;
	}
	return true;
}

bool UAuraAbilitySystemComponent::ApplyRoleGrantSet(FName RoleId, int32 RoleDefinitionVersion,
	const FRoleDefaultInfo& Role, const ULoadScreenSaveGame* SaveData, FString& OutError)
{
	if (!ValidateRoleGrantSet(RoleId, Role, SaveData, OutError))
	{
		return false;
	}

	TArray<FRoleGrantCandidate> Candidates;
	if (!BuildRoleGrantCandidates(Role, Candidates, OutError))
	{
		return false;
	}
	const bool bWasInitialized = RoleGrantLedger.bInitialized;

	auto RecordRoleSpec = [this, RoleId](const FGameplayAbilitySpecHandle Handle)
	{
		RoleGrantLedger.AbilitySpecHandles.AddUnique(Handle);
		GrantedRoleBySpecHandle.Add(Handle, RoleId);
	};

	if (!bWasInitialized && SaveData)
	{
		for (const FSavedAbility& Data : SaveData->SavedAbilities)
		{
			if (!Data.AbilityTag.IsValid() || CountAbilitySpecsByTag(Data.AbilityTag) > 0)
			{
				continue;
			}

			if (Data.ProvenanceVersion < 1 || Data.ProvenanceVersion > 1
				|| Data.GrantSource > static_cast<uint8>(EAuraAbilityGrantSource::Progression))
			{
				UE_LOG(LogAura, Error, TEXT("[RoleGrant] Quarantined ability '%s' with unsupported provenance version/source."),
					*Data.AbilityTag.ToString());
				continue;
			}
			const bool bLegacyUnknownProvenance = Data.GrantSource == static_cast<uint8>(EAuraAbilityGrantSource::Unknown);
			EAuraAbilityGrantSource Provenance = static_cast<EAuraAbilityGrantSource>(Data.GrantSource);
			if (Provenance == EAuraAbilityGrantSource::Unknown)
			{
				if (IsLegacyShippedRoleGrant(RoleId, Data.AbilityTag))
				{
					Provenance = EAuraAbilityGrantSource::Role;
				}
				else if (IsKnownProgressionGrant(Role, Data.AbilityTag))
				{
					Provenance = EAuraAbilityGrantSource::Progression;
				}
				else
				{
					UE_LOG(LogAura, Error, TEXT("[RoleGrant] Quarantined unknown legacy ability '%s' for role '%s'."),
						*Data.AbilityTag.ToString(), *RoleId.ToString());
					continue;
				}
			}
			if (Provenance == EAuraAbilityGrantSource::Progression && !IsKnownProgressionGrant(Role, Data.AbilityTag))
			{
				UE_LOG(LogAura, Error, TEXT("[RoleGrant] Quarantined saved progression ability '%s' because it is not an unlockable grant for role '%s'."),
					*Data.AbilityTag.ToString(), *RoleId.ToString());
				continue;
			}
			if ((!bLegacyUnknownProvenance && Provenance == EAuraAbilityGrantSource::Role && Data.GrantedRoleId != RoleId)
				|| (!bLegacyUnknownProvenance && Provenance == EAuraAbilityGrantSource::Progression && !Data.GrantedRoleId.IsNone()))
			{
				UE_LOG(LogAura, Error, TEXT("[RoleGrant] Quarantined saved ability '%s' with mismatched provenance role metadata."),
					*Data.AbilityTag.ToString());
				continue;
			}

			const bool bAllowedRoleGrant = Candidates.ContainsByPredicate([&Data](const FRoleGrantCandidate& Candidate)
			{
				return Candidate.AbilityTag.MatchesTagExact(Data.AbilityTag);
			});
			if (Provenance == EAuraAbilityGrantSource::Role
				&& ((!Data.GrantedRoleId.IsNone() && Data.GrantedRoleId != RoleId) || !bAllowedRoleGrant))
			{
				UE_LOG(LogAura, Warning, TEXT("[RoleGrant] Removed/mismatched role grant '%s' was not restored or promoted."),
					*Data.AbilityTag.ToString());
				continue;
			}

			FResolvedAbilitySource Source;
			if (!ResolveAbilitySource(GetAvatarActor(), Data.AbilityTag, nullptr, Source))
			{
				UE_LOG(LogAura, Error, TEXT("[RoleGrant] Save restore skipped unresolved ability '%s'."), *Data.AbilityTag.ToString());
				continue;
			}
			const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
			const bool bValidStatus = !Data.AbilityStatus.IsValid()
				|| Data.AbilityStatus == GameplayTags.Abilities_Status_Locked
				|| Data.AbilityStatus == GameplayTags.Abilities_Status_Eligible
				|| Data.AbilityStatus == GameplayTags.Abilities_Status_Unlocked
				|| Data.AbilityStatus == GameplayTags.Abilities_Status_Equipped;
			const bool bValidSlot = !Data.AbilitySlot.IsValid()
				|| GetOrderedSlotsForAbilityType(Source.AbilityType).Contains(Data.AbilitySlot);
			if (!bValidStatus || !bValidSlot
				|| (Data.AbilityStatus == GameplayTags.Abilities_Status_Equipped && !Data.AbilitySlot.IsValid()))
			{
				UE_LOG(LogAura, Error, TEXT("[RoleGrant] Quarantined saved ability '%s' with invalid status/slot combination."),
					*Data.AbilityTag.ToString());
				continue;
			}
			FGameplayAbilitySpec Spec = MakeAbilitySpec(Source, FMath::Max(1, Data.AbilityLevel));
			if (Data.AbilitySlot.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Data.AbilitySlot);
			if (Data.AbilityStatus.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Data.AbilityStatus);
			if (Provenance == EAuraAbilityGrantSource::Role)
			{
				Spec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().GrantSource_Role);
			}
			if (Source.Definition)
			{
				GrantedAbilityDefinitions.AddUnique(const_cast<UAuraAbilityDefinition*>(Source.Definition));
			}

			const bool bActivatePassive = Source.AbilityType.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Type_Passive)
				&& Data.AbilityStatus.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Equipped);
			const FGameplayAbilitySpecHandle Handle = bActivatePassive ? GiveAbilityAndActivateOnce(Spec) : GiveAbility(Spec);
			if (!Handle.IsValid())
			{
				OutError = FString::Printf(TEXT("Failed to grant saved ability '%s'."), *Data.AbilityTag.ToString());
				return false;
			}
			if (Provenance == EAuraAbilityGrantSource::Role)
			{
				RecordRoleSpec(Handle);
			}
			if (bActivatePassive)
			{
				MulticastActivatePassiveEffect(Data.AbilityTag, true);
			}
		}
	}

	for (const FRoleGrantCandidate& Candidate : Candidates)
	{
		FGameplayAbilitySpec* Existing = GetSpecFromAbilityTag(Candidate.AbilityTag);
		if (Existing)
		{
			if (Existing->GetDynamicSpecSourceTags().HasTagExact(FAuraGameplayTags::Get().GrantSource_Role))
			{
				RecordRoleSpec(Existing->Handle);
			}
			continue;
		}

		FGameplayAbilitySpec Spec(Candidate.AbilityClass, 1);
		if (Candidate.Definition)
		{
			GrantedAbilityDefinitions.AddUnique(Candidate.Definition);
			Spec.SourceObject = Candidate.Definition;
			Spec.GetDynamicSpecSourceTags().AppendTags(Candidate.Definition->AbilityTags);
			Spec.GetDynamicSpecSourceTags().AddTag(Candidate.AbilityTag);
		}
		if (Candidate.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Candidate.InputTag);
		}
		Spec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
		Spec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().GrantSource_Role);
		const FGameplayAbilitySpecHandle Handle = Candidate.bPassive ? GiveAbilityAndActivateOnce(Spec) : GiveAbility(Spec);
		if (!Handle.IsValid())
		{
			OutError = FString::Printf(TEXT("Failed to grant role ability '%s'."), *Candidate.AbilityTag.ToString());
			return false;
		}
		RecordRoleSpec(Handle);
		if (Candidate.bPassive)
		{
			MulticastActivatePassiveEffect(Candidate.AbilityTag, true);
		}
	}

	RoleGrantLedger.GrantedRoleId = RoleId;
	RoleGrantLedger.RoleDefinitionVersion = RoleDefinitionVersion;
	RoleGrantLedger.bInitialized = true;
	RoleGrantLedger.bReconciled = true;
	bStartupAbilitiesGiven = true;
	if (!bWasInitialized)
	{
		AbilitiesGivenDelegate.Broadcast();
	}
	UE_LOG(LogAura, Display, TEXT("[RoleGrant][Server] Role=%s Required=%d OwnedSpecs=%d ReusedLedger=%d"),
		*RoleId.ToString(), Candidates.Num(), RoleGrantLedger.AbilitySpecHandles.Num(), bWasInitialized);
	return true;
}

void UAuraAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AuraAbility->StartupInputTag);
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
			GiveAbility(AbilitySpec);
		}
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::AddCharacterDataAbilities(const TArray<UAuraAbilityDefinition*>& Definitions, int32 AbilityLevel)
{
	UE_LOG(LogAura, Log, TEXT("[ASC] AddCharacterDataAbilities count=%d"), Definitions.Num());
	for (UAuraAbilityDefinition* Definition : Definitions)
	{
		if (!Definition || !Definition->AbilityTag.IsValid())
		{
			UE_LOG(LogAura, Warning, TEXT("[ASC] AddCharacterDataAbilities skipping invalid definition"));
			continue;
		}

		TSubclassOf<UGameplayAbility> DataAbilityClass = UAuraDataAbility::StaticClass();
		if (Definition->AbilityTags.HasTagExact(FAuraGameplayTags::Get().Abilities_Attack))
		{
			DataAbilityClass = UAuraEnemyAttackDataAbility::StaticClass();
		}
		else if (Definition->AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Effects_HitReact))
		{
			DataAbilityClass = UAuraEnemyHitReactDataAbility::StaticClass();
		}
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(DataAbilityClass, FMath::Max(1, AbilityLevel));
		GrantedAbilityDefinitions.AddUnique(Definition);
		AbilitySpec.SourceObject = Definition;
		if (Definition->InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Definition->InputTag);
		}
		AbilitySpec.GetDynamicSpecSourceTags().AppendTags(Definition->AbilityTags);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
		GiveAbility(AbilitySpec);
		UE_LOG(LogAura, Log, TEXT("[ASC] AddCharacterDataAbilities granted definition=%s InputTag=%s AbilityTag=%s"),
			*Definition->AbilityTag.ToString(), *Definition->InputTag.ToString(), *Definition->AbilityTag.ToString());
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::AddCharacterDataPassiveAbilities(const TArray<UAuraAbilityDefinition*>& Definitions)
{
	UE_LOG(LogAura, Log, TEXT("[ASC] AddCharacterDataPassiveAbilities count=%d"), Definitions.Num());
	for (UAuraAbilityDefinition* Definition : Definitions)
	{
		if (!Definition || !Definition->AbilityTag.IsValid())
		{
			UE_LOG(LogAura, Warning, TEXT("[ASC] AddCharacterDataPassiveAbilities skipping invalid definition"));
			continue;
		}

		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(UAuraDataAbility::StaticClass(), 1);
		GrantedAbilityDefinitions.AddUnique(Definition);
		AbilitySpec.SourceObject = Definition;
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(Definition->AbilityTag);
		AbilitySpec.GetDynamicSpecSourceTags().AppendTags(Definition->AbilityTags);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
		GiveAbilityAndActivateOnce(AbilitySpec);
		UE_LOG(LogAura, Log, TEXT("[ASC] AddCharacterDataPassiveAbilities granted definition=%s"), *Definition->AbilityTag.ToString());
	}
}

void UAuraAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	UE_LOG(LogAura, Log, TEXT("[ASC] AbilityInputTagPressed: Tag=%s"), *InputTag.ToString());
	FScopedAbilityListLock ActiveScopeLoc(*this);
	int32 MatchCount = 0;
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			++MatchCount;
			const FGameplayTag AbilityTag = GetAbilityTagFromSpec(AbilitySpec);
			const FGameplayTag StatusTag = GetStatusFromSpec(AbilitySpec);
			UE_LOG(LogAura, Log, TEXT("[ASC] AbilityInputTagPressed: Found ability Ability=%s Status=%s IsActive=%s"),
				*AbilityTag.ToString(), *StatusTag.ToString(), AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle, AbilitySpec.ActivationInfo.GetActivationPredictionKey());
			}
		}
	}
	if (MatchCount == 0)
	{
		UE_LOG(LogAura, Warning, TEXT("[ASC] AbilityInputTagPressed: No ability found with InputTag=%s (total activatable: %d)"),
			*InputTag.ToString(), GetActivatableAbilities().Num());
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			const FGameplayTag AbilityTag = GetAbilityTagFromSpec(AbilitySpec);
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				const float NextAllowed = NextAllowedInputTagTryTime.FindRef(InputTag);
				if (Now < NextAllowed)
				{
					continue;
				}

				// Log CanActivateAbility failure reason before attempting activation
				if (AbilitySpec.Ability && AbilityActorInfo.IsValid())
				{
					UAbilitySystemComponent* AbilityASC = AbilityActorInfo->AbilitySystemComponent.Get();
					AActor* LogOwnerActor = AbilityASC ? AbilityASC->GetOwnerActor() : nullptr;
					
					FGameplayTagContainer CooldownTags;
					const bool bCooldownOk = AbilitySpec.Ability->CheckCooldown(AbilitySpec.Handle, AbilityActorInfo.Get(), &CooldownTags);
					FGameplayTagContainer CostTags;
					const bool bCostOk = AbilitySpec.Ability->CheckCost(AbilitySpec.Handle, AbilityActorInfo.Get(), &CostTags);

					// Log actual mana to diagnose cost failures
					float CurrentMana = 0.f;
					float MaxMana = 0.f;
					bool bAttributeSetFound = false;
					if (AbilityASC)
					{
						if (const UAuraAttributeSet* AuraAS = AbilityASC->GetSet<UAuraAttributeSet>())
						{
							bAttributeSetFound = true;
							CurrentMana = AuraAS->GetMana();
							MaxMana = AuraAS->GetMaxMana();
						}
					}
					UE_LOG(LogAura, Log, TEXT("[ASC] AttributeSetFound=%s Mana=%.1f/%.1f"),
						bAttributeSetFound ? TEXT("true") : TEXT("false"), CurrentMana, MaxMana);
					
					FGameplayTagContainer FailureTags;
					const bool bCanActivate = AbilitySpec.Ability->CanActivateAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), nullptr, nullptr, &FailureTags);
					const EGameplayAbilityNetExecutionPolicy::Type NetPolicy = AbilitySpec.Ability->GetNetExecutionPolicy();
					UE_LOG(LogAura, Log, TEXT("[ASC] CanActivate=%s Ability=%s Tag=%s NetPolicy=%d AvatarActor=%s OwnerActor=%s IsNetAuth=%s Mana=%.1f/%.1f CooldownOk=%s CostOk=%s FailureTags=[%s]"),
						bCanActivate ? TEXT("true") : TEXT("false"),
						*AbilityTag.ToString(), *InputTag.ToString(),
						(int32)NetPolicy,
						*GetNameSafe(AbilityActorInfo->AvatarActor.Get()),
						*GetNameSafe(LogOwnerActor),
						AbilityActorInfo->IsNetAuthority() ? TEXT("true") : TEXT("false"),
						CurrentMana, MaxMana,
						bCooldownOk ? TEXT("true") : TEXT("false"),
						bCostOk ? TEXT("true") : TEXT("false"),
						*FailureTags.ToString());

					if (!bCanActivate)
					{
						float RetryDelay = HeldRetryDelay;
						if (!bCostOk)
						{
							RetryDelay = HeldCostRetryDelay;
						}
						else if (!bCooldownOk)
						{
							RetryDelay = HeldCooldownRetryDelay;
						}
						NextAllowedInputTagTryTime.FindOrAdd(InputTag) = Now + RetryDelay;
						continue;
					}
				}
				else
				{
					UE_LOG(LogAura, Warning, TEXT("[ASC] AbilityInputTagHeld: AbilityActorInfo invalid! Ability=%s AbilityActorInfoValid=%s"),
						*AbilityTag.ToString(), AbilityActorInfo.IsValid() ? TEXT("true") : TEXT("false"));
					NextAllowedInputTagTryTime.FindOrAdd(InputTag) = Now + HeldRetryDelay;
					continue;
				}

				UE_LOG(LogAura, Log, TEXT("[ASC] AbilityInputTagHeld: TryActivateAbility Ability=%s Tag=%s"),
					*AbilityTag.ToString(), *InputTag.ToString());
				const bool bActivated = TryActivateAbility(AbilitySpec.Handle);
				if (bActivated)
				{
					NextAllowedInputTagTryTime.FindOrAdd(InputTag) = Now + HeldSuccessRetryDelay;
				}
				else
				{
					NextAllowedInputTagTryTime.FindOrAdd(InputTag) = Now + HeldRetryDelay;
				}
				if (!bActivated)
				{
					UE_LOG(LogAura, Warning, TEXT("[ASC] AbilityInputTagHeld: TryActivateAbility FAILED for Ability=%s"), *AbilityTag.ToString());
				}
			}
		}
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	NextAllowedInputTagTryTime.Remove(InputTag);
	UE_LOG(LogAura, Log, TEXT("[ASC] AbilityInputTagReleased: Tag=%s"), *InputTag.ToString());
	FScopedAbilityListLock ActiveScopeLoc(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			const FGameplayTag AbilityTag = GetAbilityTagFromSpec(AbilitySpec);
			UE_LOG(LogAura, Log, TEXT("[ASC] AbilityInputTagReleased: Ability=%s IsActive=%s"),
				*AbilityTag.ToString(), AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
			if (AbilitySpec.IsActive())
			{
				AbilitySpecInputReleased(AbilitySpec);
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, AbilitySpec.ActivationInfo.GetActivationPredictionKey());
			}
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
	UE_LOG(LogAura, Log, TEXT("[ASC] GetAbilityTagFromSpec Ability=%s DynamicTags=%d"),
		*GetNameSafe(AbilitySpec.Ability), AbilitySpec.GetDynamicSpecSourceTags().Num());
	if (AbilitySpec.Ability)
	{
		for (FGameplayTag Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
			{
				UE_LOG(LogAura, Log, TEXT("[ASC] GetAbilityTagFromSpec resolved from AbilityTags: %s"), *Tag.ToString());
				return Tag;
			}
		}
	}
	for (FGameplayTag Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
		{
				UE_LOG(LogAura, Log, TEXT("[ASC] GetAbilityTagFromSpec resolved from dynamic spec source tags: %s"), *Tag.ToString());
			return Tag;
		}
	}
	UE_LOG(LogAura, Warning, TEXT("[ASC] GetAbilityTagFromSpec FAILED to resolve ability tag"));
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (FGameplayTag Tag : AbilitySpec.GetDynamicSpecSourceTags())
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
	for (FGameplayTag StatusTag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (StatusTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities.Status"))))
		{
			return StatusTag;
		}
	}
	return FGameplayTag();
}

FAuraAbilityInfo UAuraAbilitySystemComponent::GetRuntimeAbilityInfoForSpec(const FGameplayAbilitySpec& AbilitySpec) const
{
	const FGameplayTag AbilityTag = GetAbilityTagFromSpec(AbilitySpec);
	FAuraAbilityInfo Info = GetRuntimeAbilityInfoForTag(AbilityTag);
	Info.InputTag = GetInputTagFromSpec(AbilitySpec);
	Info.StatusTag = GetStatusFromSpec(AbilitySpec);
	return Info;
}

FAuraAbilityInfo UAuraAbilitySystemComponent::GetRuntimeAbilityInfoForTag(const FGameplayTag& AbilityTag) const
{
	FAuraAbilityInfo Info;
	const AActor* AvatarActorContext = AbilityActorInfo.IsValid() ? GetAvatarActor() : nullptr;
	if (const URuntimeAbilityInfo* RuntimeInfo = UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(AvatarActorContext))
	{
		Info = RuntimeInfo->FindAbilityInfoForTag(AbilityTag, true);
	}
	Info.AbilityTag = AbilityTag;

	const FGameplayAbilitySpec* LiveSpec = nullptr;
	if (AbilityActorInfo.IsValid())
	{
		LiveSpec = const_cast<UAuraAbilitySystemComponent*>(this)->GetSpecFromAbilityTag(AbilityTag);
	}
	FResolvedAbilitySource Source;
	if (ResolveAbilitySource(AvatarActorContext, AbilityTag, LiveSpec, Source))
	{
		Info.AbilityType = Source.AbilityType;
		Info.InputTag = LiveSpec ? GetInputTagFromSpec(*LiveSpec) : Source.InputTag;
		if (Source.Definition) Info.CooldownTag = Source.Definition->CooldownTag;
	}
	return Info;
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
	return Spec.GetDynamicSpecSourceTags().HasTagExact(Slot);
}

bool UAuraAbilitySystemComponent::AbilityHasAnySlot(const FGameplayAbilitySpec& Spec)
{
	return Spec.GetDynamicSpecSourceTags().HasTag(FGameplayTag::RequestGameplayTag(FName("InputTag")));
}

FGameplayAbilitySpec* UAuraAbilitySystemComponent::GetSpecWithSlot(const FGameplayTag& Slot)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Slot))
		{
			return &AbilitySpec;
		}
	}
	return nullptr;
}

bool UAuraAbilitySystemComponent::IsPassiveAbility(const FGameplayAbilitySpec& Spec) const
{
	return GetRuntimeAbilityInfoForSpec(Spec).AbilityType.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Type_Passive);
}

void UAuraAbilitySystemComponent::AssignSlotToAbility(FGameplayAbilitySpec& Spec, const FGameplayTag& Slot)
{
	ClearSlot(&Spec);
	Spec.GetDynamicSpecSourceTags().AddTag(Slot);
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
		if (GetAbilityTagFromSpec(AbilitySpec).MatchesTagExact(AbilityTag)) return &AbilitySpec;
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
	AActor* Avatar = GetAvatarActor();
	if (!IsOwnerActorAuthoritative() || !Avatar || !Avatar->Implements<UPlayerInterface>())
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected attribute upgrade without an authoritative player avatar."));
		return;
	}
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const bool bPrimaryAttribute = AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Strength)
		|| AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Intelligence)
		|| AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Resilience)
		|| AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Vigor);
	if (!bPrimaryAttribute || IPlayerInterface::Execute_GetAttributePoints(Avatar) <= 0)
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected forged attribute upgrade tag=%s or exhausted points."), *AttributeTag.ToString());
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = AttributeTag;
	Payload.EventMagnitude = 1.f;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, AttributeTag, Payload);
	IPlayerInterface::Execute_AddToAttributePoints(Avatar, -1);
}

void UAuraAbilitySystemComponent::UpdateAbilityStatuses(int32 Level)
{
	const URuntimeAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(GetAvatarActor());
	if (!AbilityInfo)
	{
		UE_LOG(LogAura, Error, TEXT("UpdateAbilityStatuses aborted: AbilityInfo.json is unavailable"));
		return;
	}
	for (const FAuraAbilityInfo& Info : AbilityInfo->GetAllAbilityInfo())
	{
		if (!Info.AbilityTag.IsValid()) continue;
		if (Level < Info.LevelRequirement) continue;
		if (GetSpecFromAbilityTag(Info.AbilityTag) == nullptr)
		{
			FResolvedAbilitySource Source;
			if (!ResolveAbilitySource(GetAvatarActor(), Info.AbilityTag, nullptr, Source))
			{
				UE_LOG(LogAura, Warning, TEXT("UpdateAbilityStatuses has metadata but no runtime source for %s"), *Info.AbilityTag.ToString());
				continue;
			}
			FGameplayAbilitySpec AbilitySpec = MakeAbilitySpec(Source, 1);
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Eligible);
			GiveAbility(AbilitySpec);
			ClientUpdateAbilityStatus(Info.AbilityTag,FAuraGameplayTags::Get().Abilities_Status_Eligible, 1);
		}
	}
}

void UAuraAbilitySystemComponent::ServerSpendSpellPoint_Implementation(const FGameplayTag& AbilityTag)
{
	AActor* Avatar = GetAvatarActor();
	if (!IsOwnerActorAuthoritative() || !Avatar || !Avatar->Implements<UPlayerInterface>()
		|| IPlayerInterface::Execute_GetSpellPoints(Avatar) <= 0)
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected spell-point spend without authority, avatar, or available points."));
		return;
	}
	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		if (RoleGrantLedger.AbilitySpecHandles.Contains(AbilitySpec->Handle))
		{
			UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected spell-point spend on role-owned ability=%s."), *AbilityTag.ToString());
			return;
		}
		
		const FAuraGameplayTags GameplayTags = FAuraGameplayTags::Get();
		FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);
		if (!Status.MatchesTagExact(GameplayTags.Abilities_Status_Eligible)
			&& !Status.MatchesTagExact(GameplayTags.Abilities_Status_Unlocked)
			&& !Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped))
		{
			UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected spell-point spend on invalid ability status=%s ability=%s."),
				*Status.ToString(), *AbilityTag.ToString());
			return;
		}
		IPlayerInterface::Execute_AddToSpellPoints(Avatar, -1);
		if (Status.MatchesTagExact(GameplayTags.Abilities_Status_Eligible))
		{
			AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GameplayTags.Abilities_Status_Eligible);
			AbilitySpec->GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_Unlocked);
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
		AbilitySpec.GetDynamicSpecSourceTags().RemoveTag(CurrentStatus);
	}

	if (StatusTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(StatusTag);
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

void UAuraAbilitySystemComponent::ServerRequestActivateAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle)
{
	if (!IsOwnerActorAuthoritative() || !GetAvatarActor() || !GetAvatarActor()->HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected ability activation without authoritative avatar."));
		return;
	}
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityHandle);
	const FGameplayTag Status = AbilitySpec ? GetStatusFromSpec(*AbilitySpec) : FGameplayTag();
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (!AbilitySpec || !AbilitySpec->Ability
		|| !Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped)
		|| !AbilityHasAnySlot(*AbilitySpec))
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected forged or ineligible ability activation handle=%s."), *AbilityHandle.ToString());
		return;
	}
	UE_LOG(LogAura, Log, TEXT("[ASC] ServerRequestActivateAbility: Handle=%s"), *AbilityHandle.ToString());
	TryActivateAbility(AbilityHandle);
}

void UAuraAbilitySystemComponent::ServerEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Slot)
{
	if (!IsOwnerActorAuthoritative() || !GetAvatarActor() || !GetAvatarActor()->HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected ability equip without authoritative avatar."));
		return;
	}
	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		const bool bPassiveSlot = Slot == GameplayTags.InputTag_Passive_1 || Slot == GameplayTags.InputTag_Passive_2;
		const bool bActiveSlot = Slot == GameplayTags.InputTag_LMB || Slot == GameplayTags.InputTag_RMB
			|| Slot == GameplayTags.InputTag_1 || Slot == GameplayTags.InputTag_2
			|| Slot == GameplayTags.InputTag_3 || Slot == GameplayTags.InputTag_4;
		if ((!bPassiveSlot && !bActiveSlot) || bPassiveSlot != IsPassiveAbility(*AbilitySpec))
		{
			UE_LOG(LogAura, Warning, TEXT("[Security][Server] Rejected forged or incompatible ability slot ability=%s slot=%s."),
				*AbilityTag.ToString(), *Slot.ToString());
			return;
		}
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
				AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GetStatusFromSpec(*AbilitySpec));
				AbilitySpec->GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_Equipped);
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
	if (!AbilityTag.IsValid() || AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_None))
	{
		OutDescription = FString();
	}
	else
	{
		OutDescription = UAuraGameplayAbility::GetLockedDescription(GetRuntimeAbilityInfoForTag(AbilityTag).LevelRequirement);
	}
	OutNextLevelDescription = FString();
	return false;
}

void UAuraAbilitySystemComponent::ClearSlot(FGameplayAbilitySpec* Spec)
{
	const FGameplayTag Slot = GetInputTagFromSpec(*Spec);
	Spec->GetDynamicSpecSourceTags().RemoveTag(Slot);
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
	for (FGameplayTag Tag : Spec->GetDynamicSpecSourceTags())
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
