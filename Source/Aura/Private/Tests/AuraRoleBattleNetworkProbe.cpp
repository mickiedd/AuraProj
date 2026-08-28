// Copyright Druid Mechanics

#include "Tests/AuraRoleBattleNetworkProbe.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AuraGameplayTags.h"
#include "Character/AuraCivilian.h"
#include "Character/AuraEnemy.h"
#include "Data/AuraGameplayConfig.h"
#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Economy/AuraMerchantComponent.h"
#include "Engine/World.h"
#include "Game/AuraGameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "World/AuraPopulationManager.h"
#include "Aura/AuraLogChannels.h"

bool FAuraRoleBattleNetworkProbe::CollectServerSnapshot(const UWorld* World,
	FAuraRoleBattleNetworkProbeSnapshot& OutSnapshot, FString& OutError)
{
	OutSnapshot = FAuraRoleBattleNetworkProbeSnapshot();
	OutError.Reset();
	if (!World || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Day 19 snapshot requires an authoritative server world.");
		return false;
	}

	OutSnapshot.bAuthorityWorld = true;
	const AAuraGameModeBase* GameMode = World->GetAuthGameMode<AAuraGameModeBase>();
	OutSnapshot.bWorldReady = GameMode && GameMode->IsWorldReadyForPlay();
	OutSnapshot.bPopulationReady = GameMode && GameMode->GetPopulationManager() != nullptr;
	OutSnapshot.bEconomyReady = GameMode && GameMode->GetEconomyRegistry() && GameMode->GetEconomyRegistry()->IsReady();
	OutSnapshot.ConfiguredPopulationSlots = GameMode && GameMode->GetPopulationManager()
		? GameMode->GetPopulationManager()->BuildDebugSnapshot().Slots.Num() : 0;
	OutSnapshot.ConfiguredEnemyRows = GameMode ? GameMode->GetLoadedMonsterSpawnRowCount() : 0;

	TArray<AActor*> Civilians;
	UGameplayStatics::GetAllActorsOfClass(World, AAuraCivilian::StaticClass(), Civilians);
	OutSnapshot.Civilians = Civilians.Num();
	for (AActor* Actor : Civilians)
	{
		const AAuraCivilian* Civilian = Cast<AAuraCivilian>(Actor);
		const UAuraMerchantComponent* Merchant = Civilian ? Civilian->GetMerchantComponent() : nullptr;
		if (Merchant && Merchant->IsMerchantActive()) ++OutSnapshot.ActiveMerchants;
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AAuraEnemy::StaticClass(), Enemies);
	OutSnapshot.Enemies = Enemies.Num();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller) continue;
		++OutSnapshot.PlayerControllers;
		if (Controller->GetNetConnection()) ++OutSnapshot.RemoteClients;
	}

	const FGameplayTag FireGunTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Gun.Fire"), false);
	OutSnapshot.bFireGunConfigured = FireGunTag.IsValid()
		&& UAuraAbilitySystemLibrary::FindAbilityDefinitionByTag(FireGunTag) != nullptr
		&& FAuraGameplayConfig::FindProjectile(TEXT("fireGunBullet")) != nullptr;
	if (!OutSnapshot.HasRequiredFixture())
	{
		OutError = FString::Printf(TEXT("Day 19 fixture is incomplete: world=%d population=%d economy=%d fireGun=%d remotes=%d merchants=%d."),
			OutSnapshot.bWorldReady ? 1 : 0, OutSnapshot.bPopulationReady ? 1 : 0,
			OutSnapshot.bEconomyReady ? 1 : 0, OutSnapshot.bFireGunConfigured ? 1 : 0,
			OutSnapshot.RemoteClients, OutSnapshot.ActiveMerchants);
	}
	return true;
}

void FAuraRoleBattleNetworkProbe::LogServerSnapshot(const FAuraRoleBattleNetworkProbeSnapshot& Snapshot)
{
	UE_LOG(LogAura, Display,
		TEXT("[Day19NetworkProbe][Server] Ready=%d AuthorityWorld=%d WorldReady=%d PopulationReady=%d EconomyReady=%d FireGunConfigured=%d Players=%d RemoteClients=%d Civilians=%d Enemies=%d ActiveMerchants=%d ConfiguredCivilians=%d ConfiguredEnemyRows=%d ReplayCacheLimit=%d SecurityAuthority=1 ReplicationFixture=%d PerformanceFixtureMinimum=%d"),
		Snapshot.HasRequiredFixture() ? 1 : 0,
		Snapshot.bAuthorityWorld ? 1 : 0, Snapshot.bWorldReady ? 1 : 0,
		Snapshot.bPopulationReady ? 1 : 0, Snapshot.bEconomyReady ? 1 : 0,
		Snapshot.bFireGunConfigured ? 1 : 0, Snapshot.PlayerControllers,
		Snapshot.RemoteClients, Snapshot.Civilians, Snapshot.Enemies,
		Snapshot.ActiveMerchants, Snapshot.ConfiguredPopulationSlots,
		Snapshot.ConfiguredEnemyRows, ReplayCacheLimit,
		Snapshot.HasRequiredFixture() ? 1 : 0,
		Snapshot.MeetsPerformanceFixtureMinimum() ? 1 : 0);
}
