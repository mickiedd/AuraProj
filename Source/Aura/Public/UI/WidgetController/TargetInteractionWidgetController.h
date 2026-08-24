// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetController/AuraWidgetController.h"
#include "Combat/AuraTargetingTypes.h"
#include "TargetInteractionWidgetController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAuraTargetPreviewSignature, const FAuraTargetDescriptor&, Descriptor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAuraTargetPreviewClearedSignature);

UCLASS(BlueprintType, Blueprintable)
class AURA_API UTargetInteractionWidgetController : public UAuraWidgetController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Target Preview")
	bool BuildPreview(const AActor* Observer, AActor* Target, FAuraTargetDescriptor& OutDescriptor) const;
	UFUNCTION(BlueprintCallable, Category = "Target Preview")
	void PublishPreview(const FAuraTargetDescriptor& Descriptor) { OnTargetPreview.Broadcast(Descriptor); }
	UFUNCTION(BlueprintCallable, Category = "Target Preview")
	void ClearPreview() { OnTargetPreviewCleared.Broadcast(); }

	UPROPERTY(BlueprintAssignable, Category = "Target Preview") FAuraTargetPreviewSignature OnTargetPreview;
	UPROPERTY(BlueprintAssignable, Category = "Target Preview") FAuraTargetPreviewClearedSignature OnTargetPreviewCleared;
};
