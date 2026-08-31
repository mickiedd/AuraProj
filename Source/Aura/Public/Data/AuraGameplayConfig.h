// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AAuraProjectile;
class AAuraEffectActor;

enum class EAuraConfiguredCollisionShape : uint8
{
	Sphere,
	Box,
	Capsule
};

struct AURA_API FAuraProjectileDefinition
{
	FName Name;
	TSubclassOf<AAuraProjectile> NativeClass;
	float CollisionRadius = 15.f;
	float InitialSpeed = 550.f;
	float MaxSpeed = 550.f;
	float GravityScale = 0.f;
	float LifeSpan = 15.f;
	ECollisionResponse WorldStaticResponse = ECR_Block;
	FSoftObjectPath Mesh;
	FVector MeshScale = FVector(0.3f);
	FSoftObjectPath TracerMesh;
	FSoftObjectPath FlightTrail;
	FSoftObjectPath FlightParticle;
	FSoftObjectPath ImpactEffect;
	FSoftObjectPath ImpactParticle;
	FSoftObjectPath ImpactSound;
	FSoftObjectPath LoopingSound;
	FSoftObjectPath SurfaceMarkMaterial;
	float SurfaceMarkSize = 8.f;
	float SurfaceMarkLifeSpan = 30.f;
	float OutboundDistance = 0.f;
	float OutboundDuration = 0.f;
	float ReturnSpeed = 0.f;
	float ReturnDistance = 0.f;
};

struct AURA_API FAuraPickupEffectDefinition
{
	FName Name;
	FString DurationType;
	float Duration = 0.f;
	float Period = 0.f;
	bool bExecuteOnApplication = true;
	TMap<FGameplayTag, float> Magnitudes;
	FGameplayTagContainer AssetTags;
};

struct AURA_API FAuraAttributeDefaults
{
	TMap<FGameplayTag, float> Magnitudes;
};

struct AURA_API FAuraPickupDefinition
{
	FName Name;
	TSubclassOf<AAuraEffectActor> NativeClass;
	FName EffectName;
	bool bApplyOnEndOverlap = false;
	bool bRemoveOnEndOverlap = false;
	bool bDestroyOnApplication = false;
	bool bApplyToEnemies = false;
	float ActorLevel = 1.f;
	EAuraConfiguredCollisionShape CollisionShape = EAuraConfiguredCollisionShape::Sphere;
	FVector CollisionSize = FVector(32.f);
	FSoftObjectPath Mesh;
	FVector MeshOffset = FVector::ZeroVector;
	FRotator MeshRotation = FRotator::ZeroRotator;
	FVector MeshScale = FVector::OneVector;
	TArray<FSoftObjectPath> Materials;
	bool bRotates = false;
	float RotationRate = 45.f;
	bool bSinusoidalMovement = false;
	float SineAmplitude = 0.f;
	float SinePeriodConstant = 1.f;
	FSoftObjectPath Vfx;
	FVector VfxOffset = FVector::ZeroVector;
	FSoftObjectPath SecondaryVfx;
	FVector SecondaryVfxOffset = FVector::ZeroVector;
};

struct AURA_API FAuraLootDefinition
{
	FName PickupDefinition;
	float ChanceToSpawn = 0.f;
	int32 MaxNumberToSpawn = 0;
	bool bLootLevelOverride = true;
};

/** Immutable, process-wide cache for loose JSON gameplay definitions. */
class AURA_API FAuraGameplayConfig
{
public:
	static const FAuraProjectileDefinition* FindProjectile(FName Name);
	static const FAuraPickupDefinition* FindPickup(FName Name);
	static const FAuraPickupEffectDefinition* FindPickupEffect(FName Name);
	static bool GetAttributeDefaults(FAuraAttributeDefaults& OutDefaults, FString& OutError);
	static const TArray<FAuraLootDefinition>& GetLootDefinitions();
	static bool ValidateAll(FString& OutError);
	static int32 GetLoadCount();
	static void ResetForTests();
};
