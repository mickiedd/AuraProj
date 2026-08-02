// Copyright Druid Mechanics


#include "UI/WidgetController/AuraWidgetController.h"

#include "Player/AuraPlayerController.h"
#include "Player/AuraPlayerState.h"
#include "Aura/AuraLogChannels.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UAuraWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController = WCParams.PlayerController;
	PlayerState = WCParams.PlayerState;
	AbilitySystemComponent = WCParams.AbilitySystemComponent;
	AttributeSet = WCParams.AttributeSet;
	const UObject* WorldContext = AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor()
		? static_cast<const UObject*>(AbilitySystemComponent->GetAvatarActor())
		: static_cast<const UObject*>(PlayerController);
	AbilityInfo = UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(WorldContext);
}

void UAuraWidgetController::BroadcastInitialValues()
{
	
}

void UAuraWidgetController::BindCallbacksToDependencies()
{
	
}

void UAuraWidgetController::BroadcastAbilityInfo()
{
	if (!GetAuraASC()->bStartupAbilitiesGiven)
	{
		UE_LOG(LogAura, Log, TEXT("BroadcastAbilityInfo skipped for widget controller %s because startup abilities are not marked as given yet"), *GetNameSafe(this));
		return;
	}

	if (AbilityInfo == nullptr)
	{
		UE_LOG(LogAura, Error, TEXT("BroadcastAbilityInfo failed for widget controller %s because AbilityInfo.json is unavailable"), *GetNameSafe(this));
		return;
	}

	int32 BroadcastCount = 0;

	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this, &BroadcastCount](const FGameplayAbilitySpec& AbilitySpec)
	{
		FAuraAbilityInfo Info = AuraAbilitySystemComponent->GetRuntimeAbilityInfoForSpec(AbilitySpec);
		Info.StatusTag = AuraAbilitySystemComponent->GetStatusFromSpec(AbilitySpec);
		AbilityInfoDelegate.Broadcast(Info);
		UE_LOG(LogAura, Log, TEXT("BroadcastAbilityInfo widget=%s ability=%s status=%s slot=%s level=%d"),
			*GetNameSafe(this),
			*Info.AbilityTag.ToString(),
			*Info.StatusTag.ToString(),
			Info.InputTag.IsValid() ? *Info.InputTag.ToString() : TEXT("None"),
			AbilitySpec.Level);
		BroadcastCount++;
	});
	GetAuraASC()->ForEachAbility(BroadcastDelegate);

	UE_LOG(LogAura, Log, TEXT("BroadcastAbilityInfo completed for widget controller %s. BroadcastCount=%d"), *GetNameSafe(this), BroadcastCount);
}

AAuraPlayerController* UAuraWidgetController::GetAuraPC()
{
	if (AuraPlayerController == nullptr)
	{
		AuraPlayerController = Cast<AAuraPlayerController>(PlayerController);
	}
	return AuraPlayerController;
}

AAuraPlayerState* UAuraWidgetController::GetAuraPS()
{
	if (AuraPlayerState == nullptr)
	{
		AuraPlayerState = Cast<AAuraPlayerState>(PlayerState);
	}
	return AuraPlayerState;
}

UAuraAbilitySystemComponent* UAuraWidgetController::GetAuraASC()
{
	if (AuraAbilitySystemComponent == nullptr)
	{
		AuraAbilitySystemComponent = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	}
	return AuraAbilitySystemComponent;
}

UAuraAttributeSet* UAuraWidgetController::GetAuraAS()
{
	if (AuraAttributeSet == nullptr)
	{
		AuraAttributeSet = Cast<UAuraAttributeSet>(AttributeSet);
	}
	return AuraAttributeSet;
}
