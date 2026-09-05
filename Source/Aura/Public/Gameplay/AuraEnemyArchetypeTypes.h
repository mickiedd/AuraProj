// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraEnemyArchetypeTypes.generated.h"

/** Archetype-owned attack references; concrete shapes remain opaque and frozen at Windup. */
USTRUCT(BlueprintType)
struct AURA_API FAuraEnemyAttackDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName AttackShapeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName TelegraphProfileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName CounterVerb = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	float WindupSeconds = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	float RecoverySeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bShapeFrozenDuringWindup = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bConsumesHeavyAdmission = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bInterruptible = false;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEnemySupportFieldDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	float DamageReductionFraction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	float RangeUnits = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	int32 MaxAllies = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	FName OwnershipPolicy = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bInterruptible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bRemoveOnRangeDeparture = true;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Enemy")
	bool bRemoveOnOwnerDeath = true;
};
