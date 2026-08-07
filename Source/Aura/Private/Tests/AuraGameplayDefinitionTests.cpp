#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Actor/AuraEffectActor.h"
#include "Actor/AuraFireBall.h"
#include "Actor/AuraProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Data/AuraGameplayConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	TestEqual(TEXT("FireGunBullet uses native projectile class"), FireGunBullet->NativeClass.Get(), AAuraProjectile::StaticClass());
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

#endif
