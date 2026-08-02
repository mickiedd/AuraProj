// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "AuraEffectActor.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class UCapsuleComponent;
class UNiagaraComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EEffectApplicationPolicy : uint8
{
	ApplyOnOverlap,
	ApplyOnEndOverlap,
	DoNotApply
};

UENUM(BlueprintType)
enum class EEffectRemovalPolicy : uint8
{
	RemoveOnEndOverlap,
	DoNotRemove
};

UCLASS()
class AURA_API AAuraEffectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAuraEffectActor();
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	bool ConfigureFromDefinition(FName InDefinitionName);
	void SetConfiguredActorLevel(float InActorLevel) { ActorLevel = InActorLevel; }

	UFUNCTION(BlueprintCallable, Category = "Pickup|DataDriven", meta = (WorldContext = "WorldContextObject"))
	static AAuraEffectActor* SpawnConfiguredPickup(UObject* WorldContextObject, FName InDefinitionName, const FTransform& Transform, float InActorLevel = 1.f);
	USphereComponent* GetSphereCollision() const { return SphereCollision; }
	UBoxComponent* GetBoxCollision() const { return BoxCollision; }
	UCapsuleComponent* GetCapsuleCollision() const { return CapsuleCollision; }
	UStaticMeshComponent* GetPickupMesh() const { return PickupMesh; }

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Pickup|DataDriven", meta = (ExposeOnSpawn = true))
	FName PickupDefinitionName;
protected:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite)
	FVector CalculatedLocation;

	UPROPERTY(BlueprintReadWrite)
	FRotator CalculatedRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	bool bRotates = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	float RotationRate = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	bool bSinusoidalMovement = false;

	UFUNCTION(BlueprintCallable)
	void StartSinusoidalMovement();

	UFUNCTION(BlueprintCallable)
	void StartRotation();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	float SineAmplitude = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	float SinePeriodConstant = 1.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup Movement")
	FVector InitialLocation;

	/** Apply a data-driven pickup effect by name (from GameplayEffects.json). Uses C++ UAuraPickupGameplayEffect. */
	UFUNCTION(BlueprintCallable)
	void ApplyDataDrivenEffect(AActor* TargetActor, const FString& EffectName);

	UFUNCTION(BlueprintCallable)
	void OnOverlap(AActor* TargetActor);

	UFUNCTION(BlueprintCallable)
	void OnEndOverlap(AActor* TargetActor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	bool bDestroyOnEffectApplication = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	bool bApplyEffectsToEnemies = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy InstantEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy DurationEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectRemovalPolicy InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;

	// ── Data-driven pickup effects (JSON) ─────────────────────────
	// The name indexes into
	// Content/Config/GameplayEffects.json → "pickupEffects" → <name>.
	// Uses UAuraPickupGameplayEffect (C++) with SetByCaller magnitudes from JSON.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects|DataDriven")
	FString InstantEffectName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects|DataDriven")
	FString DurationEffectName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects|DataDriven")
	FString InfiniteEffectName;

	TMap<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>> ActiveEffectHandles;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Applied Effects")
	float ActorLevel = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<USphereComponent> SphereCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UBoxComponent> BoxCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UCapsuleComponent> CapsuleCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UNiagaraComponent> PickupVfx;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UNiagaraComponent> SecondaryPickupVfx;

private:
	UFUNCTION()
	void OnCollisionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnCollisionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	float RunningTime = 0.f;
	void ItemMovement(float DeltaTime);
};
