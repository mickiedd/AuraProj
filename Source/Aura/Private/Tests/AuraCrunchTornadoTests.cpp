#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchTornado.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "AuraGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuraCrunchTornadoRuntimeTest,
	"Aura.Migration.Crunch.Tornado.DamagePresentationAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAuraCrunchTornadoRuntimeTest::RunTest(const FString& Parameters)
{
	auto* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_Tornado_Aura.AM_Tornado_Aura"));
	if (!TestNotNull(TEXT("Repaired montage loads"), Montage)) return false;
	TestEqual(TEXT("Four second montage"), Montage->GetPlayLength(), 4.f);
	TestEqual(TEXT("One montage slot"), Montage->SlotAnimTracks.Num(), 1);
	TestEqual(TEXT("No legacy montage notifies"), Montage->Notifies.Num(), 0);
	if (Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1) return false;
	auto* Animation = Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
	if (!TestNotNull(TEXT("Actual spin animation resolves"), Animation)) return false;
	TestTrue(TEXT("Spin has authored bone tracks"), Animation->GetDataModel()->GetNumBoneTracks() > 0);
	TestTrue(TEXT("Animation skeleton agrees with montage"), Animation->GetSkeleton() == Montage->GetSkeleton());
	TestEqual(TEXT("No legacy animation notifies"), Animation->Notifies.Num(), 0);
	TestNotNull(TEXT("Tornado effect resolves"), GetDefault<AAuraRoleApplicationTestActor>()->TornadoEffect.Get());

	for (int32 Scenario = 0; Scenario < 2; ++Scenario)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		World->AddToRoot();
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto MakeActor = [&](FVector Location, bool Hostile)
		{
			auto* Actor = World->SpawnActor<AAuraRoleApplicationTestActor>(AAuraRoleApplicationTestActor::StaticClass(), FTransform(Location), Spawn);
			Actor->InitializeTestAbilityActorInfo();
			Actor->DispatchBeginPlay();
			FAuraCombatIdentity Identity = Actor->GetCombatIdentity();
			if (Hostile) Identity.FactionTag = FAuraGameplayTags::Get().Faction_Enemy;
			Actor->InitializeCombatIdentityForTest(Identity);
			Actor->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Actor->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
			Actor->GetCharacterMovement()->DisableMovement();
			Actor->GetTestASC()->SetNumericAttributeBase(UAuraAttributeSet::GetMaxHealthAttribute(), 10000.f);
			Actor->GetTestASC()->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), 10000.f);
			return Actor;
		};
		auto* Owner = MakeActor(FVector::ZeroVector, false);
		auto* Enemy = MakeActor(FVector(150,0,0), true);
		auto* Friend = MakeActor(FVector(0,150,0), false);
		auto* Outside = MakeActor(FVector(600,0,0), true);
		auto* Dead = MakeActor(FVector(-150,0,0), true);
		Dead->GetCombatStateComponentMutable()->TryEnterDying();
		World->BeginPlay();
		// Exercise real GAS montage playback on the same mesh and AnimBP used by Crunch.
		Owner->GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Assets/Characters/Crunch/Meshes/SM_CrunchV4.SM_CrunchV4")));
		Owner->GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV5.ABP_Crunch_AuraV5_C")));
		Owner->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		auto* ASC = Owner->GetTestASC();
		const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(UAuraCrunchTornado::StaticClass(), 1));
		const FGameplayTag Cue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Crunch.Tornado"));
		// The fixture overrides cue dispatch; exercise the production handler explicitly too.
		FGameplayCueParameters CueParameters;
		Owner->GameplayCue_Crunch_Tornado(EGameplayCueEvent::OnActive, CueParameters);
		UNiagaraComponent* Effect = Owner->TornadoEffectComponent;
		if (FApp::CanEverRender()) TestNotNull(TEXT("Native persistent cue creates a component"), Effect);
		else TestNull(TEXT("Headless run skips Niagara creation"), Effect);
		Owner->GameplayCue_Crunch_Tornado(EGameplayCueEvent::WhileActive, CueParameters);
		TestTrue(TEXT("Duplicate active cue does not spawn duplicate effects"), Owner->TornadoEffectComponent == Effect);
		Owner->GameplayCue_Crunch_Tornado(EGameplayCueEvent::Removed, CueParameters);
		TestNull(TEXT("Native cue removal releases component"), Owner->TornadoEffectComponent.Get());
		TestTrue(TEXT("Tornado activates"), ASC->TryActivateAbility(Handle));
		TestTrue(TEXT("Persistent cue active"), ASC->HasMatchingGameplayTag(Cue));
		TestTrue(TEXT("Real spin montage playing"), Owner->GetMesh()->GetAnimInstance() && Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage));
		auto Tick = [&](int32 Frames) { for (int32 Frame=0; Frame<Frames; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, 1.f/60.f); } };
		auto Health = [](AAuraRoleApplicationTestActor* Actor) { return Actor->GetTestASC()->GetNumericAttribute(UAuraAttributeSet::GetHealthAttribute()); };
		Tick(20);
		TestTrue(TEXT("Authority cadence damages nearby enemy"), Health(Enemy) < 10000.f);
		TestEqual(TEXT("Friendly untouched"), Health(Friend), 10000.f);
		TestEqual(TEXT("Outside radius untouched"), Health(Outside), 10000.f);
		TestEqual(TEXT("Dying target untouched"), Health(Dead), 10000.f);
		TestEqual(TEXT("Owner untouched"), Health(Owner), 10000.f);
		if (Scenario == 1) ASC->CancelAbilityHandle(Handle);
		else Tick(250);
		TestFalse(TEXT("Ability ends on timeout or cancellation"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
		TestFalse(TEXT("Persistent cue removed"), ASC->HasMatchingGameplayTag(Cue));
		const float EndHealth = Health(Enemy);
		Tick(30);
		TestEqual(TEXT("No late timer damage after end"), Health(Enemy), EndHealth);
		TestFalse(TEXT("Montage stopped"), Owner->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage));
		TestTrue(TEXT("Can cast again after cleanup"), ASC->TryActivateAbility(Handle));
		ASC->CancelAbilityHandle(Handle);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World->RemoveFromRoot();
	}
	return !HasAnyErrors();
}
#endif
