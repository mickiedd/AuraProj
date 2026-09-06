// Copyright Druid Mechanics

#include "Game/AuraPersistenceSubsystem.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Economy/AuraCurrencyComponent.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraInventoryComponent.h"
#include "Game/AuraPersistenceManifestSaveGame.h"
#include "Game/AuraPlayerSaveGame.h"
#include "Game/AuraWorldSaveGame.h"
#include "Game/LoadScreenSaveGame.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Player/AuraPlayerState.h"
#include "OnlineSubsystemTypes.h"
#include "UObject/UObjectGlobals.h"

namespace AuraPersistenceSubsystemPrivate
{
	FString GetConnectionOption(const FString& Options, const TCHAR* Key)
	{
		const FString Token = FString::Printf(TEXT("?%s="), Key);
		const int32 TokenIndex = Options.Find(Token, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (TokenIndex == INDEX_NONE) return FString();
		const int32 ValueStart = TokenIndex + Token.Len();
		const int32 NextOption = Options.Find(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		return Options.Mid(ValueStart, NextOption == INDEX_NONE ? MAX_int32 : NextOption - ValueStart);
	}

	FString HashText(const FString& Text)
	{
		return FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Text));
	}

	FString RecordText(const UAuraPlayerSaveGame& Save)
	{
		FString Text = FString::Printf(TEXT("%d|%d|%s|%s|%s|%lld|%u|%u|%s|%d|%d|%d|%d|%d|%.6f|%.6f|%.6f|%.6f|%d|"),
			Save.SaveSchemaVersion, Save.RecordGeneration, *Save.IdentityProvider, *Save.IdentityValue,
			*Save.CurrencyId.ToString(), Save.CurrencyBalance, Save.CurrencyRevision, Save.InventoryRevision,
			*Save.Role.ToString(), Save.PlayerLevel, Save.XP, Save.SpellPoints, Save.AttributePoints,
			Save.bFirstTimeLoadIn ? 1 : 0, Save.Strength, Save.Intelligence, Save.Resilience, Save.Vigor,
			Save.SavedAbilities.Num());
		Text += FString::Printf(TEXT("%d|%d|%d|%d|%d|%.3f|%u|%u|%s|"), Save.bFirearmApplicable ? 1 : 0,
			Save.FirearmMagazineCapacity, Save.FirearmMagazineRounds, Save.FirearmReserveCapacity, Save.FirearmReserveRounds,
			Save.FirearmReloadDuration, Save.FirearmAmmoRevision, Save.TutorialCompletionMask, *Save.RecoveryState.ToString());
		for (const FAuraInventorySlot& Slot : Save.InventorySlots)
		{
			Text += FString::Printf(TEXT("%s:%lld;"), *Slot.ItemId.ToString(), Slot.Quantity);
		}
		for (const FSavedAbility& Ability : Save.SavedAbilities)
		{
			Text += FString::Printf(TEXT("%s:%s:%s:%d:%d:%s:%d;"), *Ability.AbilityTag.ToString(),
				*Ability.AbilityStatus.ToString(), *Ability.AbilitySlot.ToString(), Ability.AbilityLevel,
				Ability.GrantSource, *Ability.GrantedRoleId.ToString(), Ability.ProvenanceVersion);
		}
		return Text;
	}

	FString RecordText(const UAuraWorldSaveGame& Save)
	{
		FString Text = FString::Printf(TEXT("%d|%d|%s|%s|%d|%d|"), Save.SaveSchemaVersion, Save.RecordGeneration,
			*Save.WorldPersistenceId, *Save.MapPackage, Save.PopulationSlots.Num(), Save.Merchants.Num());
		for (const FAuraPopulationSlotSnapshot& Slot : Save.PopulationSlots)
		{
			Text += FString::Printf(TEXT("%s:%d:%d:%d:%d:%.3f:%.3f;"), *Slot.PopulationMemberId.ToString(),
				Slot.SlotIndex, static_cast<int32>(Slot.State), Slot.Generation, Slot.DeathSequence,
				Slot.RemainingCorpseDelay, Slot.RemainingRefillDelay);
		}
		for (const FAuraPersistedMerchantStock& Merchant : Save.Merchants)
		{
			Text += FString::Printf(TEXT("%s:%s:%u:%d:%s:%s;"), *Merchant.PopulationMemberId.ToString(),
				*Merchant.MerchantDefinitionId.ToString(), Merchant.StockRevision, Merchant.bAvailable ? 1 : 0,
				*Merchant.LastRestockAtUtc.ToIso8601(), *Merchant.LastObservedAtUtc.ToIso8601());
			for (const FAuraPersistedOfferStock& Offer : Merchant.Offers)
			{
				Text += FString::Printf(TEXT("%s:%lld;"), *Offer.OfferId.ToString(), Offer.CurrentStock);
			}
		}
		return Text;
	}

	bool IsSortedAndUnique(const TArray<FAuraPopulationSlotSnapshot>& Slots)
	{
		FName Previous = NAME_None;
		for (const FAuraPopulationSlotSnapshot& Slot : Slots)
		{
			if (Slot.PopulationMemberId.IsNone() || (!Previous.IsNone() && !Previous.LexicalLess(Slot.PopulationMemberId))) return false;
			Previous = Slot.PopulationMemberId;
		}
		return true;
	}

	bool IsSortedAndUnique(const TArray<FAuraPersistedMerchantStock>& Merchants)
	{
		FName Previous = NAME_None;
		for (const FAuraPersistedMerchantStock& Merchant : Merchants)
		{
			if (Merchant.PopulationMemberId.IsNone() || (!Previous.IsNone() && !Previous.LexicalLess(Merchant.PopulationMemberId))) return false;
			Previous = Merchant.PopulationMemberId;
		}
		return true;
	}
}

void UAuraPersistenceSubsystem::Deinitialize()
{
	if (bAuthorityPersistenceEnabled)
	{
		TArray<TWeakObjectPtr<AAuraPlayerState>> States;
		ActiveProfileKeys.GetKeys(States);
		for (const TWeakObjectPtr<AAuraPlayerState>& State : States)
		{
			if (AAuraPlayerState* PlayerState = State.Get())
			{
				FString Error;
				SavePlayerProfile(PlayerState, Cast<UAuraAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()),
					Cast<UAuraAttributeSet>(PlayerState->GetAttributeSet()), &Error);
			}
		}
	}
	PreparedProfiles.Reset();
	ActiveProfileKeys.Reset();
	ActiveProfiles.Reset();
	Super::Deinitialize();
}

void UAuraPersistenceSubsystem::ResetWorldRestoreState()
{
	LoadedWorldSave = nullptr;
	LoadedPopulationSnapshot.Reset();
	LoadedMerchantStocks.Reset();
	WorldManifestRecord = FAuraPersistenceManifestRecord();
	ActiveManifestSlot.Reset();
	ActiveManifestGeneration = 0;
	WorldGeneration = 0;
	bWorldRestoreAttempted = false;
	bWorldRestoreReady = false;
}

bool UAuraPersistenceSubsystem::ConfigureAuthorityWorld(const FString& MapName, FString& OutError, FName MapId)
{
	OutError.Reset();
	if (!GetGameInstance() || !GetGameInstance()->GetWorld() || GetGameInstance()->GetWorld()->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Persistence configuration requires an authority game-instance world.");
		return false;
	}
	if ((!CurrentWorldMapName.IsEmpty() && CurrentWorldMapName != MapName)
		|| (!CurrentWorldMapId.IsNone() && !MapId.IsNone() && CurrentWorldMapId != MapId))
	{
		ResetWorldRestoreState();
	}
	CurrentWorldMapName = MapName;
	CurrentWorldMapId = MapId;
	bAuthorityPersistenceEnabled = false;
	bPersistentLoginRequired = false;
	ExpectedProviderName.Reset();
	WorldPersistenceId.Reset();
	GConfig->GetString(TEXT("/Script/Aura.AuraPersistenceSettings"), TEXT("ExpectedProviderName"), ExpectedProviderName, GEngineIni);
	if (ExpectedProviderName.IsEmpty()) ExpectedProviderName = TEXT("Steam");
	FString CommandProvider;
#if !UE_BUILD_SHIPPING
	if (FParse::Value(FCommandLine::Get(), TEXT("AuraPersistenceProvider="), CommandProvider) && !CommandProvider.IsEmpty())
	{
		ExpectedProviderName = CommandProvider;
		UE_LOG(LogAura, Warning, TEXT("[Persistence][Identity] Development provider override='%s'; this path is not available in Shipping."), *ExpectedProviderName);
	}
#endif
	FParse::Value(FCommandLine::Get(), TEXT("WorldPersistenceId="), WorldPersistenceId);
	if (WorldPersistenceId.IsEmpty())
	{
		GConfig->GetString(TEXT("/Script/Aura.AuraPersistenceSettings"), TEXT("WorldPersistenceId"), WorldPersistenceId, GEngineIni);
	}
	if (!ValidateWorldPersistenceId(WorldPersistenceId, OutError)) return false;
	bAuthorityPersistenceEnabled = true;
	bPersistentLoginRequired = GetGameInstance()->GetWorld()->GetNetMode() != NM_Standalone;
	UE_LOG(LogAura, Display, TEXT("[Persistence][World] Configured WorldPersistenceId=%s provider=%s map=%s required=%d."),
		*WorldPersistenceId, *ExpectedProviderName, *MapName, bPersistentLoginRequired ? 1 : 0);
	return true;
}

bool UAuraPersistenceSubsystem::ValidateWorldPersistenceId(const FString& InWorldPersistenceId, FString& OutError)
{
	OutError.Reset();
	FString Value = InWorldPersistenceId;
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty() || Value != InWorldPersistenceId || Value.Len() > 96 || Value.Contains(TEXT("/")) || Value.Contains(TEXT("\\"))
		|| Value.Contains(TEXT("..")) || Value.Contains(TEXT("?")) || Value.Contains(TEXT("&")) || Value.Contains(TEXT("=")))
	{
		OutError = TEXT("WorldPersistenceId must be a non-empty deployment-owned safe identifier.");
		return false;
	}
	return true;
}

bool UAuraPersistenceSubsystem::ResolveConnectionIdentity(const FUniqueNetIdRepl& NetId, const FString& Options,
	FAuraPlayerProfileId& OutIdentity, FString& OutError) const
{
#if !UE_BUILD_SHIPPING
	if (bPersistentLoginRequired && FParse::Param(FCommandLine::Get(), TEXT("RoleBattleDay18PersistenceSmoke")))
	{
		// Fixed identities are reserved for the explicit persistence harness. A
		// development server must never let an ordinary connection select them.
		const FString Fixture = AuraPersistenceSubsystemPrivate::GetConnectionOption(Options, TEXT("AuraFixtureIdentity"));
		if (Fixture == TEXT("Day18ProfileA") || Fixture == TEXT("Day18ProfileB"))
		{
			FString FixtureProvider = AuraPersistenceSubsystemPrivate::GetConnectionOption(Options, TEXT("AuraFixtureProvider"));
			if (FixtureProvider.IsEmpty()) FixtureProvider = ExpectedProviderName;
			return FAuraPlayerProfileId::ValidateComponents(FixtureProvider, Fixture, ExpectedProviderName, true, OutIdentity, OutError);
		}
	}
#endif
	return FAuraPlayerProfileId::FromUniqueNetId(NetId, ExpectedProviderName,
#if !UE_BUILD_SHIPPING
		bPersistentLoginRequired,
#else
		false,
#endif
		OutIdentity, OutError);
}

bool UAuraPersistenceSubsystem::PreparePlayerProfile(APlayerController* NewPlayerController, const FUniqueNetIdRepl& NetId,
	const FString& Options, FName RequestedRole, FString& OutError)
{
	OutError.Reset();
	if (!bPersistentLoginRequired) return true;
	if (!NewPlayerController || !NewPlayerController->HasAuthority()) { OutError = TEXT("Profile preparation requires an authority controller."); return false; }
	FAuraPlayerProfileId Identity;
	if (!ResolveConnectionIdentity(NetId, Options, Identity, OutError)) return false;
	const FString Key = Identity.ToCanonicalString();
	if (ActiveProfiles.Contains(Key))
	{
		OutError = TEXT("A connection for this authenticated profile is already active.");
		return false;
	}
	UAuraPlayerSaveGame* Save = nullptr;
	if (!LoadOrCreatePlayerSave(Identity, RequestedRole, Save, OutError)) return false;
	AAuraPlayerState* PlayerState = NewPlayerController->GetPlayerState<AAuraPlayerState>();
	if (!PlayerState) { OutError = TEXT("PlayerState was not available before profile preparation."); return false; }
	PlayerState->SetProfileIdentity(Identity);
	PlayerState->SetPendingPersistentProfile(Save);
	PreparedProfiles.Add(Key, Save);
	ActiveProfileKeys.Add(PlayerState, Key);
	ActiveProfiles.Add(Key);
	UE_LOG(LogAura, Display, TEXT("[Persistence][Server] ProfilePrepared=1 provider=%s identityHash=%s existing=%d LoadBeforePawnInitialization=1."),
		*Identity.ProviderName, *AuraPersistenceSubsystemPrivate::HashText(Key), Save->bFirstTimeLoadIn ? 0 : 1);
	return true;
}

bool UAuraPersistenceSubsystem::ApplyPreparedProfile(AAuraPlayerState* PlayerState, FString& OutError) const
{
	OutError.Reset();
	if (!PlayerState || !PlayerState->HasAuthority()) { OutError = TEXT("Persistent profile application requires authority PlayerState."); return false; }
	const TObjectPtr<UAuraPlayerSaveGame>* Save = PreparedProfiles.Find(PlayerState->GetProfileIdentity().ToCanonicalString());
	if (!Save || !Save->Get())
	{
		OutError = TEXT("No prepared persistent profile exists for this authenticated identity.");
		return false;
	}
	return PlayerState->ApplyPersistentProfile(*Save->Get(), OutError);
}

bool UAuraPersistenceSubsystem::LoadOrCreatePlayerSave(const FAuraPlayerProfileId& Identity, FName RequestedRole,
	UAuraPlayerSaveGame*& OutSave, FString& OutError)
{
	OutSave = nullptr;
	OutError.Reset();
	const FString IdentityKey = Identity.ToCanonicalString();
	const FString SlotBase = Identity.BuildSlotName(WorldPersistenceId);
	bool bFoundNewRecord = false;
	const auto TryLoadRecord = [this, &Identity, &OutSave, &OutError](const FString& Slot,
		int32 ExpectedGeneration, const FString& ExpectedChecksum) -> bool
	{
		UAuraPlayerSaveGame* Candidate = Cast<UAuraPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Candidate || (ExpectedGeneration > 0 && Candidate->RecordGeneration != ExpectedGeneration)
			|| (!ExpectedChecksum.IsEmpty() && ComputePlayerSaveChecksum(*Candidate) != ExpectedChecksum)) return false;
		FString CandidateError;
		if (!ValidatePlayerSaveRecord(*Candidate, Identity, ExpectedProviderName, CandidateError)) return false;
		OutSave = Candidate;
		OutError.Reset();
		return true;
	};
	if (const FAuraPersistenceManifestRecord* ManifestRecord = PlayerManifestRecords.Find(IdentityKey))
	{
		bFoundNewRecord = true;
		if (!ManifestRecord->RecordSlot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(ManifestRecord->RecordSlot, 0)
			&& TryLoadRecord(ManifestRecord->RecordSlot, ManifestRecord->Generation, ManifestRecord->Checksum))
		{
			PlayerGenerations.Add(IdentityKey, OutSave->RecordGeneration);
			return true;
		}
		UE_LOG(LogAura, Warning, TEXT("[Persistence][Player] Manifest record for identityHash=%s was invalid; searching prior verified generations."),
			*AuraPersistenceSubsystemPrivate::HashText(IdentityKey));
	}

	TArray<int32> CandidateGenerations;
	for (int32 CandidateGeneration = 1; CandidateGeneration <= 4096; ++CandidateGeneration)
	{
		const FString CandidateSlot = FString::Printf(TEXT("%s_G%d"), *SlotBase, CandidateGeneration);
		if (UGameplayStatics::DoesSaveGameExist(CandidateSlot, 0))
		{
			bFoundNewRecord = true;
			CandidateGenerations.Add(CandidateGeneration);
		}
	}
	CandidateGenerations.Sort([](const int32 A, const int32 B) { return A > B; });
	for (const int32 CandidateGeneration : CandidateGenerations)
	{
		const FString Slot = FString::Printf(TEXT("%s_G%d"), *SlotBase, CandidateGeneration);
		if (TryLoadRecord(Slot, 0, FString()))
		{
			PlayerGenerations.Add(IdentityKey, OutSave->RecordGeneration);
			return true;
		}
		UE_LOG(LogAura, Warning, TEXT("[Persistence][Player] Ignoring invalid player record slot=%s while selecting a verified generation."), *Slot);
	}
	if (bFoundNewRecord)
	{
		OutError = TEXT("All available player generations were invalid; refusing legacy fallback or partial profile initialization.");
		return false;
	}

	if (UGameplayStatics::DoesSaveGameExist(GetLegacySlotName(Identity), 0))
	{
		ULoadScreenSaveGame* Legacy = Cast<ULoadScreenSaveGame>(UGameplayStatics::LoadGameFromSlot(GetLegacySlotName(Identity), 0));
		UAuraPlayerSaveGame* Migrated = Cast<UAuraPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UAuraPlayerSaveGame::StaticClass()));
		if (!Legacy || !Migrated || !MigrateLegacySave(*Legacy, Identity, RequestedRole, *Migrated, OutError))
		{
			OutError = OutError.IsEmpty() ? TEXT("Legacy profile migration failed.") : OutError;
			return false;
		}
		OutSave = Migrated;
		if (const UAuraEconomyRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UAuraEconomyRegistrySubsystem>();
			Registry && Registry->IsReady())
		{
			OutSave->CurrencyId = Registry->GetSnapshot()->Settings.CurrencyId;
			OutSave->CurrencyBalance = Registry->GetSnapshot()->Settings.StartingBalance;
			OutSave->CurrencyRevision = 1;
		}
		else
		{
			OutError = TEXT("Legacy migration requires the authoritative economy registry before applying starting balance.");
			OutSave = nullptr;
			return false;
		}
		return true;
	}
	OutSave = Cast<UAuraPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UAuraPlayerSaveGame::StaticClass()));
	if (!OutSave) { OutError = TEXT("Could not create a player save object."); return false; }
	OutSave->SaveSchemaVersion = UAuraPlayerSaveGame::CurrentSchemaVersion;
	OutSave->RecordGeneration = 0;
	OutSave->IdentityProvider = Identity.ProviderName;
	OutSave->IdentityValue = Identity.UniqueId;
	OutSave->Role = RequestedRole;
	OutSave->bFirstTimeLoadIn = true;
	OutSave->bMigrationCompleted = false;
	return true;
}

bool UAuraPersistenceSubsystem::SavePlayerProfile(AAuraPlayerState* PlayerState, const UAuraAbilitySystemComponent* AbilitySystemComponent,
	const UAuraAttributeSet* AttributeSet, FString* OutError)
{
	FString LocalError;
	FString& Error = OutError ? *OutError : LocalError;
	Error.Reset();
	const FString* Key = PlayerState ? ActiveProfileKeys.Find(PlayerState) : nullptr;
	const FString ProfileKey = Key ? *Key : FString();
	const bool bHadGeneration = !ProfileKey.IsEmpty() && PlayerGenerations.Contains(ProfileKey);
	const int32 PreviousGeneration = bHadGeneration ? PlayerGenerations.FindRef(ProfileKey) : 0;
	const bool bHadManifestRecord = !ProfileKey.IsEmpty() && PlayerManifestRecords.Contains(ProfileKey);
	const FAuraPersistenceManifestRecord PreviousManifestRecord = bHadManifestRecord
		? PlayerManifestRecords.FindRef(ProfileKey) : FAuraPersistenceManifestRecord();
	const TObjectPtr<UAuraPlayerSaveGame> PreviousPreparedProfile = !ProfileKey.IsEmpty()
		? PreparedProfiles.FindRef(ProfileKey) : nullptr;
	if (!SavePlayerProfileRecord(PlayerState, AbilitySystemComponent, AttributeSet, Error)) return false;
	if (WriteManifest(PlayerGenerations.FindRef(ProfileKey), Error)) return true;
	const FString UncommittedSlot = PlayerManifestRecords.FindRef(ProfileKey).RecordSlot;
	if (!UncommittedSlot.IsEmpty() && (!bHadManifestRecord || UncommittedSlot != PreviousManifestRecord.RecordSlot))
	{
		UGameplayStatics::DeleteGameInSlot(UncommittedSlot, 0);
	}
	if (bHadGeneration) PlayerGenerations.Add(ProfileKey, PreviousGeneration); else PlayerGenerations.Remove(ProfileKey);
	if (bHadManifestRecord) PlayerManifestRecords.Add(ProfileKey, PreviousManifestRecord); else PlayerManifestRecords.Remove(ProfileKey);
	if (PreviousPreparedProfile) PreparedProfiles.Add(ProfileKey, PreviousPreparedProfile); else PreparedProfiles.Remove(ProfileKey);
	return false;
}

bool UAuraPersistenceSubsystem::SavePlayerProfileRecord(AAuraPlayerState* PlayerState,
	const UAuraAbilitySystemComponent* AbilitySystemComponent, const UAuraAttributeSet* AttributeSet, FString& Error)
{
	Error.Reset();
	if (!bPersistentLoginRequired || !PlayerState || !PlayerState->HasAuthority()) { Error = TEXT("Persistent player save requires an active authority profile."); return false; }
	const FString* Key = ActiveProfileKeys.Find(PlayerState);
	const TObjectPtr<UAuraPlayerSaveGame>* Existing = Key ? PreparedProfiles.Find(*Key) : nullptr;
	if (!Key || !Existing || !Existing->Get()) { Error = TEXT("No prepared authenticated profile is bound to PlayerState."); return false; }
	UAuraPlayerSaveGame* Save = DuplicateObject<UAuraPlayerSaveGame>(Existing->Get(), this);
	if (!Save) { Error = TEXT("Could not create an isolated player save record."); return false; }
	Save->SaveSchemaVersion = UAuraPlayerSaveGame::CurrentSchemaVersion;
	Save->RecordGeneration = FMath::Max(PlayerGenerations.FindRef(*Key), Save->RecordGeneration) + 1;
	Save->IdentityProvider = PlayerState->GetProfileIdentity().ProviderName;
	Save->IdentityValue = PlayerState->GetProfileIdentity().UniqueId;
	Save->Role = PlayerState->GetRole();
	Save->PlayerLevel = PlayerState->GetPlayerLevel();
	Save->XP = PlayerState->GetXP();
	Save->SpellPoints = PlayerState->GetSpellPoints();
	Save->AttributePoints = PlayerState->GetAttributePoints();
	Save->bFirstTimeLoadIn = false;
	Save->bMigrationCompleted = true;
	const FAuraFirearmState& Firearm = PlayerState->GetFirearmState();
	Save->bFirearmApplicable = Firearm.bApplicable;
	Save->FirearmMagazineCapacity = Firearm.MagazineCapacity;
	Save->FirearmMagazineRounds = Firearm.MagazineRounds;
	Save->FirearmReserveCapacity = Firearm.ReserveCapacity;
	Save->FirearmReserveRounds = Firearm.ReserveRounds;
	Save->FirearmReloadDuration = Firearm.ReloadDuration;
	Save->FirearmAmmoRevision = Firearm.AmmoRevision;
	Save->TutorialCompletionMask = PlayerState->GetTutorialCompletionMask();
	Save->RecoveryState = PlayerState->GetRecoveryState();
	if (AttributeSet)
	{
		Save->Strength = UAuraAttributeSet::GetStrengthAttribute().GetNumericValue(AttributeSet);
		Save->Intelligence = UAuraAttributeSet::GetIntelligenceAttribute().GetNumericValue(AttributeSet);
		Save->Resilience = UAuraAttributeSet::GetResilienceAttribute().GetNumericValue(AttributeSet);
		Save->Vigor = UAuraAttributeSet::GetVigorAttribute().GetNumericValue(AttributeSet);
	}
	if (!PlayerState->GetCurrencyComponent() || !PlayerState->GetInventoryComponent()) { Error = TEXT("Player economy components are unavailable."); return false; }
	Save->CurrencyId = PlayerState->GetCurrencyComponent()->GetCurrencyId();
	Save->CurrencyBalance = PlayerState->GetCurrencyComponent()->GetBalance();
	Save->CurrencyRevision = PlayerState->GetCurrencyComponent()->GetRevision();
	Save->InventorySlots = PlayerState->GetInventoryComponent()->GetSlots();
	Save->InventoryRevision = PlayerState->GetInventoryComponent()->GetRevision();
	CaptureAbilityState(*Save, AbilitySystemComponent);
	if (!ValidatePlayerSaveRecord(*Save, PlayerState->GetProfileIdentity(), ExpectedProviderName, Error)) return false;
	const FString Slot = FString::Printf(TEXT("%s_G%d"), *PlayerState->GetProfileIdentity().BuildSlotName(WorldPersistenceId), Save->RecordGeneration);
	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0)) { Error = TEXT("Player record write failed."); return false; }
	UAuraPlayerSaveGame* Verify = Cast<UAuraPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Verify || !ValidatePlayerSaveRecord(*Verify, PlayerState->GetProfileIdentity(), ExpectedProviderName, Error))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		return false;
	}
	PlayerGenerations.Add(*Key, Save->RecordGeneration);
	FAuraPersistenceManifestRecord ManifestRecord;
	ManifestRecord.RecordSlot = Slot;
	ManifestRecord.Generation = Save->RecordGeneration;
	ManifestRecord.Checksum = ComputePlayerSaveChecksum(*Save);
	ManifestRecord.IdentityKey = *Key;
	PlayerManifestRecords.Add(*Key, ManifestRecord);
	PreparedProfiles.Add(*Key, Verify);
	return true;
}

bool UAuraPersistenceSubsystem::SavePlayerAndWorldCheckpoint(AAuraPlayerState* PlayerState,
	const UAuraAbilitySystemComponent* AbilitySystemComponent, const UAuraAttributeSet* AttributeSet,
	const FAuraPopulationDebugSnapshot& PopulationSnapshot,
	const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError)
{
	OutError.Reset();
	const TMap<FString, int32> PreviousPlayerGenerations = PlayerGenerations;
	const TMap<FString, FAuraPersistenceManifestRecord> PreviousPlayerManifestRecords = PlayerManifestRecords;
	const TMap<FString, TObjectPtr<UAuraPlayerSaveGame>> PreviousPreparedProfiles = PreparedProfiles;
	const FAuraPersistenceManifestRecord PreviousWorldManifestRecord = WorldManifestRecord;
	const int32 PreviousWorldGeneration = WorldGeneration;
	const TObjectPtr<UAuraWorldSaveGame> PreviousLoadedWorldSave = LoadedWorldSave;
	const TArray<FAuraPopulationSlotSnapshot> PreviousLoadedPopulationSnapshot = LoadedPopulationSnapshot;
	const TArray<FAuraPersistedMerchantStock> PreviousLoadedMerchantStocks = LoadedMerchantStocks;
	FString UncommittedPlayerSlot;
	FString UncommittedWorldSlot;
	const auto RestoreCheckpointState = [this, &PreviousPlayerGenerations, &PreviousPlayerManifestRecords,
		&PreviousPreparedProfiles, &UncommittedPlayerSlot, &UncommittedWorldSlot,
		&PreviousWorldManifestRecord, PreviousWorldGeneration, PreviousLoadedWorldSave,
		&PreviousLoadedPopulationSnapshot, &PreviousLoadedMerchantStocks]()
	{
		if (!UncommittedPlayerSlot.IsEmpty()) UGameplayStatics::DeleteGameInSlot(UncommittedPlayerSlot, 0);
		if (!UncommittedWorldSlot.IsEmpty()) UGameplayStatics::DeleteGameInSlot(UncommittedWorldSlot, 0);
		PlayerGenerations = PreviousPlayerGenerations;
		PlayerManifestRecords = PreviousPlayerManifestRecords;
		PreparedProfiles = PreviousPreparedProfiles;
		WorldManifestRecord = PreviousWorldManifestRecord;
		WorldGeneration = PreviousWorldGeneration;
		LoadedWorldSave = PreviousLoadedWorldSave;
		LoadedPopulationSnapshot = PreviousLoadedPopulationSnapshot;
		LoadedMerchantStocks = PreviousLoadedMerchantStocks;
	};

	if (!SavePlayerProfileRecord(PlayerState, AbilitySystemComponent, AttributeSet, OutError)) return false;
	if (const FString* ProfileKey = PlayerState ? ActiveProfileKeys.Find(PlayerState) : nullptr)
	{
		const FAuraPersistenceManifestRecord& NewRecord = PlayerManifestRecords.FindRef(*ProfileKey);
		const FAuraPersistenceManifestRecord* PreviousRecord = PreviousPlayerManifestRecords.Find(*ProfileKey);
		if (!PreviousRecord || NewRecord.RecordSlot != PreviousRecord->RecordSlot) UncommittedPlayerSlot = NewRecord.RecordSlot;
	}
	if (!SaveWorldStateRecord(PopulationSnapshot, MerchantStocks, OutError))
	{
		RestoreCheckpointState();
		return false;
	}
	if (WorldManifestRecord.RecordSlot != PreviousWorldManifestRecord.RecordSlot)
	{
		UncommittedWorldSlot = WorldManifestRecord.RecordSlot;
	}
	if (!WriteManifest(WorldGeneration, OutError))
	{
		RestoreCheckpointState();
		return false;
	}
	return true;
}

void UAuraPersistenceSubsystem::ReleasePlayerProfile(AAuraPlayerState* PlayerState)
{
	if (!PlayerState) return;
	if (const FString* Key = ActiveProfileKeys.Find(PlayerState))
	{
		ActiveProfiles.Remove(*Key);
		PreparedProfiles.Remove(*Key);
	}
	ActiveProfileKeys.Remove(PlayerState);
}

void UAuraPersistenceSubsystem::CaptureAbilityState(UAuraPlayerSaveGame& Save, const UAuraAbilitySystemComponent* AbilitySystemComponent) const
{
	Save.SavedAbilities.Reset();
	if (!AbilitySystemComponent) return;
	FForEachAbility Delegate;
	Delegate.BindLambda([&Save, AbilitySystemComponent](const FGameplayAbilitySpec& AbilitySpec)
		{
			FSavedAbility Ability;
			Ability.AbilityTag = AbilitySystemComponent->GetAbilityTagFromSpec(AbilitySpec);
			Ability.AbilityLevel = AbilitySpec.Level;
			Ability.AbilitySlot = const_cast<UAuraAbilitySystemComponent*>(AbilitySystemComponent)->GetSlotFromAbilityTag(Ability.AbilityTag);
			Ability.AbilityStatus = const_cast<UAuraAbilitySystemComponent*>(AbilitySystemComponent)->GetStatusFromAbilityTag(Ability.AbilityTag);
			FName RoleId;
			Ability.GrantSource = static_cast<uint8>(AbilitySystemComponent->GetGrantSourceForSpec(AbilitySpec, RoleId));
			Ability.GrantedRoleId = RoleId;
			Ability.ProvenanceVersion = 1;
			Save.SavedAbilities.AddUnique(Ability);
		});
	const_cast<UAuraAbilitySystemComponent*>(AbilitySystemComponent)->ForEachAbility(Delegate);
}

bool UAuraPersistenceSubsystem::LoadWorldState(const FString& MapName, FString& OutError, FName ExpectedMapId)
{
	OutError.Reset();
	if (!bAuthorityPersistenceEnabled)
	{
		OutError = TEXT("Persistence subsystem has not been configured.");
		return false;
	}
	const FString EffectiveMapName = MapName.IsEmpty() ? CurrentWorldMapName : MapName;
	if (bWorldRestoreAttempted && ((!CurrentWorldMapName.IsEmpty() && CurrentWorldMapName != EffectiveMapName)
		|| (!ExpectedMapId.IsNone() && !CurrentWorldMapId.IsNone() && CurrentWorldMapId != ExpectedMapId)))
	{
		ResetWorldRestoreState();
	}
	CurrentWorldMapName = EffectiveMapName;
	if (!ExpectedMapId.IsNone()) CurrentWorldMapId = ExpectedMapId;
	if (bWorldRestoreAttempted) return bWorldRestoreReady;
	bWorldRestoreAttempted = true;
	struct FManifestCandidate
	{
		FString Slot;
		TObjectPtr<UAuraPersistenceManifestSaveGame> Manifest;
	};
	TArray<FManifestCandidate> Candidates;
	for (const FString& ManifestSlot : { BuildManifestSlot(true), BuildManifestSlot(false) })
	{
		UAuraPersistenceManifestSaveGame* Candidate = nullptr;
		if (!LoadManifestCandidate(ManifestSlot, Candidate) || !Candidate) continue;
		FString CandidateError;
		if (!ValidateManifest(*Candidate, CandidateError)) continue;
		Candidates.Add({ ManifestSlot, Candidate });
	}
	Candidates.Sort([](const FManifestCandidate& A, const FManifestCandidate& B)
	{
		return A.Manifest->Generation > B.Manifest->Generation;
	});
	UAuraPersistenceManifestSaveGame* SelectedManifest = nullptr;
	FString SelectedManifestSlot;
	UAuraWorldSaveGame* SelectedWorldSave = nullptr;
	UAuraPersistenceManifestSaveGame* FallbackManifest = nullptr;
	FString FallbackManifestSlot;
	bool bSawInvalidWorldRecord = false;
	for (const FManifestCandidate& Candidate : Candidates)
	{
		if (Candidate.Manifest->WorldRecord.RecordSlot.IsEmpty())
		{
			SelectedManifest = Candidate.Manifest;
			SelectedManifestSlot = Candidate.Slot;
			break;
		}
		UAuraWorldSaveGame* WorldSave = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Candidate.Manifest->WorldRecord.RecordSlot, 0));
		FString WorldError;
		const bool bMapMatches = ExpectedMapId.IsNone() || !WorldSave
			|| WorldSave->MapPackage.Equals(ExpectedMapId.ToString(), ESearchCase::IgnoreCase);
		if (WorldSave && ComputeWorldSaveChecksum(*WorldSave) == Candidate.Manifest->WorldRecord.Checksum && !bMapMatches)
		{
			if (!FallbackManifest)
			{
				FallbackManifest = Candidate.Manifest;
				FallbackManifestSlot = Candidate.Slot;
			}
			UE_LOG(LogAura, Display, TEXT("[Persistence][World] Ignoring manifest slot=%s because its record belongs to map=%s, current map=%s."),
				*Candidate.Slot, *WorldSave->MapPackage, *ExpectedMapId.ToString());
			continue;
		}
		if (WorldSave && ComputeWorldSaveChecksum(*WorldSave) == Candidate.Manifest->WorldRecord.Checksum
			&& ValidateWorldSaveRecord(*WorldSave, WorldPersistenceId, WorldError, ExpectedMapId))
		{
			SelectedManifest = Candidate.Manifest;
			SelectedManifestSlot = Candidate.Slot;
			SelectedWorldSave = WorldSave;
			break;
		}
		bSawInvalidWorldRecord = true;
		UE_LOG(LogAura, Warning, TEXT("[Persistence][World] Ignoring manifest slot=%s because its referenced world record is torn, corrupt, or invalid; trying the prior manifest."),
			*Candidate.Slot);
	}
	if (!SelectedManifest && FallbackManifest)
	{
		SelectedManifest = FallbackManifest;
		SelectedManifestSlot = FallbackManifestSlot;
	}
	if (!SelectedManifest && Candidates.Num() > 0 && bSawInvalidWorldRecord)
	{
		OutError = TEXT("No valid manifest referenced a valid world record; refusing to publish partial world state.");
		return false;
	}
	if (!SelectedManifest || !SelectedWorldSave)
	{
		WorldGeneration = SelectedManifest ? SelectedManifest->Generation : 0;
		ActiveManifestGeneration = WorldGeneration;
		ActiveManifestSlot = SelectedManifestSlot;
		WorldManifestRecord = FAuraPersistenceManifestRecord();
		PlayerManifestRecords.Reset();
		if (SelectedManifest)
		{
			for (const FAuraPersistenceManifestRecord& Record : SelectedManifest->PlayerRecords)
			{
				if (!Record.IdentityKey.IsEmpty()) PlayerManifestRecords.Add(Record.IdentityKey, Record);
			}
		}
		bWorldRestoreReady = true;
		UE_LOG(LogAura, Display, TEXT("[Persistence][World] LoadedOnce=1 generation=%d map=%s mapId=%s record=none defaultState=1."),
			WorldGeneration, *EffectiveMapName, *CurrentWorldMapId.ToString());
		return true;
	}
	LoadedWorldSave = SelectedWorldSave;
	LoadedPopulationSnapshot = SelectedWorldSave->PopulationSlots;
	LoadedMerchantStocks = SelectedWorldSave->Merchants;
	WorldGeneration = SelectedWorldSave->RecordGeneration;
	ActiveManifestGeneration = SelectedManifest->Generation;
	ActiveManifestSlot = SelectedManifestSlot;
	WorldManifestRecord = SelectedManifest->WorldRecord;
	PlayerManifestRecords.Reset();
	for (const FAuraPersistenceManifestRecord& Record : SelectedManifest->PlayerRecords)
	{
		if (!Record.IdentityKey.IsEmpty()) PlayerManifestRecords.Add(Record.IdentityKey, Record);
	}
	bWorldRestoreReady = true;
	UE_LOG(LogAura, Display, TEXT("[Persistence][World] LoadedOnce=1 generation=%d map=%s mapId=%s population=%d merchants=%d."),
		WorldGeneration, *EffectiveMapName, *CurrentWorldMapId.ToString(), LoadedPopulationSnapshot.Num(), LoadedMerchantStocks.Num());
	return true;
}

bool UAuraPersistenceSubsystem::SaveWorldState(const FAuraPopulationDebugSnapshot& PopulationSnapshot,
	const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError)
{
	OutError.Reset();
	const FAuraPersistenceManifestRecord PreviousWorldManifestRecord = WorldManifestRecord;
	const int32 PreviousWorldGeneration = WorldGeneration;
	const TObjectPtr<UAuraWorldSaveGame> PreviousLoadedWorldSave = LoadedWorldSave;
	const TArray<FAuraPopulationSlotSnapshot> PreviousLoadedPopulationSnapshot = LoadedPopulationSnapshot;
	const TArray<FAuraPersistedMerchantStock> PreviousLoadedMerchantStocks = LoadedMerchantStocks;
	if (!SaveWorldStateRecord(PopulationSnapshot, MerchantStocks, OutError)) return false;
	if (WriteManifest(WorldGeneration, OutError)) return true;
	if (!WorldManifestRecord.RecordSlot.IsEmpty() && WorldManifestRecord.RecordSlot != PreviousWorldManifestRecord.RecordSlot)
	{
		UGameplayStatics::DeleteGameInSlot(WorldManifestRecord.RecordSlot, 0);
	}
	WorldManifestRecord = PreviousWorldManifestRecord;
	WorldGeneration = PreviousWorldGeneration;
	LoadedWorldSave = PreviousLoadedWorldSave;
	LoadedPopulationSnapshot = PreviousLoadedPopulationSnapshot;
	LoadedMerchantStocks = PreviousLoadedMerchantStocks;
	return false;
}

bool UAuraPersistenceSubsystem::SaveWorldStateRecord(const FAuraPopulationDebugSnapshot& PopulationSnapshot,
	const TArray<FAuraPersistedMerchantStock>& MerchantStocks, FString& OutError)
{
	OutError.Reset();
	if (!bAuthorityPersistenceEnabled || !bWorldRestoreReady) { OutError = TEXT("World save requires a loaded authority persistence state."); return false; }
	UAuraWorldSaveGame* Save = Cast<UAuraWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UAuraWorldSaveGame::StaticClass()));
	if (!Save) { OutError = TEXT("Could not create a world save object."); return false; }
	Save->SaveSchemaVersion = UAuraWorldSaveGame::CurrentSchemaVersion;
	Save->RecordGeneration = FMath::Max(WorldGeneration, ActiveManifestGeneration) + 1;
	Save->WorldPersistenceId = WorldPersistenceId;
	Save->MapPackage = PopulationSnapshot.MapId.ToString();
	if (!CurrentWorldMapId.IsNone() && PopulationSnapshot.MapId != CurrentWorldMapId)
	{
		OutError = FString::Printf(TEXT("World snapshot map '%s' does not match the current map '%s'."),
			*PopulationSnapshot.MapId.ToString(), *CurrentWorldMapId.ToString());
		return false;
	}
	Save->PopulationSlots = PopulationSnapshot.Slots;
	Save->PopulationSlots.Sort([](const FAuraPopulationSlotSnapshot& A, const FAuraPopulationSlotSnapshot& B) { return A.PopulationMemberId.LexicalLess(B.PopulationMemberId); });
	Save->Merchants = MerchantStocks;
	Save->Merchants.Sort([](const FAuraPersistedMerchantStock& A, const FAuraPersistedMerchantStock& B) { return A.PopulationMemberId.LexicalLess(B.PopulationMemberId); });
	if (!AuraPersistenceSubsystemPrivate::IsSortedAndUnique(Save->PopulationSlots) || !AuraPersistenceSubsystemPrivate::IsSortedAndUnique(Save->Merchants)
		|| !ValidateWorldSaveRecord(*Save, WorldPersistenceId, OutError, CurrentWorldMapId)) return false;
	const FString Slot = BuildWorldRecordSlot(Save->RecordGeneration);
	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0)) { OutError = TEXT("World record write failed."); return false; }
	UAuraWorldSaveGame* Verify = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Verify || ComputeWorldSaveChecksum(*Verify) != ComputeWorldSaveChecksum(*Save)) { OutError = TEXT("World record verification failed."); return false; }
	WorldGeneration = Save->RecordGeneration;
	WorldManifestRecord = { Slot, Save->RecordGeneration, ComputeWorldSaveChecksum(*Save) };
	LoadedWorldSave = Save;
	LoadedPopulationSnapshot = Save->PopulationSlots;
	LoadedMerchantStocks = Save->Merchants;
	return true;
}

bool UAuraPersistenceSubsystem::LoadManifestCandidate(const FString& SlotName, UAuraPersistenceManifestSaveGame*& OutManifest) const
{
	OutManifest = nullptr;
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	OutManifest = Cast<UAuraPersistenceManifestSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	return OutManifest != nullptr;
}

bool UAuraPersistenceSubsystem::ValidateManifest(const UAuraPersistenceManifestSaveGame& Manifest, FString& OutError) const
{
	OutError.Reset();
	if (Manifest.SaveSchemaVersion != UAuraPersistenceManifestSaveGame::CurrentSchemaVersion || Manifest.Generation <= 0
		|| Manifest.WorldPersistenceId != WorldPersistenceId || Manifest.Checksum.IsEmpty()
		|| ComputeManifestChecksum(Manifest) != Manifest.Checksum)
	{
		OutError = TEXT("Manifest schema, world identity, generation, or checksum is invalid.");
		return false;
	}
	for (const FAuraPersistenceManifestRecord& Record : Manifest.PlayerRecords)
	{
		if (Record.IdentityKey.IsEmpty() || !Record.IdentityKey.Contains(TEXT(":"))
			|| Record.RecordSlot.IsEmpty() || Record.Generation <= 0 || Record.Checksum.IsEmpty())
		{
			OutError = TEXT("Manifest contains an invalid player record reference.");
			return false;
		}
	}
	return true;
}

bool UAuraPersistenceSubsystem::WriteManifest(int32 Generation, FString& OutError)
{
	UAuraPersistenceManifestSaveGame* Manifest = Cast<UAuraPersistenceManifestSaveGame>(UGameplayStatics::CreateSaveGameObject(UAuraPersistenceManifestSaveGame::StaticClass()));
	if (!Manifest) { OutError = TEXT("Could not create manifest object."); return false; }
	Manifest->SaveSchemaVersion = UAuraPersistenceManifestSaveGame::CurrentSchemaVersion;
	Manifest->Generation = FMath::Max(Generation, ActiveManifestGeneration) + 1;
	Manifest->WorldPersistenceId = WorldPersistenceId;
	Manifest->WorldRecord = WorldManifestRecord;
	PlayerManifestRecords.GenerateValueArray(Manifest->PlayerRecords);
	Manifest->PlayerRecords.Sort([](const FAuraPersistenceManifestRecord& A, const FAuraPersistenceManifestRecord& B) { return A.IdentityKey < B.IdentityKey; });
	Manifest->Checksum = ComputeManifestChecksum(*Manifest);
	const FString Slot = ActiveManifestSlot == BuildManifestSlot(true) ? BuildManifestSlot(false) : BuildManifestSlot(true);
	if (!UGameplayStatics::SaveGameToSlot(Manifest, Slot, 0)) { OutError = TEXT("Manifest write failed after durable record verification."); return false; }
	UAuraPersistenceManifestSaveGame* Verify = Cast<UAuraPersistenceManifestSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Verify || Verify->Checksum != ComputeManifestChecksum(*Verify)) { OutError = TEXT("Manifest verification failed; prior manifest remains authoritative."); return false; }
	ActiveManifestGeneration = Manifest->Generation;
	ActiveManifestSlot = Slot;
	return true;
}

FString UAuraPersistenceSubsystem::BuildWorldRecordSlot(int32 Generation) const
{
	return FString::Printf(TEXT("AuraWorld_%08X_G%d"), FCrc::StrCrc32(*WorldPersistenceId), Generation);
}

FString UAuraPersistenceSubsystem::BuildManifestSlot(bool bUseA) const
{
	return FString::Printf(TEXT("AuraManifest_%08X_%s"), FCrc::StrCrc32(*WorldPersistenceId), bUseA ? TEXT("A") : TEXT("B"));
}

FString UAuraPersistenceSubsystem::GetLegacySlotName(const FAuraPlayerProfileId& Identity) const
{
	return FString::Printf(TEXT("AuraLegacy_%s"), *Identity.BuildSlotName(WorldPersistenceId));
}

bool UAuraPersistenceSubsystem::ValidatePlayerSaveRecord(const UAuraPlayerSaveGame& Save, const FAuraPlayerProfileId& ExpectedIdentity,
	const FString& ExpectedProviderName, FString& OutError)
{
	OutError.Reset();
	FAuraPlayerProfileId Actual;
	if (Save.SaveSchemaVersion > UAuraPlayerSaveGame::CurrentSchemaVersion || Save.RecordGeneration < 0
		|| !FAuraPlayerProfileId::ValidateComponents(Save.IdentityProvider, Save.IdentityValue, ExpectedProviderName, true, Actual, OutError)
		|| Actual.ToCanonicalString() != ExpectedIdentity.ToCanonicalString()) return false;
	if (Save.Role.IsNone()) { OutError = TEXT("Player save has no committed role."); return false; }
	if (Save.CurrencyBalance < 0 || Save.CurrencyId.IsNone()) { OutError = TEXT("Player save has invalid currency state."); return false; }
	if (Save.FirearmMagazineCapacity < 0 || Save.FirearmReserveCapacity < 0 || Save.FirearmMagazineRounds < 0
		|| Save.FirearmReserveRounds < 0 || Save.FirearmMagazineRounds > Save.FirearmMagazineCapacity
		|| Save.FirearmReserveRounds > Save.FirearmReserveCapacity || Save.FirearmReloadDuration < 0.f)
	{
		OutError = TEXT("Player save contains invalid completed firearm state.");
		return false;
	}
	if (!Save.RecoveryState.IsNone() && Save.RecoveryState != TEXT("Alive") && Save.RecoveryState != TEXT("Dying")
		&& Save.RecoveryState != TEXT("Dead") && Save.RecoveryState != TEXT("Recovering"))
	{
		OutError = TEXT("Player save contains an invalid recovery state.");
		return false;
	}
	for (const FAuraInventorySlot& Slot : Save.InventorySlots)
	{
		if (Slot.ItemId.IsNone() || Slot.Quantity <= 0) { OutError = TEXT("Player save contains an invalid inventory slot."); return false; }
	}
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	TSet<FGameplayTag> SeenAbilityTags;
	for (const FSavedAbility& Ability : Save.SavedAbilities)
	{
		if (!Ability.AbilityTag.IsValid() || SeenAbilityTags.Contains(Ability.AbilityTag)
			|| Ability.AbilityLevel < 1 || Ability.GrantSource > static_cast<uint8>(EAuraAbilityGrantSource::Progression)
			|| Ability.ProvenanceVersion < 1 || Ability.ProvenanceVersion > 1)
		{
			OutError = TEXT("Player save contains an invalid or duplicate ability record.");
			return false;
		}
		SeenAbilityTags.Add(Ability.AbilityTag);
		const bool bValidStatus = !Ability.AbilityStatus.IsValid()
			|| Ability.AbilityStatus == GameplayTags.Abilities_Status_Locked
			|| Ability.AbilityStatus == GameplayTags.Abilities_Status_Eligible
			|| Ability.AbilityStatus == GameplayTags.Abilities_Status_Unlocked
			|| Ability.AbilityStatus == GameplayTags.Abilities_Status_Equipped;
		const bool bValidSlot = !Ability.AbilitySlot.IsValid()
			|| Ability.AbilitySlot == GameplayTags.InputTag_LMB
			|| Ability.AbilitySlot == GameplayTags.InputTag_RMB
			|| Ability.AbilitySlot == GameplayTags.InputTag_1
			|| Ability.AbilitySlot == GameplayTags.InputTag_2
			|| Ability.AbilitySlot == GameplayTags.InputTag_3
			|| Ability.AbilitySlot == GameplayTags.InputTag_4
			|| Ability.AbilitySlot == GameplayTags.InputTag_Passive_1
			|| Ability.AbilitySlot == GameplayTags.InputTag_Passive_2;
		if (!bValidStatus || !bValidSlot
			|| (Ability.AbilityStatus == GameplayTags.Abilities_Status_Equipped && !Ability.AbilitySlot.IsValid())
			|| (Ability.GrantSource == static_cast<uint8>(EAuraAbilityGrantSource::Role) && Ability.GrantedRoleId.IsNone())
			|| (Ability.GrantSource == static_cast<uint8>(EAuraAbilityGrantSource::Progression) && !Ability.GrantedRoleId.IsNone()))
		{
			OutError = TEXT("Player save contains an invalid ability provenance or status/slot record.");
			return false;
		}
	}
	return true;
}

bool UAuraPersistenceSubsystem::ValidateWorldSaveRecord(const UAuraWorldSaveGame& Save,
	const FString& ExpectedWorldPersistenceId, FString& OutError, FName ExpectedMapId)
{
	OutError.Reset();
	if (Save.SaveSchemaVersion > UAuraWorldSaveGame::CurrentSchemaVersion || Save.RecordGeneration <= 0
		|| Save.WorldPersistenceId != ExpectedWorldPersistenceId || !ValidateWorldPersistenceId(Save.WorldPersistenceId, OutError)) return false;
	if (!ExpectedMapId.IsNone() && !Save.MapPackage.Equals(ExpectedMapId.ToString(), ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(TEXT("World save map '%s' does not match expected map '%s'."),
			*Save.MapPackage, *ExpectedMapId.ToString());
		return false;
	}
	if (!AuraPersistenceSubsystemPrivate::IsSortedAndUnique(Save.PopulationSlots) || !AuraPersistenceSubsystemPrivate::IsSortedAndUnique(Save.Merchants))
	{
		OutError = TEXT("World save entries must be sorted and unique by canonical identity.");
		return false;
	}
	for (const FAuraPersistedMerchantStock& Merchant : Save.Merchants)
	{
		if (Merchant.LastRestockAtUtc.GetTicks() > 0 && Merchant.LastObservedAtUtc.GetTicks() > 0
			&& Merchant.LastObservedAtUtc < Merchant.LastRestockAtUtc)
		{
			OutError = TEXT("World save contains a merchant clock that moved backward.");
			return false;
		}
		for (const FAuraPersistedOfferStock& Offer : Merchant.Offers)
		{
			if (Offer.OfferId.IsNone() || Offer.CurrentStock < -1) { OutError = TEXT("World save contains invalid merchant stock."); return false; }
		}
	}
	return true;
}

FString UAuraPersistenceSubsystem::ComputePlayerSaveChecksum(const UAuraPlayerSaveGame& Save)
{
	return AuraPersistenceSubsystemPrivate::HashText(AuraPersistenceSubsystemPrivate::RecordText(Save));
}

FString UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(const UAuraWorldSaveGame& Save)
{
	return AuraPersistenceSubsystemPrivate::HashText(AuraPersistenceSubsystemPrivate::RecordText(Save));
}

FString UAuraPersistenceSubsystem::ComputeManifestChecksum(const UAuraPersistenceManifestSaveGame& Manifest)
{
	FString Text = FString::Printf(TEXT("%d|%d|%s|%s|%d|"), Manifest.SaveSchemaVersion, Manifest.Generation,
		*Manifest.WorldPersistenceId, *Manifest.WorldRecord.RecordSlot, Manifest.WorldRecord.Generation);
	Text += FString::Printf(TEXT("%s|"), *Manifest.WorldRecord.Checksum);
	for (const FAuraPersistenceManifestRecord& Record : Manifest.PlayerRecords)
		Text += FString::Printf(TEXT("%s|%s|%d|%s;"), *Record.IdentityKey, *Record.RecordSlot, Record.Generation, *Record.Checksum);
	return AuraPersistenceSubsystemPrivate::HashText(Text);
}

bool UAuraPersistenceSubsystem::MigrateLegacySave(const ULoadScreenSaveGame& Legacy, const FAuraPlayerProfileId& Identity,
	FName RequestedRole, UAuraPlayerSaveGame& OutSave, FString& OutError)
{
	OutError.Reset();
	if (!Identity.IsValid() || RequestedRole.IsNone()) { OutError = TEXT("Legacy migration requires a validated identity and role."); return false; }
	OutSave.SaveSchemaVersion = UAuraPlayerSaveGame::CurrentSchemaVersion;
	OutSave.RecordGeneration = 0;
	OutSave.IdentityProvider = Identity.ProviderName;
	OutSave.IdentityValue = Identity.UniqueId;
	// BungeeMan was a shipped role in legacy records but is no longer a valid
	// registry key. Migrate it deterministically to the replacement playable
	// combat role so load/reconnect cannot depend on parser fallback behavior.
	const FName LegacyRole = Legacy.Role.IsNone() ? RequestedRole : Legacy.Role;
	OutSave.Role = LegacyRole == TEXT("BungeeMan") ? FName(TEXT("Crunch")) : LegacyRole;
	if (OutSave.Role.IsNone())
	{
		OutError = TEXT("Legacy migration produced no playable role.");
		return false;
	}
	OutSave.PlayerName = Legacy.PlayerName;
	OutSave.MapName = Legacy.MapName;
	OutSave.MapAssetName = Legacy.MapAssetName;
	OutSave.PlayerStartTag = Legacy.PlayerStartTag;
	OutSave.PlayerLevel = FMath::Max(1, Legacy.PlayerLevel);
	OutSave.XP = FMath::Max(0, Legacy.XP);
	OutSave.SpellPoints = FMath::Max(0, Legacy.SpellPoints);
	OutSave.AttributePoints = FMath::Max(0, Legacy.AttributePoints);
	OutSave.Strength = Legacy.Strength;
	OutSave.Intelligence = Legacy.Intelligence;
	OutSave.Resilience = Legacy.Resilience;
	OutSave.Vigor = Legacy.Vigor;
	OutSave.CurrencyId = NAME_None;
	OutSave.CurrencyBalance = 0;
	OutSave.InventorySlots.Reset();
	OutSave.SavedAbilities = Legacy.SavedAbilities;
	OutSave.SavedMaps = Legacy.SavedMaps;
	OutSave.bFirstTimeLoadIn = false;
	OutSave.bMigrationCompleted = false;
	return true;
}
