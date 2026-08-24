// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interaction/AuraInteractionTypes.h"
#include "AuraInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAuraInteractionResultSignature, int32, ClientRequestId, EAuraInteractionResultCode, ResultCode);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraInteractionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RequestInteraction(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId);

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FAuraInteractionResultSignature OnInteractionResult;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestInteraction(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId);

	UFUNCTION(Client, Reliable)
	void ClientInteractionResult(int32 ClientRequestId, EAuraInteractionResultCode ResultCode);

	void ReturnResult(int32 ClientRequestId, EAuraInteractionResultCode ResultCode);
	int32 LastAcceptedRequestId = 0;
	double LastAcceptedRequestTime = -1.0;
};
