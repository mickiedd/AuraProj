// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Battle/AuraBattleDirector.h"
#include "Engine/World.h"

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

#endif
