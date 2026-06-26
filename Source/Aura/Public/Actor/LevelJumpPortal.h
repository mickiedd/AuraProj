// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelJumpPortal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWorld;
class APlayerController;

UCLASS()
class AURA_API ALevelJumpPortal : public AActor
{
	GENERATED_BODY()

public:
	ALevelJumpPortal();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	FName DestinationPlayerStartTag = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	FString DestinationMapAssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	TSoftObjectPtr<UWorld> DestinationMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	FString DestinationServer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	FString DestinationServerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal")
	bool bOneShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal|Trigger", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float TriggerSphereRadius = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpPortal|Trigger", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float MinTriggerSphereRadius = 120.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "JumpPortal")
	void OnJumpPortalTriggered(AActor* TriggeringActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JumpPortal")
	TObjectPtr<USphereComponent> TriggerSphere;

private:
	bool bTriggered = false;
};
