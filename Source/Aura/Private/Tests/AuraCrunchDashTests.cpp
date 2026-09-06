#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchDash.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraCrunchDashTravelTest,
 "Aura.Migration.Crunch.Dash.TravelCollisionAndCancel",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraCrunchDashTravelTest::RunTest(const FString& Parameters)
{
 for (int32 Scenario = 0; Scenario < 4; ++Scenario)
 {
  UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
  World->AddToRoot();
  GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
  World->InitializeActorsForPlay(FURL());
  auto AddBox = [World](FVector Location, FVector Extent)
  {
   AActor* Actor = World->SpawnActor<AActor>();
   UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
   Actor->SetRootComponent(Box);
   Box->SetBoxExtent(Extent);
   Box->SetCollisionProfileName(TEXT("BlockAll"));
   Box->RegisterComponent();
   Actor->SetActorLocation(Location);
  };
  AddBox(FVector(0,0,-50), FVector(5000,5000,50));
  if (Scenario == 2) AddBox(FVector(650,0,250), FVector(50,1000,250));
  auto* Owner = World->SpawnActor<AAuraRoleApplicationTestActor>();
  Owner->SetActorLocation(FVector(0,0,100));
  Owner->InitializeTestAbilityActorInfo();
  Owner->DispatchBeginPlay();
  World->BeginPlay();
  auto* Movement = Owner->GetCharacterMovement();
  Movement->bRunPhysicsWithNoController = true;
  Movement->SetMovementMode(MOVE_Walking);
  auto* ASC = Owner->GetTestASC();
  const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UAuraCrunchDash::StaticClass(), 1));
  TestTrue(TEXT("Dash activates"), ASC->TryActivateAbility(Handle));
  const float Dt = Scenario == 1 ? 1.f/30.f : 1.f/60.f;
  for (int32 Frame = 0; Frame < FMath::CeilToInt(1.3f/Dt); ++Frame)
  {
   ++GFrameCounter; // Each synthetic world tick must advance the engine timer frame guard.
   World->Tick(LEVELTICK_All, Dt);
   if (Scenario == 3 && Frame == 25) ASC->CancelAbilityHandle(Handle);
  }
  const float Travel = Owner->GetActorLocation().X;
  AddInfo(FString::Printf(TEXT("Scenario=%d travel=%.1f"), Scenario, Travel));
  if (Scenario < 2) TestTrue(TEXT("Unobstructed dash travels approximately 1800 units at 30/60 FPS"), Travel >= 1650.f && Travel <= 1900.f);
  if (Scenario == 2) TestTrue(TEXT("Wall blocks dash without tunnelling"), Travel > 100.f && Travel < 600.f);
  if (Scenario == 3) TestTrue(TEXT("Cancellation stops early"), Travel > 100.f && Travel < 1000.f);
  TestFalse(TEXT("Ability finishes"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
  const FVector End = Owner->GetActorLocation();
  for (int32 Frame = 0; Frame < 15; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, Dt); }
  TestTrue(TEXT("No residual dash movement"), FVector::Dist2D(End, Owner->GetActorLocation()) < 1.f);
  TestFalse(TEXT("Dash root motion source removed"), Movement->GetRootMotionSource(TEXT("CrunchDashMovement")).IsValid());
  GEngine->DestroyWorldContext(World);
  World->DestroyWorld(false);
  World->RemoveFromRoot();
 }
 return !HasAnyErrors();
}
#endif
