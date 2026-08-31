// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Battle/AuraBattleDirector.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Player/AuraPlayerState.h"
#include "UI/HUD/AuraHUD.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebHUDBattlePopulationSummaryTest,
	"Aura.UI.WebHUD.BattlePopulationSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebHUDBattlePopulationSummaryTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Transient Web HUD state test world created"), TestWorld))
	{
		return false;
	}
	TestWorld->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraBattleDirector* Director = TestWorld->SpawnActor<AAuraBattleDirector>(AAuraBattleDirector::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Battle director fixture spawned"), Director))
	{
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
		return false;
	}

	TestTrue(TEXT("Standalone battle director fixture has authority"), Director->HasAuthority());
	Director->UpdatePopulationSummary(7, 10, 2, 4);
	TestEqual(TEXT("HUD receives active population count"), Director->GetPopulationActiveCount(), 7);
	TestEqual(TEXT("HUD receives configured population capacity"), Director->GetPopulationMaximumCount(), 10);
	TestEqual(TEXT("HUD receives pending refill count"), Director->GetPopulationPendingCount(), 2);
	TestEqual(TEXT("HUD receives accepted casualty count"), Director->GetPopulationCasualtyCount(), 4);

	Director->UpdatePopulationSummary(-1, -2, -3, -4);
	TestEqual(TEXT("Negative active population values fail closed"), Director->GetPopulationActiveCount(), 0);
	TestEqual(TEXT("Negative capacity values fail closed"), Director->GetPopulationMaximumCount(), 0);
	TestEqual(TEXT("Negative pending refill values fail closed"), Director->GetPopulationPendingCount(), 0);
	TestEqual(TEXT("Negative casualty values fail closed"), Director->GetPopulationCasualtyCount(), 0);

	Director->Destroy();
	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraWebHUDFirearmPresentationTest,
	"Aura.UI.WebHUD.FirearmPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraWebHUDFirearmPresentationTest::RunTest(const FString& Parameters)
{
	FAuraFirearmState State;
	State.bApplicable = true;
	State.MagazineCapacity = State.MagazineRounds = 12;
	State.ReserveCapacity = State.ReserveRounds = 48;
	State.ReloadDuration = 1.25f;
	State.FireMode = TEXT("SemiAuto");
	auto Check = [this, &State](const TCHAR* ExpectedState, bool bCanFire, bool bCanReload,
		FName Role = TEXT("BungeeMan"), bool bAlive = true)
	{
		const TSharedRef<FJsonObject> Payload = AAuraHUD::BuildFirearmStatePayload(State, Role, bAlive);
		TestEqual(TEXT("Presentation state"), Payload->GetStringField(TEXT("state")), FString(ExpectedState));
		TestEqual(TEXT("Fire availability"), Payload->GetBoolField(TEXT("canFire")), bCanFire);
		TestEqual(TEXT("Reload availability"), Payload->GetBoolField(TEXT("canReload")), bCanReload);
		TestEqual(TEXT("Magazine remains authoritative"), Payload->GetIntegerField(TEXT("magazineRounds")), State.MagazineRounds);
		TestEqual(TEXT("Reserve remains authoritative"), Payload->GetIntegerField(TEXT("reserveRounds")), State.ReserveRounds);
		return Payload;
	};
	const auto Ready = Check(TEXT("Ready"), true, false);
	TestEqual(TEXT("Only gun ability owns ammo presentation"), Ready->GetStringField(TEXT("abilityTag")), FString(TEXT("Abilities.Gun.Fire")));
	TestEqual(TEXT("Only LMB owns ammo presentation"), Ready->GetStringField(TEXT("inputTag")), FString(TEXT("InputTag.LMB")));
	TestEqual(TEXT("Configured reload duration"), Ready->GetNumberField(TEXT("reloadDuration")), 1.25);
	State.MagazineRounds = 7;
	Check(TEXT("Ready"), true, true);
	State.MagazineRounds = 0;
	const auto Empty = Check(TEXT("EmptyMagazine"), false, true);
	TestTrue(TEXT("Empty magazine explains manual reload key"), Empty->GetStringField(TEXT("unavailableReason")).Contains(TEXT("R to reload")));
	State.bReloading = true;
	Check(TEXT("Reloading"), false, false);
	// Rendering never completes a reload locally, even when queried repeatedly.
	Check(TEXT("Reloading"), false, false);
	TestEqual(TEXT("Presentation did not refill magazine"), State.MagazineRounds, 0);
	State.bReloading = false;
	State.MagazineRounds = 12;
	State.ReserveRounds = 36;
	Check(TEXT("Ready"), true, false);
	State.MagazineRounds = 0;
	State.ReserveRounds = 0;
	Check(TEXT("OutOfAmmo"), false, false);
	State.MagazineRounds = 3;
	Check(TEXT("Ready"), true, false);
	State.ReserveRounds = 36;
	Check(TEXT("Unavailable"), false, false, TEXT("BungeeMan"), false);
	State.bReloading = true;
	Check(TEXT("Unavailable"), false, false, TEXT("BungeeMan"), false);
	const auto Aura = Check(TEXT("NotApplicable"), false, false, TEXT("Aura"));
	TestFalse(TEXT("Aura never displays firearm UI"), Aura->GetBoolField(TEXT("applicable")));
	TestTrue(TEXT("Aura does not project a gun ability"), Aura->GetStringField(TEXT("abilityTag")).IsEmpty());
	Check(TEXT("NotApplicable"), false, false, NAME_None);
	State.bApplicable = false;
	Check(TEXT("NotApplicable"), false, false);
	return !HasAnyErrors();
}

#endif
