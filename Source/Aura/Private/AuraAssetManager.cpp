// Copyright Druid Mechanics


#include "AuraAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "AuraGameplayTags.h"
#include "AuraAttributeGameplayEffect.h"

UAuraAssetManager& UAuraAssetManager::Get()
{
	check(GEngine);

	UAuraAssetManager* AuraAssetManager = Cast<UAuraAssetManager>(GEngine->AssetManager);
	return *AuraAssetManager;
}

void UAuraAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	FAuraGameplayTags::InitializeNativeGameplayTags();

	// This is required to use Target Data!
	UAbilitySystemGlobals::Get().InitGlobalData();

	// The C++ attribute/pickup GameplayEffect CDOs are constructed at module load,
	// before FAuraGameplayTags::InitializeNativeGameplayTags() (above) populated
	// the tag members — so their SetByCaller modifier DataTags were baked as None.
	// That made AssignTagSetByCallerMagnitude unable to bind values, leaving every
	// attribute at 0 (Mana=0/0) and aborting every mana-cost ability at CheckCost.
	// Re-bake the modifiers now that the native tags are registered.
	if (UAuraAttributeGameplayEffect* AttrCDO = UAuraAttributeGameplayEffect::StaticClass()->GetDefaultObject<UAuraAttributeGameplayEffect>())
	{
		AttrCDO->RebuildModifiers();
	}
	if (UAuraAttributeGameplayEffect_Infinite* InfCDO = UAuraAttributeGameplayEffect_Infinite::StaticClass()->GetDefaultObject<UAuraAttributeGameplayEffect_Infinite>())
	{
		InfCDO->RebuildModifiers();
	}
	if (UAuraPickupGameplayEffect* PickCDO = UAuraPickupGameplayEffect::StaticClass()->GetDefaultObject<UAuraPickupGameplayEffect>())
	{
		PickCDO->RebuildModifiers();
	}
}
