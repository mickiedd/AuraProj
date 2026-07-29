// Copyright Druid Mechanics


#include "Actor/AuraEffectActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "AuraGameplayTags.h"
#include "AuraAttributeGameplayEffect.h"
#include "JsonObjectConverter.h"
#include "JsonUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

AAuraEffectActor::AAuraEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
}

void AAuraEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RunningTime += DeltaTime;
	const float SinePeriod = 2 * PI / SinePeriodConstant;
	if (RunningTime > SinePeriod)
	{
		RunningTime = 0.f;
	}
	ItemMovement(DeltaTime);
}

void AAuraEffectActor::ItemMovement(float DeltaTime)
{
	if (bRotates)
	{
		const FRotator DeltaRotation(0.f, DeltaTime * RotationRate, 0.f);
		CalculatedRotation = UKismetMathLibrary::ComposeRotators(CalculatedRotation, DeltaRotation);
	}
	if (bSinusoidalMovement)
	{
		const float Sine = SineAmplitude * FMath::Sin(RunningTime * SinePeriodConstant);
		CalculatedLocation = InitialLocation + FVector(0.f, 0.f, Sine);
	}
}


void AAuraEffectActor::BeginPlay()
{
	Super::BeginPlay();
	InitialLocation = GetActorLocation();
	CalculatedLocation = InitialLocation;
	CalculatedRotation = GetActorRotation();
}

void AAuraEffectActor::StartSinusoidalMovement()
{
	bSinusoidalMovement = true;
	InitialLocation = GetActorLocation();
	CalculatedLocation = InitialLocation;
}

void AAuraEffectActor::StartRotation()
{
	bRotates = true;
	CalculatedRotation = GetActorRotation();
}

void AAuraEffectActor::ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	check(GameplayEffectClass);
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	const bool bIsInfinite =  EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}

	if (!bIsInfinite)
	{
		Destroy();
	}
}

void AAuraEffectActor::ApplyDataDrivenEffect(AActor* TargetActor, const FString& EffectName)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;
	if (EffectName.IsEmpty()) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	// Load GameplayEffects.json
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("GameplayEffects.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Failed to load GameplayEffects.json"));
		return;
	}

	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Failed to parse GameplayEffects.json"));
		return;
	}

	const TSharedPtr<FJsonObject>* PickupObj;
	if (!RootObj->GetObjectField(TEXT("pickupEffects"))->TryGetObjectField(EffectName, PickupObj) || !PickupObj->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' not found in GameplayEffects.json"), *EffectName);
		return;
	}

	// Apply via the C++ UAuraPickupGameplayEffect with SetByCaller magnitudes.
	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(UAuraPickupGameplayEffect::StaticClass(), ActorLevel, Context);

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const float HealthValue = static_cast<float>((*PickupObj)->GetNumberField(TEXT("health")));
	const float ManaValue = static_cast<float>((*PickupObj)->GetNumberField(TEXT("mana")));
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, GameplayTags.Attributes_Vital_Health, HealthValue);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, GameplayTags.Attributes_Vital_Mana, ManaValue);

	// Handle duration override
	const FString DurationStr = (*PickupObj)->GetStringField(TEXT("duration"));
	if (DurationStr == TEXT("duration") && Spec.Data.IsValid())
	{
		Spec.Data.Get()->SetDuration((*PickupObj)->GetNumberField(TEXT("durationValue")), false);
	}

	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	const bool bIsInfinite = Spec.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}

	if (!bIsInfinite)
	{
		Destroy();
	}
}

void AAuraEffectActor::OnOverlap(AActor* TargetActor)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// Data-driven path: if effect names are set, use C++ GE + JSON config.
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		if (!InstantEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, InstantEffectName);
		else if (InstantGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		if (!DurationEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, DurationEffectName);
		else if (DurationGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		if (!InfiniteEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, InfiniteEffectName);
		else if (InfiniteGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
}

void AAuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		if (!InstantEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, InstantEffectName);
		else if (InstantGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		if (!DurationEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, DurationEffectName);
		else if (DurationGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		if (!InfiniteEffectName.IsEmpty())
			ApplyDataDrivenEffect(TargetActor, InfiniteEffectName);
		else if (InfiniteGameplayEffectClass)
			ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
	if (InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		TWeakObjectPtr<UAbilitySystemComponent> TargetASCWeak = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!TargetASCWeak.IsValid()) return;

		TArray<FActiveGameplayEffectHandle> HandlesToRemove;
		for (const TTuple<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>& HandlePair : ActiveEffectHandles)
		{
			if (TargetASCWeak == HandlePair.Value)
			{
				if (UAbilitySystemComponent* ASC = HandlePair.Value.Get())
				{
					ASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
				}
				HandlesToRemove.Add(HandlePair.Key);
			}
		}
		for (FActiveGameplayEffectHandle& Handle : HandlesToRemove)
		{
			ActiveEffectHandles.FindAndRemoveChecked(Handle);
		}
	}
}

void AAuraEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Remove any infinite effects we are still tracking so they don't leak when
	// the actor is destroyed without an end-overlap (e.g. level transition, GC).
	for (const TTuple<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>& HandlePair : ActiveEffectHandles)
	{
		if (UAbilitySystemComponent* ASC = HandlePair.Value.Get())
		{
			ASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
		}
	}
	ActiveEffectHandles.Empty();

	Super::EndPlay(EndPlayReason);
}


