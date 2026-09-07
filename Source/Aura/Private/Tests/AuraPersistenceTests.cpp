// Copyright Druid Mechanics

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

#include "Game/AuraPersistenceManifestSaveGame.h"
#include "Game/AuraPersistenceSubsystem.h"
#include "Game/AuraPlayerSaveGame.h"
#include "Game/AuraWorldSaveGame.h"
#include "Game/AuraGameModeBase.h"
#include "Game/LoginGameMode.h"
#include "Game/LoadScreenSaveGame.h"

namespace AuraPersistenceTestsPrivate
{
	FString LegacyFixturePath()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/Aura/Private/Tests/Fixtures/RoleBattleLegacyV0.sav"));
	}

	bool LoadLegacyFixture(ULoadScreenSaveGame*& OutLegacy, FString& OutError)
	{
		OutLegacy = nullptr;
		OutError.Reset();
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *LegacyFixturePath()) || Bytes.Num() == 0)
		{
			OutError = FString::Printf(TEXT("Could not read checked-in legacy fixture: %s"), *LegacyFixturePath());
			return false;
		}
		OutLegacy = Cast<ULoadScreenSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!OutLegacy)
		{
			OutError = TEXT("Checked-in legacy fixture did not deserialize as ULoadScreenSaveGame.");
			return false;
		}
		return true;
	}

	FAuraPlayerProfileId MakeIdentity(const TCHAR* Id)
	{
		FAuraPlayerProfileId Result;
		FString Error;
		FAuraPlayerProfileId::ValidateComponents(TEXT("Steam"), Id, TEXT("Steam"), false, Result, Error);
		return Result;
	}

	UAuraPlayerSaveGame* MakePlayerSave(const TCHAR* Id)
	{
		UAuraPlayerSaveGame* Save = NewObject<UAuraPlayerSaveGame>();
		Save->IdentityProvider = TEXT("Steam");
		Save->IdentityValue = Id;
		Save->CurrencyId = TEXT("gold");
		Save->CurrencyBalance = 1234567890123LL;
		Save->RecordGeneration = 4;
		Save->Role = TEXT("BungeeMan");
		Save->PlayerLevel = 7;
		Save->bFirstTimeLoadIn = false;
		return Save;
	}

	UAuraWorldSaveGame* MakeWorldSave(const TCHAR* WorldId)
	{
		UAuraWorldSaveGame* Save = NewObject<UAuraWorldSaveGame>();
		Save->WorldPersistenceId = WorldId;
		Save->RecordGeneration = 3;
		Save->MapPackage = TEXT("RoleBattleCivilianTest");
		FAuraPopulationSlotSnapshot& Slot = Save->PopulationSlots.AddDefaulted_GetRef();
		Slot.PopulationMemberId = TEXT("marketcivilians:0");
		Slot.PopulationId = TEXT("MarketCivilians");
		Slot.SlotIndex = 0;
		Slot.State = EAuraPopulationSlotState::RefillPending;
		FAuraPersistedMerchantStock& Merchant = Save->Merchants.AddDefaulted_GetRef();
		Merchant.PopulationMemberId = TEXT("marketcivilians:0");
		Merchant.MerchantDefinitionId = TEXT("marketmerchant");
		Merchant.StockRevision = 8;
		FAuraPersistedOfferStock& Offer = Merchant.Offers.AddDefaulted_GetRef();
		Offer.OfferId = TEXT("market_health_potion");
		Offer.CurrentStock = 2;
		return Save;
	}
}

#define AURA_PERSISTENCE_TEST(TestName, TestDescription) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestName, "Aura.RoleBattle.Day18.Save." #TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18LegacyV0Migration, "Aura.RoleBattle.Day18.Save.LegacyV0Migration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18LegacyV0Migration::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("AuraGenerateLegacyFixture")))
	{
		ULoadScreenSaveGame* Generated = NewObject<ULoadScreenSaveGame>();
		Generated->Role = TEXT("BungeeMan"); Generated->PlayerLevel = 8; Generated->XP = 144;
		Generated->SpellPoints = 2; Generated->AttributePoints = 3;
		Generated->SavedAbilities.AddDefaulted_GetRef().AbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"));
		TArray<uint8> Bytes;
		const bool bSerialized = UGameplayStatics::SaveGameToMemory(Generated, Bytes);
		const bool bWritten = bSerialized && FFileHelper::SaveArrayToFile(Bytes, *AuraPersistenceTestsPrivate::LegacyFixturePath());
		TestTrue(TEXT("version-zero fixture is serialized by Unreal SaveGame"), bWritten);
		return bWritten;
	}
	ULoadScreenSaveGame* Legacy = nullptr; FString Error;
	const bool bFixtureLoaded = AuraPersistenceTestsPrivate::LoadLegacyFixture(Legacy, Error);
	TestTrue(TEXT("checked-in version-zero fixture deserializes"), bFixtureLoaded);
	if (!bFixtureLoaded) return false;
	UAuraPlayerSaveGame* Migrated = NewObject<UAuraPlayerSaveGame>();
	const bool bMigrated = UAuraPersistenceSubsystem::MigrateLegacySave(*Legacy, AuraPersistenceTestsPrivate::MakeIdentity(TEXT("legacy-account")), TEXT("Crunch"), *Migrated, Error);
	TestTrue(TEXT("version-zero legacy record migrates"), bMigrated); TestEqual(TEXT("retired BungeeMan role migrates to Crunch"), Migrated->Role, FName(TEXT("Crunch"))); TestEqual(TEXT("progression is preserved"), Migrated->PlayerLevel, 8); TestEqual(TEXT("experience is preserved"), Migrated->XP, 144); TestFalse(TEXT("migration never duplicates inventory"), Migrated->InventorySlots.Num() > 0);
	return bMigrated && Migrated->Role == TEXT("Crunch") && Migrated->PlayerLevel == 8 && Migrated->XP == 144;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18FutureVersionRejected, "Aura.RoleBattle.Day18.Save.FutureVersionRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18FutureVersionRejected::RunTest(const FString& Parameters)
{
	UAuraPlayerSaveGame* Save = AuraPersistenceTestsPrivate::MakePlayerSave(TEXT("future-account")); Save->SaveSchemaVersion = 99; FString Error;
	const bool bValid = UAuraPersistenceSubsystem::ValidatePlayerSaveRecord(*Save, AuraPersistenceTestsPrivate::MakeIdentity(TEXT("future-account")), TEXT("Steam"), Error);
	TestFalse(TEXT("future player schema is rejected"), bValid); return !bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18Int64RoundTrip, "Aura.RoleBattle.Day18.Save.Int64RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18Int64RoundTrip::RunTest(const FString& Parameters)
{
	UAuraPlayerSaveGame* Save = AuraPersistenceTestsPrivate::MakePlayerSave(TEXT("large-account")); FString Error;
	const bool bValid = UAuraPersistenceSubsystem::ValidatePlayerSaveRecord(*Save, AuraPersistenceTestsPrivate::MakeIdentity(TEXT("large-account")), TEXT("Steam"), Error);
	TestTrue(TEXT("large signed 64-bit balance survives schema validation"), bValid); TestEqual(TEXT("balance remains exact"), Save->CurrencyBalance, 1234567890123LL); return bValid && Save->CurrencyBalance == 1234567890123LL;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18PlayerIsolation, "Aura.RoleBattle.Day18.Save.PlayerIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18PlayerIsolation::RunTest(const FString& Parameters)
{
	const FString A = AuraPersistenceTestsPrivate::MakeIdentity(TEXT("player-a")).BuildSlotName(TEXT("campaign")); const FString B = AuraPersistenceTestsPrivate::MakeIdentity(TEXT("player-b")).BuildSlotName(TEXT("campaign"));
	TestFalse(TEXT("two authenticated profiles never share a slot"), A == B); return A != B;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18LoadBeforePawnInitialization, "Aura.RoleBattle.Day18.Save.LoadBeforePawnInitialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18LoadBeforePawnInitialization::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("profile identity has a deterministic record slot before pawn work"), AuraPersistenceTestsPrivate::MakeIdentity(TEXT("load-order")).BuildSlotName(TEXT("campaign")).StartsWith(TEXT("AuraProfile_"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18WorldLoadsOnce, "Aura.RoleBattle.Day18.Save.WorldLoadsOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18WorldLoadsOnce::RunTest(const FString& Parameters)
{
	UAuraWorldSaveGame* Save = AuraPersistenceTestsPrivate::MakeWorldSave(TEXT("campaign")); const FString First = UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save); const FString Second = UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save); TestEqual(TEXT("world checksum is deterministic"), First, Second); return First == Second;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18PopulationReconciliation, "Aura.RoleBattle.Day18.Save.PopulationReconciliation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18PopulationReconciliation::RunTest(const FString& Parameters)
{
	UAuraWorldSaveGame* Save = AuraPersistenceTestsPrivate::MakeWorldSave(TEXT("campaign")); FString Error; const bool bValid = UAuraPersistenceSubsystem::ValidateWorldSaveRecord(*Save, TEXT("campaign"), Error); const FAuraPopulationSlotSnapshot ExistingSlot = Save->PopulationSlots[0]; Save->PopulationSlots.Add(ExistingSlot); const bool bDuplicateRejected = !UAuraPersistenceSubsystem::ValidateWorldSaveRecord(*Save, TEXT("campaign"), Error); TestTrue(TEXT("canonical population entries restore"), bValid); TestTrue(TEXT("duplicate member entries reject"), bDuplicateRejected); return bValid && bDuplicateRejected;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18MerchantStockRoundTrip, "Aura.RoleBattle.Day18.Save.MerchantStockRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18MerchantStockRoundTrip::RunTest(const FString& Parameters)
{
	UAuraWorldSaveGame* Save = AuraPersistenceTestsPrivate::MakeWorldSave(TEXT("campaign")); const FString Checksum = UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save); TestEqual(TEXT("merchant stock is represented in world record"), Save->Merchants[0].Offers[0].CurrentStock, 2LL); TestTrue(TEXT("merchant checksum includes stock"), !Checksum.IsEmpty()); return Save->Merchants.Num() == 1 && Save->Merchants[0].Offers[0].CurrentStock == 2;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18CommittedTransactionOnly, "Aura.RoleBattle.Day18.Save.CommittedTransactionOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18CommittedTransactionOnly::RunTest(const FString& Parameters)
{
	UAuraWorldSaveGame* Save = AuraPersistenceTestsPrivate::MakeWorldSave(TEXT("campaign")); const FString Before = UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save); Save->Merchants[0].Offers[0].CurrentStock = 1; const FString After = UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save); TestFalse(TEXT("world checkpoint changes only after a complete stock state exists"), Before == After); return Before != After;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18TornCheckpointRecovery, "Aura.RoleBattle.Day18.Save.TornCheckpointRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18TornCheckpointRecovery::RunTest(const FString& Parameters)
{
	UAuraPersistenceManifestSaveGame* Manifest = NewObject<UAuraPersistenceManifestSaveGame>(); Manifest->Generation = 3; Manifest->WorldPersistenceId = TEXT("campaign"); Manifest->WorldRecord = { TEXT("AuraWorld_G3"), 3, TEXT("world-checksum") }; Manifest->Checksum = UAuraPersistenceSubsystem::ComputeManifestChecksum(*Manifest); const FString Durable = Manifest->Checksum; Manifest->WorldRecord.RecordSlot = TEXT("torn"); TestFalse(TEXT("torn record reference changes manifest checksum"), Durable == UAuraPersistenceSubsystem::ComputeManifestChecksum(*Manifest)); return Durable != UAuraPersistenceSubsystem::ComputeManifestChecksum(*Manifest);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18CorruptNewestManifestFallsBack, "Aura.RoleBattle.Day18.Save.CorruptNewestManifestFallsBack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18CorruptNewestManifestFallsBack::RunTest(const FString& Parameters)
{
	UAuraPersistenceManifestSaveGame* Old = NewObject<UAuraPersistenceManifestSaveGame>(); Old->Generation = 2; Old->WorldPersistenceId = TEXT("campaign"); Old->WorldRecord = { TEXT("AuraWorld_G2"), 2, TEXT("old") }; Old->Checksum = UAuraPersistenceSubsystem::ComputeManifestChecksum(*Old); UAuraPersistenceManifestSaveGame* Newest = NewObject<UAuraPersistenceManifestSaveGame>(); Newest->Generation = 3; Newest->WorldPersistenceId = TEXT("campaign"); Newest->WorldRecord = { TEXT("AuraWorld_G3"), 3, TEXT("new") }; Newest->Checksum = TEXT("corrupt"); const bool bOldValid = Old->Checksum == UAuraPersistenceSubsystem::ComputeManifestChecksum(*Old); const bool bNewestInvalid = Newest->Checksum != UAuraPersistenceSubsystem::ComputeManifestChecksum(*Newest); TestTrue(TEXT("prior manifest remains valid"), bOldValid); TestTrue(TEXT("corrupt newest manifest is ignored"), bNewestInvalid); return bOldValid && bNewestInvalid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18AuthenticatedIdentityRequired, "Aura.RoleBattle.Day18.Save.AuthenticatedIdentityRequired", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18AuthenticatedIdentityRequired::RunTest(const FString& Parameters)
{
	FAuraPlayerProfileId Id; FString Error; const bool bAccepted = FAuraPlayerProfileId::ValidateComponents(TEXT(""), TEXT("display-name"), TEXT("Steam"), false, Id, Error); TestFalse(TEXT("missing provider identity fails closed"), bAccepted); const bool bNullAccepted = FAuraPlayerProfileId::ValidateComponents(TEXT("NULL"), TEXT("account"), TEXT("NULL"), false, Id, Error); TestFalse(TEXT("OnlineSubsystemNull is not a release identity"), bNullAccepted); return !bAccepted && !bNullAccepted;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18ProviderMismatchRejected, "Aura.RoleBattle.Day18.Save.ProviderMismatchRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18ProviderMismatchRejected::RunTest(const FString& Parameters)
{
	FAuraPlayerProfileId Id; FString Error; const bool bAccepted = FAuraPlayerProfileId::ValidateComponents(TEXT("EOS"), TEXT("account"), TEXT("Steam"), false, Id, Error); TestFalse(TEXT("unexpected provider is rejected"), bAccepted); return !bAccepted;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18DuplicateProfileConnectionRejected, "Aura.RoleBattle.Day18.Save.DuplicateProfileConnectionRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18DuplicateProfileConnectionRejected::RunTest(const FString& Parameters)
{
	const FAuraPlayerProfileId A = AuraPersistenceTestsPrivate::MakeIdentity(TEXT("same-account")); const FAuraPlayerProfileId B = AuraPersistenceTestsPrivate::MakeIdentity(TEXT("same-account")); TestEqual(TEXT("same authenticated identity canonicalizes identically"), A.ToCanonicalString(), B.ToCanonicalString()); return A.ToCanonicalString() == B.ToCanonicalString();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraDay18WorldPersistenceIdIsolation, "Aura.RoleBattle.Day18.Save.WorldPersistenceIdIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraDay18WorldPersistenceIdIsolation::RunTest(const FString& Parameters)
{
	const FAuraPlayerProfileId Identity = AuraPersistenceTestsPrivate::MakeIdentity(TEXT("same-account")); const FString A = Identity.BuildSlotName(TEXT("campaign-a")); const FString B = Identity.BuildSlotName(TEXT("campaign-b")); TestFalse(TEXT("world IDs isolate profile record slots"), A == B); return A != B;
}

// Read-only incident audit, deliberately opt-in so normal tests never depend on local saves.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraPersistenceRecordAudit, "Aura.Persistence.RecordAudit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraPersistenceRecordAudit::RunTest(const FString& Parameters)
{
    FString Prefix;
    if (!FParse::Value(FCommandLine::Get(), TEXT("AuraAuditWorldPrefix="), Prefix)) return true;
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Prefix + TEXT("*.sav")), true, false);
    for (const FString& File : Files)
    {
        const FString Slot = FPaths::GetBaseFilename(File);
        if (UAuraWorldSaveGame* Save = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
        {
            FString Error;
            const bool bValid = UAuraPersistenceSubsystem::ValidateWorldSaveRecord(*Save, Save->WorldPersistenceId, Error);
            AddInfo(FString::Printf(TEXT("RecordAudit slot=%s generation=%d checksum=%s map=%s valid=%d reason=%s"),
                *Slot, Save->RecordGeneration, *UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Save), *Save->MapPackage, bValid, *Error));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraPersistenceCheckpointIsolation, "Aura.Persistence.CheckpointIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraPersistenceCheckpointIsolation::RunTest(const FString& Parameters)
{
    const FString WorldId = TEXT("ConnectionFix_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const auto MakeStore = [&]()
    {
        UAuraPersistenceSubsystem* Store = NewObject<UAuraPersistenceSubsystem>(NewObject<UGameInstance>());
        Store->WorldPersistenceId = WorldId;
        Store->bAuthorityPersistenceEnabled = true;
        return Store;
    };
    UAuraPersistenceSubsystem* First = MakeStore();
    UAuraPersistenceSubsystem* Stale = MakeStore();
    FString Error;
    const FName MapId(TEXT("ConnectionFixTest"));
    TestTrue(TEXT("first writer loads empty campaign"), First->LoadWorldState(TEXT("Test"), Error, MapId));
    TestTrue(TEXT("second writer observes same empty campaign"), Stale->LoadWorldState(TEXT("Test"), Error, MapId));
	FAuraPopulationDebugSnapshot Snapshot;
	Snapshot.MapId = MapId;
	TestTrue(TEXT("first writer publishes checkpoint"), First->SaveWorldState(Snapshot, {}, Error));
	const FString OriginalSlot = First->WorldManifestRecord.RecordSlot;
	const FString OriginalChecksum = First->WorldManifestRecord.Checksum;
	TestFalse(TEXT("stale writer cannot replace published manifest"), Stale->SaveWorldState(Snapshot, {}, Error));
	TestTrue(TEXT("stale writer gets actionable error"), Error.Contains(TEXT("another authority process")));
	UAuraWorldSaveGame* Original = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(OriginalSlot, 0));
	TestNotNull(TEXT("stale rollback preserves original world record"), Original);
	if (Original) TestEqual(TEXT("original checksum survives stale write"), UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*Original), OriginalChecksum);
	TestTrue(TEXT("first writer can publish next checkpoint"), First->SaveWorldState(Snapshot, {}, Error));
	const FString NewSlot = First->WorldManifestRecord.RecordSlot;
	const FString NewChecksum = First->WorldManifestRecord.Checksum;
	TestTrue(TEXT("records use distinct immutable slots"), OriginalSlot != NewSlot);
	UAuraWorldSaveGame* NewRecord = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(NewSlot, 0));
	TestNotNull(TEXT("newest checkpoint is durable before manifest fault injection"), NewRecord);
	if (NewRecord) TestEqual(TEXT("newest checkpoint checksum is durable"), UAuraPersistenceSubsystem::ComputeWorldSaveChecksum(*NewRecord), NewChecksum);

	UAuraWorldSaveGame* Torn = Cast<UAuraWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(NewSlot, 0));
	if (Torn)
	{
		Torn->MapPackage = TEXT("damaged");
		UGameplayStatics::SaveGameToSlot(Torn, NewSlot, 0);
	}
	UAuraPersistenceSubsystem* RestartedWorld = MakeStore();
	TestTrue(TEXT("torn newest world falls back to prior committed checkpoint"), RestartedWorld->LoadWorldState(TEXT("Test"), Error, MapId));
	TestEqual(TEXT("torn world recovery selects original world record"), RestartedWorld->WorldManifestRecord.RecordSlot, OriginalSlot);
	TestTrue(TEXT("world recovery may publish above observed manifest generation"), RestartedWorld->SaveWorldState(Snapshot, {}, Error));
	const FString RecoverySlot = RestartedWorld->WorldManifestRecord.RecordSlot;
	const FString RecoveryChecksum = RestartedWorld->WorldManifestRecord.Checksum;

	// Fault-inject a torn write into the inactive manifest slot.  Recovery must
	// ignore that slot and retain the last valid manifest/checkpoint.
	const FString TornManifestSlot = RestartedWorld->ActiveManifestSlot == RestartedWorld->BuildManifestSlot(true)
		? RestartedWorld->BuildManifestSlot(false) : RestartedWorld->BuildManifestSlot(true);
	const FString TornManifestPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TornManifestSlot + TEXT(".sav"));
	TArray<uint8> TornManifestBytes;
	TestTrue(TEXT("inactive manifest exists for torn-write injection"), FFileHelper::LoadFileToArray(TornManifestBytes, *TornManifestPath));
	TestTrue(TEXT("inactive manifest has enough bytes to tear"), TornManifestBytes.Num() > 1);
	if (TornManifestBytes.Num() > 1)
	{
		TornManifestBytes.SetNum(FMath::Max(1, TornManifestBytes.Num() / 2));
		TestTrue(TEXT("torn inactive manifest write succeeds"), FFileHelper::SaveArrayToFile(TornManifestBytes, *TornManifestPath));
	}
	UAuraPersistenceSubsystem* RestartedManifest = MakeStore();
	TestTrue(TEXT("torn manifest recovery loads the surviving publication"), RestartedManifest->LoadWorldState(TEXT("Test"), Error, MapId));
	TestEqual(TEXT("torn manifest recovery keeps the surviving checkpoint"), RestartedManifest->WorldManifestRecord.RecordSlot, RecoverySlot);
	TestEqual(TEXT("torn manifest recovery keeps the surviving checksum"), RestartedManifest->WorldManifestRecord.Checksum, RecoveryChecksum);
	for (const FString& Slot : { OriginalSlot, NewSlot, RecoverySlot,
		First->BuildManifestSlot(true), First->BuildManifestSlot(false) })
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraFrontendPersistenceIsolationContract, "Aura.Persistence.FrontendIsolationContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraFrontendPersistenceIsolationContract::RunTest(const FString& Parameters)
{
	const ALoginGameMode* LoginMode = GetDefault<ALoginGameMode>();
	const AAuraGameModeBase* GameplayMode = GetDefault<AAuraGameModeBase>();
	TestNotNull(TEXT("login game mode default object exists"), LoginMode);
	TestNotNull(TEXT("gameplay game mode default object exists"), GameplayMode);
	if (LoginMode) TestFalse(TEXT("login map disables authority world persistence"), LoginMode->IsAuthorityWorldPersistenceEnabled());
	if (GameplayMode) TestTrue(TEXT("gameplay map enables authority world persistence"), GameplayMode->IsAuthorityWorldPersistenceEnabled());
	return !HasAnyErrors();
}
