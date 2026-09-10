#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Aura/AuraLogChannels.h"
#include "World/AuraCivilianObservationMarker.h"
#include "World/AuraCivilianShelterMarker.h"
#include "World/AuraCivilianWorkMarker.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraServerLoggingMarkerTest,
	"Aura.ServerLogging.MarkerTransformAndRelevancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraServerLoggingMarkerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}

	const FVector Location(1200.f, 3400.f, 500.f);
	for (UClass* MarkerClass : { AAuraCivilianWorkMarker::StaticClass(),
		AAuraCivilianShelterMarker::StaticClass(), AAuraCivilianObservationMarker::StaticClass() })
	{
		AActor* Marker = World->SpawnActor<AActor>(MarkerClass, FTransform(Location));
		if (!TestNotNull(TEXT("Marker spawned"), Marker))
		{
			continue;
		}
		TestNotNull(TEXT("Marker has a spatial root"), Marker->GetRootComponent());
		TestTrue(TEXT("Spawn transform is retained"), Marker->GetActorLocation().Equals(Location));
		TestTrue(TEXT("Replication contract is retained"), Marker->GetIsReplicated());
		TestTrue(TEXT("Marker can participate in relevancy"),
			Marker->IsNetRelevantFor(nullptr, nullptr, Marker->GetActorLocation()));
	}

	World->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraServerLoggingDefaultTest,
	"Aura.ServerLogging.AnimationDiagnosticsDefaultOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraServerLoggingDefaultTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Animation probes are disabled in an ordinary launch"),
		UE_LOG_ACTIVE(LogAuraAnimationDiagnostics, Verbose));
	TestTrue(TEXT("Aura warnings remain enabled"), UE_LOG_ACTIVE(LogAura, Warning));
	TestTrue(TEXT("Aura errors remain enabled"), UE_LOG_ACTIVE(LogAura, Error));
	return !HasAnyErrors();
}

#endif
