#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Actor/AuraEffectActor.h"
#include "Actor/AuraFireBall.h"
#include "Actor/AuraBullet.h"
#include "Actor/AuraProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/AuraGameplayConfig.h"
#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "AuraAbilityGraph/Public/Nodes/Actions/PlayMontageNode.h"
#include "AuraAbilityGraph/Public/Nodes/Actions/WaitForMontageEventNode.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Tests/Fixtures/AuraBulletPresentationTestActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraBulletPresentationTest,
	"Aura.Projectiles.FireGunPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraBulletPresentationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Isolated projectile world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Native RPC dispatch uses ProcessEvent, which requires initialized actors.
	World->InitializeActorsForPlay(FURL());
	const auto Cleanup = [&]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	const FAuraProjectileDefinition* Definition = FAuraGameplayConfig::FindProjectile(TEXT("fireGunBullet"));
	if (!TestNotNull(TEXT("Bullet definition"), Definition))
	{
		Cleanup();
		return false;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const auto SpawnBullet = [&]()
	{
		const FTransform SpawnTransform(FVector(500.f, 0.f, 0.f));
		AAuraBulletPresentationTestActor* Bullet = World->SpawnActorDeferred<AAuraBulletPresentationTestActor>(
			AAuraBulletPresentationTestActor::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Bullet)
		{
			Bullet->ConfigureFromDefinition(TEXT("fireGunBullet"));
			Bullet->FinishSpawning(SpawnTransform);
			Bullet->DispatchBeginPlay();
		}
		return Bullet;
	};
	const auto GetMarks = [&]()
	{
		TArray<UDecalComponent*> Marks;
		for (TObjectIterator<UDecalComponent> It; It; ++It)
		{
			if (IsValid(*It) && It->GetWorld() == World) Marks.Add(*It);
		}
		return Marks;
	};

	AAuraBulletPresentationTestActor* Bullet = SpawnBullet();
	if (!TestNotNull(TEXT("Configured bullet spawns"), Bullet))
	{
		Cleanup();
		return false;
	}
	TestEqual(TEXT("Definition lifetime survives BeginPlay"), Bullet->GetLifeSpan(), Definition->LifeSpan);
	TestEqual(TEXT("Definition speed retained"), Bullet->ProjectileMovement->InitialSpeed, Definition->InitialSpeed);
	TestTrue(TEXT("Deferred-spawn velocity initializes at the configured speed"),
		FMath::IsNearlyEqual(Bullet->ProjectileMovement->Velocity.Size(), static_cast<double>(Definition->InitialSpeed)));
	TArray<UStaticMeshComponent*> Meshes;
	Bullet->GetComponents(Meshes);
	bool bHasTracer = false;
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetStaticMesh() == Definition->TracerMesh.ResolveObject())
		{
			bHasTracer = true;
			TestTrue(TEXT("Tracer scale comes from definition"), Mesh->GetRelativeScale3D().Equals(Definition->MeshScale));
		}
	}
	TestTrue(TEXT("Configured tracer mesh is attached"), bHasTracer);

	AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactPoint = FVector::ZeroVector;
	Hit.ImpactNormal = FVector::ForwardVector;
	Bullet->OnSphereHit(Bullet->GetSphereComponent(), Wall, nullptr, FVector::ZeroVector, Hit);
	TArray<UDecalComponent*> Marks = GetMarks();
	TestEqual(TEXT("Blocking hit creates exactly one mark"), Marks.Num(), 1);
	if (Marks.Num() == 1)
	{
		TestTrue(TEXT("World-origin hit is not replaced with projectile center"), Marks[0]->GetComponentLocation().Equals(FVector(0.5f, 0.f, 0.f)));
		TestTrue(TEXT("Projection axis points into wall"), Marks[0]->GetForwardVector().Equals(-FVector::ForwardVector));
		TestTrue(TEXT("Mark remains registered after bullet destruction"), Marks[0]->IsRegistered());
		TestTrue(TEXT("Mark ownership is independent of bullet"), Marks[0]->GetOwner() != Bullet);
	}
	TestTrue(TEXT("Authority destroys the impacted bullet"), Bullet->IsActorBeingDestroyed());

	AAuraBulletPresentationTestActor* OverlapBullet = SpawnBullet();
	if (TestNotNull(TEXT("Overlap bullet spawns"), OverlapBullet))
	{
		OverlapBullet->DeliverOverlap();
		TestEqual(TEXT("Authority overlap does not create a wall mark"), GetMarks().Num(), 1);
	}
	AAuraBulletPresentationTestActor* ClientBullet = SpawnBullet();
	if (TestNotNull(TEXT("Client bullet spawns"), ClientBullet))
	{
		ClientBullet->UseClientRole();
		ClientBullet->DeliverOverlap();
		ClientBullet->OnSphereHit(ClientBullet->GetSphereComponent(), Wall, nullptr, FVector::ZeroVector, Hit);
		TestEqual(TEXT("Predicted client hits create no persistent marks"), GetMarks().Num(), 1);
		ClientBullet->DeliverServerImpact(FVector(0.f, 100.f, 0.f), FVector::UpVector, true);
		TestEqual(TEXT("Server surface impact still creates a mark after predicted hits"), GetMarks().Num(), 2);
		ClientBullet->DeliverServerImpact(FVector(0.f, 100.f, 0.f), FVector::UpVector, true);
		TestEqual(TEXT("Repeated delivery is deduplicated"), GetMarks().Num(), 2);
	}
	Cleanup();
	return true;
}

static void CollectAbilityGraphNodes(const UAuraAbilityActionNode* Node, TArray<const UAuraAbilityActionNode*>& OutNodes)
{
	if (!Node)
	{
		return;
	}

	OutNodes.Add(Node);
	for (const UAuraAbilityActionNode* Child : Node->Children)
	{
		CollectAbilityGraphNodes(Child, OutNodes);
	}
}

static bool IsAuthorityEffectNode(const UAuraAbilityActionNode* Node)
{
	if (!Node)
	{
		return false;
	}

	return Node->NodeClassName == TEXT("ApplyDamage")
		|| Node->NodeClassName == TEXT("CauseDamage")
		|| Node->NodeClassName == TEXT("HitscanTrace")
		|| Node->NodeClassName == TEXT("SpawnProjectile")
		|| Node->NodeClassName == TEXT("SpawnProjectiles")
		|| Node->NodeClassName == TEXT("SpawnShards")
		|| Node->NodeClassName == TEXT("SpawnBeamVisuals")
		|| Node->NodeClassName == TEXT("MulticastGunFX")
		|| Node->NodeClassName == TEXT("ElectrocuteBeam");
}

static bool MontageContainsGameplayEventTagForTest(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
	if (!Montage || !EventTag.IsValid())
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UObject* NotifyObject = NotifyEvent.Notify
			? static_cast<const UObject*>(NotifyEvent.Notify)
			: static_cast<const UObject*>(NotifyEvent.NotifyStateClass);
		if (!NotifyObject)
		{
			continue;
		}

		const FStructProperty* EventTagProperty = CastField<FStructProperty>(NotifyObject->GetClass()->FindPropertyByName(TEXT("EventTag")));
		if (!EventTagProperty || EventTagProperty->Struct != FGameplayTag::StaticStruct())
		{
			continue;
		}

		const FGameplayTag* AuthoredTag = EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(NotifyObject);
		if (AuthoredTag && *AuthoredTag == EventTag)
		{
			return true;
		}
	}

	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraProjectileDefinitionsTest,
	"Aura.Projectiles.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraProjectileDefinitionsTest::RunTest(const FString& Parameters)
{
	FAuraGameplayConfig::ResetForTests();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("Shipped gameplay definitions validate: %s"), *Error), FAuraGameplayConfig::ValidateAll(Error))) return false;
	TestEqual(TEXT("All JSON files are parsed once"), FAuraGameplayConfig::GetLoadCount(), 1);
	FAuraAttributeDefaults AttributeDefaults;
	TestTrue(TEXT("Shared secondary/resistance defaults validate"), FAuraGameplayConfig::GetAttributeDefaults(AttributeDefaults, Error));
	TestEqual(TEXT("All secondary and resistance defaults are present"), AttributeDefaults.Magnitudes.Num(), 14);

	const FAuraProjectileDefinition* FireBolt = FAuraGameplayConfig::FindProjectile(TEXT("fireBolt"));
	const FAuraProjectileDefinition* FireBall = FAuraGameplayConfig::FindProjectile(TEXT("fireBall"));
	const FAuraProjectileDefinition* FireGunBullet = FAuraGameplayConfig::FindProjectile(TEXT("fireGunBullet"));
	TestNotNull(TEXT("fireBolt definition exists"), FireBolt);
	TestNotNull(TEXT("fireBall definition exists"), FireBall);
	TestNotNull(TEXT("fireGunBullet definition exists"), FireGunBullet);
	if (!FireBolt || !FireBall || !FireGunBullet) return false;
	TestEqual(TEXT("FireBolt uses native projectile class"), FireBolt->NativeClass.Get(), AAuraProjectile::StaticClass());
	TestEqual(TEXT("FireBolt measured speed retained"), FireBolt->InitialSpeed, 650.f);
	TestTrue(TEXT("FireBolt measured radius retained"), FMath::IsNearlyEqual(FireBolt->CollisionRadius, 32.235878f));
	TestEqual(TEXT("FireBall uses native fireball class"), FireBall->NativeClass.Get(), AAuraFireBall::StaticClass());
	TestEqual(TEXT("FireBall outbound distance retained"), FireBall->OutboundDistance, 800.f);
	TestEqual(TEXT("FireBall return threshold retained"), FireBall->ReturnDistance, 150.f);
	TestEqual(TEXT("FireGunBullet uses native bullet class"), FireGunBullet->NativeClass.Get(), AAuraBullet::StaticClass());
	TestEqual(TEXT("FireGunBullet measured speed retained"), FireGunBullet->InitialSpeed, 550.f);
	TestEqual(TEXT("FireGunBullet measured radius retained"), FireGunBullet->CollisionRadius, 15.f);
	TestNull(TEXT("FireBall no longer exposes Blueprint timeline event"), AAuraFireBall::StaticClass()->FindFunctionByName(TEXT("StartOutgoingTimeline")));
	TestEqual(TEXT("Repeated lookup does not reparse config"), FAuraGameplayConfig::GetLoadCount(), 1);

	for (const TCHAR* RelativeXml : { TEXT("AbilityDefinitions/FireBolt.xml"), TEXT("AbilityDefinitions/FireBlast.xml") })
	{
		FString Xml;
		TestTrue(RelativeXml, FFileHelper::LoadFileToString(Xml, *FPaths::Combine(FPaths::ProjectContentDir(), RelativeXml)));
		TestFalse(FString::Printf(TEXT("%s contains no Blueprint-generated projectile class"), RelativeXml), Xml.Contains(TEXT("BP_FireBolt_C")) || Xml.Contains(TEXT("BP_FireBall_C")));
		TestTrue(FString::Printf(TEXT("%s uses ProjectileDefinition"), RelativeXml), Xml.Contains(TEXT("ProjectileDefinition")));
	}

	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.World()) { World = Context.World(); break; }
	if (!TestNotNull(TEXT("World available for native projectile fixture"), World)) return false;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraProjectile* Projectile = World->SpawnActor<AAuraProjectile>(AAuraProjectile::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("Native projectile spawns"), Projectile)) return false;
	TestTrue(TEXT("Native projectile accepts fireBolt config"), Projectile->ConfigureFromDefinition(TEXT("fireBolt")));
	TestTrue(TEXT("Configured collision radius"), FMath::IsNearlyEqual(Projectile->GetSphereComponent()->GetUnscaledSphereRadius(), FireBolt->CollisionRadius));
	TestEqual(TEXT("Configured WorldStatic blocks"), Projectile->GetSphereComponent()->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
	TestEqual(TEXT("Configured initial speed"), Projectile->ProjectileMovement->InitialSpeed, 650.f);

	// Presentation regression: loading the native definition must resolve every
	// FireBolt visual/audio dependency, and BeginPlay must attach the flight trail.
	TestTrue(TEXT("FireBolt flight trail path is configured"), !FireBolt->FlightTrail.IsNull());
	TestTrue(TEXT("FireBolt impact effect path is configured"), !FireBolt->ImpactEffect.IsNull());
	TestTrue(TEXT("FireBolt impact sound path is configured"), !FireBolt->ImpactSound.IsNull());
	TestTrue(TEXT("FireBolt looping sound path is configured"), !FireBolt->LoopingSound.IsNull());
	UNiagaraSystem* FireBoltFlightTrail = Cast<UNiagaraSystem>(FireBolt->FlightTrail.TryLoad());
	UNiagaraSystem* FireBoltImpactEffect = Cast<UNiagaraSystem>(FireBolt->ImpactEffect.TryLoad());
	USoundBase* FireBoltImpactSound = Cast<USoundBase>(FireBolt->ImpactSound.TryLoad());
	USoundBase* FireBoltLoopingSound = Cast<USoundBase>(FireBolt->LoopingSound.TryLoad());
	TestNotNull(TEXT("FireBolt flight trail asset loads as Niagara"), FireBoltFlightTrail);
	TestNotNull(TEXT("FireBolt impact effect asset loads as Niagara"), FireBoltImpactEffect);
	TestNotNull(TEXT("FireBolt impact sound asset loads as sound"), FireBoltImpactSound);
	TestNotNull(TEXT("FireBolt looping sound asset loads as sound"), FireBoltLoopingSound);
	TestTrue(TEXT("FireBolt flight trail is a non-empty Niagara system"), FireBoltFlightTrail && !FireBoltFlightTrail->GetPathName().IsEmpty());
	TestTrue(TEXT("FireBolt impact effect is a non-empty Niagara system"), FireBoltImpactEffect && !FireBoltImpactEffect->GetPathName().IsEmpty());
	TestTrue(TEXT("FireBolt impact sound has a valid object path"), FireBoltImpactSound && !FireBoltImpactSound->GetPathName().IsEmpty());
	TestTrue(TEXT("FireBolt looping sound has a valid object path"), FireBoltLoopingSound && !FireBoltLoopingSound->GetPathName().IsEmpty());

	// BeginPlay is only dispatched by a running game world. The asset/type checks
	// above remain valid in commandlet/editor fixture worlds where it is not safe
	// to call DispatchBeginPlay manually.
	if (World->HasBegunPlay())
	{
		TestNotNull(TEXT("FireBolt flight trail component attaches on BeginPlay"), Projectile->FindComponentByClass<UNiagaraComponent>());
	}
	Projectile->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraPickupDefinitionsTest,
	"Aura.Buffs.PickupDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraPickupDefinitionsTest::RunTest(const FString& Parameters)
{
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("Cross-file pickup definitions validate: %s"), *Error), FAuraGameplayConfig::ValidateAll(Error))) return false;
	for (const TCHAR* Name : { TEXT("healthPotion"), TEXT("manaPotion"), TEXT("healthCrystal"), TEXT("manaCrystal"), TEXT("fireArea"), TEXT("testAttribute") })
	{
		const FAuraPickupDefinition* Definition = FAuraGameplayConfig::FindPickup(FName(Name));
		TestNotNull(FString::Printf(TEXT("Pickup %s exists"), Name), Definition);
		if (Definition)
		{
			TestEqual(FString::Printf(TEXT("Pickup %s is native"), Name), Definition->NativeClass.Get(), AAuraEffectActor::StaticClass());
			TestNotNull(FString::Printf(TEXT("Pickup %s effect resolves"), Name), FAuraGameplayConfig::FindPickupEffect(Definition->EffectName));
		}
	}
	TestEqual(TEXT("Four data-driven loot rows"), FAuraGameplayConfig::GetLootDefinitions().Num(), 4);

	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.World()) { World = Context.World(); break; }
	if (!TestNotNull(TEXT("World available for native pickup fixture"), World)) return false;
	AAuraEffectActor* Health = AAuraEffectActor::SpawnConfiguredPickup(World, TEXT("healthPotion"), FTransform::Identity, 3.f);
	if (!TestNotNull(TEXT("Native health potion spawns"), Health)) return false;
	TestEqual(TEXT("Stable definition name retained"), Health->PickupDefinitionName, FName(TEXT("healthPotion")));
	TestEqual(TEXT("Health potion collision is enabled"), Health->GetSphereCollision()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestNotNull(TEXT("Health potion presentation mesh loads"), Health->GetPickupMesh()->GetStaticMesh().Get());
	Health->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraStrictAbilityDefinitionParsingTest,
	"Aura.AbilityGraph.StrictDefinitionParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraStrictAbilityDefinitionParsingTest::RunTest(const FString& Parameters)
{
	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("Transient ability definition created"), Definition)) return false;

	const FString InvalidNumber = TEXT("<ability name=\"Strict\" abilityTag=\"Abilities.Fire.FireBolt\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\"><damage type=\"Damage.Fire\" base=\"not-a-number\"/><graph><node class=\"Sequence\"/></graph></ability>");
	AddExpectedError(TEXT("invalid numeric attribute 'base=not-a-number'"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Invalid numeric XML is rejected"), Definition->LoadFromXML(InvalidNumber));

	const FString InvalidTag = TEXT("<ability name=\"Strict\" abilityTag=\"Abilities.Missing\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\"><graph><node class=\"Sequence\"/></graph></ability>");
	AddExpectedError(TEXT("has no registered abilityTag"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Unregistered ability tag is rejected"), Definition->LoadFromXML(InvalidTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraElectrocuteRetriableDefinitionTest,
	"Aura.AbilityGraph.ElectrocuteRetriable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraElectrocuteRetriableDefinitionTest::RunTest(const FString& Parameters)
{
	const FString XmlPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions/Electrocute.xml"));
	FString Xml;
	if (!TestTrue(TEXT("Electrocute definition is readable"), FFileHelper::LoadFileToString(Xml, *XmlPath)))
	{
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("Electrocute definition object created"), Definition)
		|| !TestTrue(TEXT("Electrocute definition parses"), Definition->LoadFromXML(Xml)))
	{
		return false;
	}

	TestEqual(TEXT("Electrocute remains assigned to Num.3"), Definition->InputTag, FGameplayTag::RequestGameplayTag(TEXT("InputTag.3")));
	if (!TestNotNull(TEXT("Electrocute graph root exists"), Definition->RootNode.Get()))
	{
		return false;
	}

	const UWaitForMontageEventNode* MontageEventNode = nullptr;
	for (const UAuraAbilityActionNode* Child : Definition->RootNode->Children)
	{
		if (const UWaitForMontageEventNode* Candidate = Cast<UWaitForMontageEventNode>(Child))
		{
			MontageEventNode = Candidate;
			break;
		}
	}

	if (!TestNotNull(TEXT("Electrocute contains its montage-event wait"), MontageEventNode))
	{
		return false;
	}

	TestEqual(TEXT("Electrocute waits for the expected montage event"), MontageEventNode->EventTag, FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Electrocute")));
	TestTrue(TEXT("Electrocute montage wait has a finite recovery timeout"), MontageEventNode->Timeout > 0.f);
	TestEqual(TEXT("Electrocute montage wait uses the five-second recovery timeout"), MontageEventNode->Timeout, 5.f);
	TestTrue(TEXT("Electrocute montage wait has an authority fallback delay"), MontageEventNode->AuthorityFallbackDelay > 0.f);
	TestEqual(TEXT("Electrocute uses the configured authority fallback delay"), MontageEventNode->AuthorityFallbackDelay, 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraFireBoltAuthorityFallbackDefinitionTest,
	"Aura.AbilityGraph.FireBoltAuthorityFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraFireBoltAuthorityFallbackDefinitionTest::RunTest(const FString& Parameters)
{
	const FString XmlPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions/FireBolt.xml"));
	FString Xml;
	if (!TestTrue(TEXT("FireBolt definition is readable"), FFileHelper::LoadFileToString(Xml, *XmlPath)))
	{
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("FireBolt definition object created"), Definition)
		|| !TestTrue(TEXT("FireBolt definition parses"), Definition->LoadFromXML(Xml)))
	{
		return false;
	}

	TestEqual(TEXT("FireBolt remains assigned to LMB"), Definition->InputTag, FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB")));
	if (!TestNotNull(TEXT("FireBolt graph root exists"), Definition->RootNode.Get()))
	{
		return false;
	}

	const UWaitForMontageEventNode* MontageEventNode = nullptr;
	for (const UAuraAbilityActionNode* Child : Definition->RootNode->Children)
	{
		if (const UWaitForMontageEventNode* Candidate = Cast<UWaitForMontageEventNode>(Child))
		{
			MontageEventNode = Candidate;
			break;
		}
	}

	if (!TestNotNull(TEXT("FireBolt contains its montage-event wait"), MontageEventNode))
	{
		return false;
	}

	TestEqual(TEXT("FireBolt waits for the expected montage event"), MontageEventNode->EventTag, FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.FireBolt")));
	TestTrue(TEXT("FireBolt montage wait has a finite recovery timeout"), MontageEventNode->Timeout > 0.f);
	TestTrue(TEXT("FireBolt montage wait has an authority fallback delay"), MontageEventNode->AuthorityFallbackDelay > 0.f);
	TestEqual(TEXT("FireBolt uses the configured authority fallback delay"), MontageEventNode->AuthorityFallbackDelay, 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraArcaneShardsAuthorityFallbackDefinitionTest,
	"Aura.AbilityGraph.ArcaneShardsAuthorityFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraArcaneShardsAuthorityFallbackDefinitionTest::RunTest(const FString& Parameters)
{
	const FString XmlPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions/ArcaneShards.xml"));
	FString Xml;
	if (!TestTrue(TEXT("ArcaneShards definition is readable"), FFileHelper::LoadFileToString(Xml, *XmlPath)))
	{
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("ArcaneShards definition object created"), Definition)
		|| !TestTrue(TEXT("ArcaneShards definition parses"), Definition->LoadFromXML(Xml)))
	{
		return false;
	}

	TestEqual(TEXT("ArcaneShards remains assigned to Num.2"), Definition->InputTag, FGameplayTag::RequestGameplayTag(TEXT("InputTag.2")));
	if (!TestNotNull(TEXT("ArcaneShards graph root exists"), Definition->RootNode.Get()))
	{
		return false;
	}

	const UWaitForMontageEventNode* MontageEventNode = nullptr;
	for (const UAuraAbilityActionNode* Child : Definition->RootNode->Children)
	{
		if (const UWaitForMontageEventNode* Candidate = Cast<UWaitForMontageEventNode>(Child))
		{
			MontageEventNode = Candidate;
			break;
		}
	}

	if (!TestNotNull(TEXT("ArcaneShards contains its montage-event wait"), MontageEventNode))
	{
		return false;
	}

	TestEqual(TEXT("ArcaneShards waits for the expected montage event"), MontageEventNode->EventTag, FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.ArcaneShards")));
	TestTrue(TEXT("ArcaneShards montage wait has a finite recovery timeout"), MontageEventNode->Timeout > 0.f);
	TestTrue(TEXT("ArcaneShards montage wait has an authority fallback delay"), MontageEventNode->AuthorityFallbackDelay > 0.f);
	TestEqual(TEXT("ArcaneShards uses the configured authority fallback delay"), MontageEventNode->AuthorityFallbackDelay, 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraFireBlastAuthorityFallbackDefinitionTest,
	"Aura.AbilityGraph.FireBlastAuthorityFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraFireBlastAuthorityFallbackDefinitionTest::RunTest(const FString& Parameters)
{
	const FString XmlPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions/FireBlast.xml"));
	FString Xml;
	if (!TestTrue(TEXT("FireBlast definition is readable"), FFileHelper::LoadFileToString(Xml, *XmlPath)))
	{
		return false;
	}

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("FireBlast definition object created"), Definition)
		|| !TestTrue(TEXT("FireBlast definition parses"), Definition->LoadFromXML(Xml)))
	{
		return false;
	}

	TestEqual(TEXT("FireBlast remains assigned to Num.1"), Definition->InputTag, FGameplayTag::RequestGameplayTag(TEXT("InputTag.1")));
	if (!TestNotNull(TEXT("FireBlast graph root exists"), Definition->RootNode.Get()))
	{
		return false;
	}

	const UPlayMontageNode* PlayMontageNode = nullptr;
	const UWaitForMontageEventNode* MontageEventNode = nullptr;
	int32 SpawnProjectilesIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Definition->RootNode->Children.Num(); ++Index)
	{
		const UAuraAbilityActionNode* Child = Definition->RootNode->Children[Index];
		if (const UPlayMontageNode* Candidate = Cast<UPlayMontageNode>(Child))
		{
			PlayMontageNode = Candidate;
		}
		else if (const UWaitForMontageEventNode* EventCandidate = Cast<UWaitForMontageEventNode>(Child))
		{
			MontageEventNode = EventCandidate;
		}
		else if (Child && Child->NodeClassName == TEXT("SpawnProjectiles"))
		{
			SpawnProjectilesIndex = Index;
		}
	}

	if (!TestNotNull(TEXT("FireBlast contains a PlayMontage node"), PlayMontageNode)
		|| !TestNotNull(TEXT("FireBlast contains its montage-event wait"), MontageEventNode))
	{
		return false;
	}

	TestEqual(TEXT("FireBlast uses the dedicated cast montage"), PlayMontageNode->MontagePath,
		FString(TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBlast.AM_Cast_FireBlast")));
	TestNotNull(TEXT("FireBlast cast montage loads"), PlayMontageNode->Montage.Get());
	TestEqual(TEXT("FireBlast waits for the expected montage event"), MontageEventNode->EventTag,
		FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.FireBlast")));
	TestEqual(TEXT("FireBlast montage wait uses the five-second recovery timeout"), MontageEventNode->Timeout, 5.f);
	TestEqual(TEXT("FireBlast uses the configured authority fallback delay"), MontageEventNode->AuthorityFallbackDelay, 0.35f);
	TestTrue(TEXT("FireBlast montage event is authored on the cast montage"),
		PlayMontageNode->Montage && MontageContainsGameplayEventTagForTest(PlayMontageNode->Montage, MontageEventNode->EventTag));
	TestEqual(TEXT("FireBlast spawns projectiles after the montage wait"), SpawnProjectilesIndex, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraPlayerSkillAuthoritySafetyTest,
	"Aura.Abilities.PlayerSkillAuthoritySafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraPlayerSkillAuthoritySafetyTest::RunTest(const FString& Parameters)
{
	const FString AbilityDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"));
	TArray<FString> AbilityFiles;
	IFileManager::Get().FindFiles(AbilityFiles, *(AbilityDirectory / TEXT("*.xml")), true, false);
	if (!TestTrue(TEXT("Ability definition directory contains XML files"), AbilityFiles.Num() > 0))
	{
		return false;
	}

	const TArray<FName> ExpectedPlayerSkills = {
		FName(TEXT("FireBlast")),
		FName(TEXT("ArcaneShards")),
		FName(TEXT("Electrocute")),
		FName(TEXT("FireBolt")),
		FName(TEXT("FireGun"))
	};
	const TArray<FName> ExpectedInputTags = {
		FName(TEXT("InputTag.1")),
		FName(TEXT("InputTag.2")),
		FName(TEXT("InputTag.3")),
		FName(TEXT("InputTag.LMB"))
	};

	TSet<FName> FoundPlayerSkills;
	TSet<FName> FoundInputTags;
	int32 InputBoundSkillCount = 0;

	for (const FString& AbilityFile : AbilityFiles)
	{
		const FString AbilityPath = FPaths::Combine(AbilityDirectory, AbilityFile);
		FString Xml;
		if (!TestTrue(FString::Printf(TEXT("Ability XML is readable: %s"), *AbilityFile), FFileHelper::LoadFileToString(Xml, *AbilityPath)))
		{
			continue;
		}

		UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
		if (!TestNotNull(FString::Printf(TEXT("Ability definition object created: %s"), *AbilityFile), Definition)
			|| !TestTrue(FString::Printf(TEXT("Ability XML parses: %s"), *AbilityFile), Definition->LoadFromXML(Xml)))
		{
			continue;
		}

		// Enemy definitions intentionally have no inputTag and are not player skills.
		if (!Definition->InputTag.IsValid())
		{
			continue;
		}

		++InputBoundSkillCount;
		const FString SkillName = Definition->AbilityName.ToString();
		const FName SkillNameKey = Definition->AbilityName;
		const FName InputTagName = Definition->InputTag.GetTagName();
		FoundPlayerSkills.Add(SkillNameKey);
		FoundInputTags.Add(InputTagName);

		TestTrue(FString::Printf(TEXT("Input-bound skill uses a registered gameplay input tag: %s (%s)"), *SkillName, *InputTagName.ToString()), Definition->InputTag.IsValid());
		if (!TestNotNull(FString::Printf(TEXT("Input-bound skill graph root exists: %s"), *SkillName), Definition->RootNode.Get()))
		{
			continue;
		}

		TArray<const UAuraAbilityActionNode*> Nodes;
		CollectAbilityGraphNodes(Definition->RootNode.Get(), Nodes);
		int32 WaitCount = 0;
		int32 WaitIndex = INDEX_NONE;
		int32 PlayMontageIndex = INDEX_NONE;
		int32 AuthorityEffectIndex = INDEX_NONE;
		int32 AuthorityEffectAfterWaitIndex = INDEX_NONE;
		const UPlayMontageNode* PlayMontageNode = nullptr;
		const UWaitForMontageEventNode* MontageEventNode = nullptr;

		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			const UAuraAbilityActionNode* Node = Nodes[NodeIndex];
			if (!Node)
			{
				continue;
			}

			if (const UPlayMontageNode* Candidate = Cast<UPlayMontageNode>(Node))
			{
				if (PlayMontageIndex == INDEX_NONE)
				{
					PlayMontageIndex = NodeIndex;
					PlayMontageNode = Candidate;
				}
			}

			if (const UWaitForMontageEventNode* Candidate = Cast<UWaitForMontageEventNode>(Node))
			{
				++WaitCount;
				if (WaitIndex == INDEX_NONE)
				{
					WaitIndex = NodeIndex;
					MontageEventNode = Candidate;
				}
			}

			if (IsAuthorityEffectNode(Node))
			{
				if (AuthorityEffectIndex == INDEX_NONE)
				{
					AuthorityEffectIndex = NodeIndex;
				}
				if (WaitIndex != INDEX_NONE && NodeIndex > WaitIndex && AuthorityEffectAfterWaitIndex == INDEX_NONE)
				{
					AuthorityEffectAfterWaitIndex = NodeIndex;
				}
			}
		}

		if (WaitCount > 0)
		{
			TestEqual(FString::Printf(TEXT("Montage-gated skill has exactly one event wait: %s"), *SkillName), WaitCount, 1);
			if (TestNotNull(FString::Printf(TEXT("Montage-gated skill event wait exists: %s"), *SkillName), MontageEventNode))
			{
				TestTrue(FString::Printf(TEXT("Montage event tag is valid: %s"), *SkillName), MontageEventNode->EventTag.IsValid());
				TestTrue(FString::Printf(TEXT("Montage wait has a finite timeout: %s"), *SkillName), MontageEventNode->Timeout > 0.f);
				TestTrue(FString::Printf(TEXT("Montage wait has an authority fallback: %s"), *SkillName), MontageEventNode->AuthorityFallbackDelay > 0.f);
				TestTrue(FString::Printf(TEXT("Authority fallback precedes timeout: %s"), *SkillName), MontageEventNode->AuthorityFallbackDelay < MontageEventNode->Timeout);
				TestNotNull(FString::Printf(TEXT("Montage-gated skill has a loaded montage asset: %s"), *SkillName), PlayMontageNode ? PlayMontageNode->Montage.Get() : nullptr);
				TestTrue(FString::Printf(TEXT("Montage authors the configured gameplay event: %s"), *SkillName), PlayMontageNode && MontageContainsGameplayEventTagForTest(PlayMontageNode->Montage, MontageEventNode->EventTag));
			}
			TestTrue(FString::Printf(TEXT("Montage-gated skill has a PlayMontage node: %s"), *SkillName), PlayMontageIndex != INDEX_NONE);
			if (PlayMontageIndex != INDEX_NONE && WaitIndex != INDEX_NONE)
			{
				TestEqual(FString::Printf(TEXT("Montage event wait immediately follows PlayMontage: %s"), *SkillName), WaitIndex, PlayMontageIndex + 1);
			}
			TestTrue(FString::Printf(TEXT("Montage-gated skill has an authority effect after its wait: %s"), *SkillName), AuthorityEffectAfterWaitIndex != INDEX_NONE);
		}
		else
		{
			// A player skill without a montage is still required to expose a reachable
			// authority effect node so activation cannot become presentation-only.
			TestTrue(FString::Printf(TEXT("Direct skill has an authority effect node: %s"), *SkillName), AuthorityEffectIndex != INDEX_NONE);
		}
	}

	TestTrue(TEXT("All current input-bound player skills were discovered"), InputBoundSkillCount >= ExpectedPlayerSkills.Num());
	for (const FName& SkillName : ExpectedPlayerSkills)
	{
		TestTrue(FString::Printf(TEXT("Player skill definition is covered: %s"), *SkillName.ToString()), FoundPlayerSkills.Contains(SkillName));
	}
	for (const FName& InputTag : ExpectedInputTags)
	{
		TestTrue(FString::Printf(TEXT("Player input tag is represented by a skill definition: %s"), *InputTag.ToString()), FoundInputTags.Contains(InputTag));
	}

	return true;
}

#endif
