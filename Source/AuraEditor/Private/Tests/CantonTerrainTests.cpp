#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Terrain/CantonTerrainLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FLoadCantonRegion);
bool FLoadCantonRegion::Update()
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if (World && World->GetWorldPartition())
        World->GetWorldPartition()->LoadLastLoadedRegions({FBox(FVector(-1000,-1000,-20000),FVector(404200,404200,20000))});
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckCantonTerrain, FAutomationTestBase*, Test);
bool FCheckCantonTerrain::Update()
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    const FString Json=UCantonTerrainLibrary::ValidateProvisionalTerrain(World,
        FPaths::ProjectDir()/TEXT("Export/Provisional/Canton_Modern_Context_SouthFirst.r16"));
    FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("QA/UE_Import_Screenshots/Automation_Validation.json")));
    TSharedPtr<FJsonObject> Result;
    const bool Parsed=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Result);
    Test->TestTrue(TEXT("Canton persisted WP map, layers, scale, heights and collision"),Parsed && Result->GetBoolField(TEXT("passed")));
    if (Parsed && !Result->GetBoolField(TEXT("passed"))) Test->AddError(Json);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCantonTerrainMapTest,"Aura.Canton.Provisional.MapConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCantonTerrainMapTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL")));
    ADD_LATENT_AUTOMATION_COMMAND(FLoadCantonRegion());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(20.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckCantonTerrain(this));
    return true;
}
#endif
