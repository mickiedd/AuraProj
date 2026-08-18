// Copyright Druid Mechanics


#include "Actor/AuraEffectActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/AuraGameplayConfig.h"
#include "Kismet/KismetMathLibrary.h"
#include "AuraGameplayTags.h"
#include "AuraAttributeGameplayEffect.h"
#include "JsonObjectConverter.h"
#include "JsonUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace AuraEffectActorPrivate
{
	bool IsSupportedAttributeTag(const FGameplayTag& Tag)
	{
		const UAuraPickupGameplayEffect* PickupCDO = GetDefault<UAuraPickupGameplayEffect>();
		for (const FGameplayModifierInfo& Modifier : PickupCDO->Modifiers)
		{
			if (Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag == Tag)
			{
				return true;
			}
		}
		return false;
	}
}

AAuraEffectActor::AAuraEffectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetupAttachment(GetRootComponent());
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetupAttachment(GetRootComponent());
	CapsuleCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleCollision"));
	CapsuleCollision->SetupAttachment(GetRootComponent());
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(GetRootComponent());
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupVfx = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PickupVfx"));
	PickupVfx->SetupAttachment(GetRootComponent());
	SecondaryPickupVfx = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SecondaryPickupVfx"));
	SecondaryPickupVfx->SetupAttachment(GetRootComponent());

	for (UShapeComponent* Shape : { static_cast<UShapeComponent*>(SphereCollision), static_cast<UShapeComponent*>(BoxCollision), static_cast<UShapeComponent*>(CapsuleCollision) })
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetCollisionObjectType(ECC_WorldDynamic);
		Shape->SetCollisionResponseToAllChannels(ECR_Overlap);
		Shape->OnComponentBeginOverlap.AddDynamic(this, &AAuraEffectActor::OnCollisionBegin);
		Shape->OnComponentEndOverlap.AddDynamic(this, &AAuraEffectActor::OnCollisionEnd);
	}
}

AAuraEffectActor* AAuraEffectActor::SpawnConfiguredPickup(UObject* WorldContextObject, FName InDefinitionName, const FTransform& Transform, float InActorLevel)
{
	if (!WorldContextObject || !WorldContextObject->GetWorld()) return nullptr;
	const FAuraPickupDefinition* Definition = FAuraGameplayConfig::FindPickup(InDefinitionName);
	if (!Definition) return nullptr;
	AAuraEffectActor* Actor = WorldContextObject->GetWorld()->SpawnActorDeferred<AAuraEffectActor>(
		Definition->NativeClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
		ESpawnActorScaleMethod::MultiplyWithRoot);
	if (!Actor || !Actor->ConfigureFromDefinition(InDefinitionName))
	{
		if (Actor) Actor->Destroy();
		return nullptr;
	}
	Actor->ActorLevel = InActorLevel;
	Actor->FinishSpawning(Transform);
	return Actor;
}

void AAuraEffectActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!PickupDefinitionName.IsNone()) ConfigureFromDefinition(PickupDefinitionName);
}

bool AAuraEffectActor::ConfigureFromDefinition(FName InDefinitionName)
{
	const FAuraPickupDefinition* Definition = FAuraGameplayConfig::FindPickup(InDefinitionName);
	if (!Definition)
	{
		UE_LOG(LogTemp, Error, TEXT("[AuraEffectActor] Unknown pickup definition '%s'"), *InDefinitionName.ToString());
		return false;
	}
	PickupDefinitionName = InDefinitionName;
	bDestroyOnEffectApplication = Definition->bDestroyOnApplication;
	bApplyEffectsToEnemies = Definition->bApplyToEnemies;
	bRotates = Definition->bRotates;
	RotationRate = Definition->RotationRate;
	bSinusoidalMovement = Definition->bSinusoidalMovement;
	SineAmplitude = Definition->SineAmplitude;
	SinePeriodConstant = Definition->SinePeriodConstant;
	PrimaryActorTick.SetTickFunctionEnable(bRotates || bSinusoidalMovement);

	InstantEffectName.Empty(); DurationEffectName.Empty(); InfiniteEffectName.Empty();
	InstantEffectApplicationPolicy = DurationEffectApplicationPolicy = InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;
	if (const FAuraPickupEffectDefinition* Effect = FAuraGameplayConfig::FindPickupEffect(Definition->EffectName))
	{
		const EEffectApplicationPolicy Policy = Definition->bApplyOnEndOverlap ? EEffectApplicationPolicy::ApplyOnEndOverlap : EEffectApplicationPolicy::ApplyOnOverlap;
		if (Effect->DurationType == TEXT("instant")) { InstantEffectName = Definition->EffectName.ToString(); InstantEffectApplicationPolicy = Policy; }
		else if (Effect->DurationType == TEXT("duration")) { DurationEffectName = Definition->EffectName.ToString(); DurationEffectApplicationPolicy = Policy; }
		else { InfiniteEffectName = Definition->EffectName.ToString(); InfiniteEffectApplicationPolicy = Policy; }
	}
	InfiniteEffectRemovalPolicy = Definition->bRemoveOnEndOverlap ? EEffectRemovalPolicy::RemoveOnEndOverlap : EEffectRemovalPolicy::DoNotRemove;

	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Definition->CollisionShape == EAuraConfiguredCollisionShape::Sphere) { SphereCollision->SetSphereRadius(Definition->CollisionSize.X); SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	else if (Definition->CollisionShape == EAuraConfiguredCollisionShape::Box) { BoxCollision->SetBoxExtent(Definition->CollisionSize); BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
	else { CapsuleCollision->SetCapsuleSize(Definition->CollisionSize.X, Definition->CollisionSize.Y); CapsuleCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }

	PickupMesh->SetStaticMesh(Cast<UStaticMesh>(Definition->Mesh.IsNull() ? nullptr : Definition->Mesh.TryLoad()));
	PickupMesh->SetRelativeLocation(Definition->MeshOffset); PickupMesh->SetRelativeRotation(Definition->MeshRotation); PickupMesh->SetRelativeScale3D(Definition->MeshScale);
	for (int32 Index = 0; Index < Definition->Materials.Num(); ++Index) PickupMesh->SetMaterial(Index, Cast<UMaterialInterface>(Definition->Materials[Index].TryLoad()));
	PickupVfx->SetAsset(Cast<UNiagaraSystem>(Definition->Vfx.IsNull() ? nullptr : Definition->Vfx.TryLoad())); PickupVfx->SetRelativeLocation(Definition->VfxOffset);
	SecondaryPickupVfx->SetAsset(Cast<UNiagaraSystem>(Definition->SecondaryVfx.IsNull() ? nullptr : Definition->SecondaryVfx.TryLoad())); SecondaryPickupVfx->SetRelativeLocation(Definition->SecondaryVfxOffset);
	return true;
}

void AAuraEffectActor::OnCollisionBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	OnOverlap(OtherActor);
}

void AAuraEffectActor::OnCollisionEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	OnEndOverlap(OtherActor);
}

void AAuraEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bRotates && !bSinusoidalMovement) return;
	RunningTime += DeltaTime;
	const float SafePeriodConstant = FMath::Max(FMath::Abs(SinePeriodConstant), UE_KINDA_SMALL_NUMBER);
	const float SinePeriod = 2 * PI / SafePeriodConstant;
	if (RunningTime > SinePeriod)
	{
		RunningTime = 0.f;
	}
	ItemMovement(DeltaTime);
	SetActorLocationAndRotation(CalculatedLocation, CalculatedRotation);
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
		const float Sine = SineAmplitude * FMath::Sin(RunningTime * FMath::Max(FMath::Abs(SinePeriodConstant), UE_KINDA_SMALL_NUMBER));
		CalculatedLocation = InitialLocation + FVector(0.f, 0.f, Sine);
	}
}


void AAuraEffectActor::BeginPlay()
{
	Super::BeginPlay();
	if (!PickupDefinitionName.IsNone()) ConfigureFromDefinition(PickupDefinitionName);
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

void AAuraEffectActor::ApplyDataDrivenEffect(AActor* TargetActor, const FString& EffectName)
{
	if (!HasAuthority()) return;
	if (!IsValid(TargetActor)) return;
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;
	if (EffectName.IsEmpty()) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	const FAuraPickupEffectDefinition* EffectDefinition = FAuraGameplayConfig::FindPickupEffect(FName(*EffectName));
	if (!EffectDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Unknown cached pickup effect '%s'"), *EffectName);
		return;
	}

	TSubclassOf<UGameplayEffect> EffectClass;
	if (EffectDefinition->DurationType == TEXT("instant")) EffectClass = UAuraPickupGameplayEffect::StaticClass();
	else if (EffectDefinition->DurationType == TEXT("duration")) EffectClass = EffectDefinition->bExecuteOnApplication ? UAuraPickupGameplayEffect_Duration::StaticClass() : UAuraPickupGameplayEffect_DurationDelayed::StaticClass();
	else EffectClass = EffectDefinition->bExecuteOnApplication ? UAuraPickupGameplayEffect_Infinite::StaticClass() : UAuraPickupGameplayEffect_InfiniteDelayed::StaticClass();

	FGameplayEffectContextHandle CachedContext = TargetASC->MakeEffectContext();
	CachedContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle CachedSpec = TargetASC->MakeOutgoingSpec(EffectClass, ActorLevel, CachedContext);
	if (!CachedSpec.IsValid()) return;
	for (const FGameplayModifierInfo& Modifier : EffectClass->GetDefaultObject<UGameplayEffect>()->Modifiers)
	{
		const FGameplayTag DataTag = Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag;
		if (DataTag.IsValid()) UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(CachedSpec, DataTag, EffectDefinition->Magnitudes.FindRef(DataTag));
	}
	if (EffectDefinition->DurationType == TEXT("duration")) CachedSpec.Data->SetDuration(EffectDefinition->Duration, false);
	if (EffectDefinition->Period > 0.f) CachedSpec.Data->Period = EffectDefinition->Period;
	CachedSpec.Data->AppendDynamicAssetTags(EffectDefinition->AssetTags);
	const FActiveGameplayEffectHandle CachedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*CachedSpec.Data.Get());
	if (!CachedHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' was rejected by the target ASC; pickup remains available."), *EffectName);
		return;
	}
	const bool bCachedInfinite = EffectDefinition->DurationType == TEXT("infinite");
	if (bCachedInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap) ActiveEffectHandles.Add(CachedHandle, TargetASC);
	if (!bCachedInfinite && bDestroyOnEffectApplication) Destroy();
	return;

#if 0 // Retained temporarily for migration comparison; the cached path above is authoritative.

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

	const TSharedPtr<FJsonObject>* PickupEffectsObj = nullptr;
	if (!RootObj->TryGetObjectField(TEXT("pickupEffects"), PickupEffectsObj) || !PickupEffectsObj || !PickupEffectsObj->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] GameplayEffects.json has no valid 'pickupEffects' object"));
		return;
	}

	const TSharedPtr<FJsonObject>* PickupObj = nullptr;
	if (!(*PickupEffectsObj)->TryGetObjectField(EffectName, PickupObj) || !PickupObj || !PickupObj->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' not found in GameplayEffects.json"), *EffectName);
		return;
	}

	FString DurationType;
	if (!(*PickupObj)->TryGetStringField(TEXT("duration"), DurationType))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' is missing string field 'duration'"), *EffectName);
		return;
	}
	DurationType.ToLowerInline();

	bool bExecuteOnApplication = true;
	(*PickupObj)->TryGetBoolField(TEXT("executeOnApplication"), bExecuteOnApplication);

	TSubclassOf<UGameplayEffect> EffectClass;
	if (DurationType == TEXT("instant"))
	{
		EffectClass = UAuraPickupGameplayEffect::StaticClass();
	}
	else if (DurationType == TEXT("duration"))
	{
		EffectClass = bExecuteOnApplication
			? UAuraPickupGameplayEffect_Duration::StaticClass()
			: UAuraPickupGameplayEffect_DurationDelayed::StaticClass();
	}
	else if (DurationType == TEXT("infinite"))
	{
		EffectClass = bExecuteOnApplication
			? UAuraPickupGameplayEffect_Infinite::StaticClass()
			: UAuraPickupGameplayEffect_InfiniteDelayed::StaticClass();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' has invalid duration '%s'"), *EffectName, *DurationType);
		return;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(EffectClass, ActorLevel, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Failed to create spec for pickup effect '%s'"), *EffectName);
		return;
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	TMap<FGameplayTag, float> AttributeMagnitudes;
	// Every modifier on the shared native GE must have a SetByCaller value. GAS
	// logs missing values as errors, so seed all supported attributes with a
	// harmless zero before overlaying the configured values.
	const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
	for (const FGameplayModifierInfo& Modifier : EffectCDO->Modifiers)
	{
		const FGameplayTag DataTag = Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag;
		if (DataTag.IsValid())
		{
			AttributeMagnitudes.Add(DataTag, 0.f);
		}
	}
	double NumberValue = 0.0;
	if ((*PickupObj)->TryGetNumberField(TEXT("health"), NumberValue))
	{
		AttributeMagnitudes.Add(GameplayTags.Attributes_Vital_Health, static_cast<float>(NumberValue));
	}
	if ((*PickupObj)->TryGetNumberField(TEXT("mana"), NumberValue))
	{
		AttributeMagnitudes.Add(GameplayTags.Attributes_Vital_Mana, static_cast<float>(NumberValue));
	}

	const TSharedPtr<FJsonObject>* AttributesObj = nullptr;
	if ((*PickupObj)->TryGetObjectField(TEXT("attributes"), AttributesObj) && AttributesObj && AttributesObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*AttributesObj)->Values)
		{
			double AttributeValue = 0.0;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(AttributeValue))
			{
				UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' attribute '%s' must be numeric"), *EffectName, *Pair.Key);
				continue;
			}

			const FGameplayTag AttributeTag = FGameplayTag::RequestGameplayTag(FName(*Pair.Key), false);
			if (!AttributeTag.IsValid() || !AuraEffectActorPrivate::IsSupportedAttributeTag(AttributeTag))
			{
				UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' uses unsupported attribute tag '%s'"), *EffectName, *Pair.Key);
				continue;
			}
			AttributeMagnitudes.Add(AttributeTag, static_cast<float>(AttributeValue));
		}
	}

	for (const TPair<FGameplayTag, float>& Pair : AttributeMagnitudes)
	{
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Pair.Key, Pair.Value);
	}

	if (DurationType == TEXT("duration"))
	{
		double DurationValue = 0.0;
		if (!(*PickupObj)->TryGetNumberField(TEXT("durationValue"), DurationValue) || DurationValue <= 0.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Duration pickup effect '%s' requires durationValue > 0"), *EffectName);
			return;
		}
		Spec.Data->SetDuration(static_cast<float>(DurationValue), false);
	}

	double PeriodValue = 0.0;
	if ((*PickupObj)->TryGetNumberField(TEXT("period"), PeriodValue))
	{
		if (DurationType == TEXT("instant") || PeriodValue <= 0.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' has an invalid period"), *EffectName);
			return;
		}
		Spec.Data->Period = static_cast<float>(PeriodValue);
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetTagValues = nullptr;
	if ((*PickupObj)->TryGetArrayField(TEXT("assetTags"), AssetTagValues) && AssetTagValues)
	{
		FGameplayTagContainer DynamicAssetTags;
		for (const TSharedPtr<FJsonValue>& TagValue : *AssetTagValues)
		{
			FString TagString;
			if (!TagValue.IsValid() || !TagValue->TryGetString(TagString)) continue;
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
			if (Tag.IsValid())
			{
				DynamicAssetTags.AddTag(Tag);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[AuraEffectActor] Pickup effect '%s' has unknown asset tag '%s'"), *EffectName, *TagString);
			}
		}
		Spec.Data->AppendDynamicAssetTags(DynamicAssetTags);
	}

	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	const bool bIsInfinite = DurationType == TEXT("infinite");
	if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}

	if (!bIsInfinite)
	{
		Destroy();
	}
#endif
}

void AAuraEffectActor::OnOverlap(AActor* TargetActor)
{
	if (!HasAuthority()) return;
	if (!IsValid(TargetActor)) return;
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap && !InstantEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, InstantEffectName);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap && !DurationEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, DurationEffectName);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap && !InfiniteEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, InfiniteEffectName);
	}
}

void AAuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
	if (!HasAuthority()) return;
	if (!IsValid(TargetActor)) return;
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap && !InstantEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, InstantEffectName);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap && !DurationEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, DurationEffectName);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap && !InfiniteEffectName.IsEmpty())
	{
		ApplyDataDrivenEffect(TargetActor, InfiniteEffectName);
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


