// Copyright Druid Mechanics

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/AuraLandmarkMarker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraLandmarkMarkerContractTest,
	"Aura.Landmark.Marker.Contract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAuraLandmarkMarkerContractTest::RunTest(const FString& Parameters)
{
	AAuraLandmarkMarker* Marker = NewObject<AAuraLandmarkMarker>();
	TestFalse(TEXT("A marker without an ID is rejected"), Marker->IsGuideConfigured());

	Marker->LandmarkId = TEXT("TestLandmark");
	Marker->DisplayName = FText::FromString(TEXT("Test Landmark"));
	Marker->ApproachTransform = FTransform(FRotator::ZeroRotator, FVector(100.f, 200.f, 0.f));
	Marker->ArrivalRadius = 150.f;
	TestTrue(TEXT("A configured marker is accepted"), Marker->IsGuideConfigured());
	TestEqual(TEXT("World approach transform applies actor transform"),
		Marker->GetWorldApproachTransform().GetLocation(), FVector(100.f, 200.f, 0.f));

	Marker->bEnabled = false;
	TestFalse(TEXT("Disabled markers are rejected"), Marker->IsGuideConfigured());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraLandmarkBehaviorContractTest,
	"Aura.Landmark.Behavior.Contract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAuraLandmarkBehaviorContractTest::RunTest(const FString& Parameters)
{
	FString TreeText;
	const FString TreePath = FPaths::ProjectContentDir() / TEXT("BehaviorTrees/BT_LandmarkGuide.xml");
	TestTrue(TEXT("Landmark BehaviorU contract is staged in Content"), FFileHelper::LoadFileToString(TreeText, *TreePath));
	TestTrue(TEXT("Tree validates requests"), TreeText.Contains(TEXT("ValidateGuideRequest")));
	TestTrue(TEXT("Tree faces landmark"), TreeText.Contains(TEXT("FaceLandmark")));
	TestTrue(TEXT("Tree follows path"), TreeText.Contains(TEXT("FollowLandmarkPath")));
	TestTrue(TEXT("Tree confirms server arrival"), TreeText.Contains(TEXT("ConfirmGuideArrival")));
	TestTrue(TEXT("Actions forward real Running state"), TreeText.Contains(TEXT("ResultOption\" value=\"BT_RUNNING\"")));
	return true;
}
