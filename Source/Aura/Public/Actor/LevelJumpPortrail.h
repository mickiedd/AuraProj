// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelJumpPortrail.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWorld;

UCLASS()
class AURA_API ALevelJumpPortrail : public AActor
{
	GENERATED_BODY()

public:
	ALevelJumpPortrail();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail")
	FName DestinationPlayerStartTag = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail")
	FString DestinationMapAssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail")
	TSoftObjectPtr<UWorld> DestinationMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail")
	FString DestinationServer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail")
	bool bOneShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail|Trigger", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float TriggerSphereRadius = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortrail|Trigger", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float MinTriggerSphereRadius = 120.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "JumpPortrail")
	void OnJumpPortrailTriggered(AActor* TriggeringActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortrail")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortrail")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortrail")
	TObjectPtr<USphereComponent> TriggerSphere;

private:
	bool bTriggered = false;
};
