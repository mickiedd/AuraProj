// Copyright Druid Mechanics


#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityTypes.h"
#include "AuraGameplayTags.h"
#include "Character/AuraCharacterBase.h"
#include "Combat/AuraCombatRules.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Game/AuraGameModeBase.h"
#include "Game/LoadScreenSaveGame.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AuraPlayerState.h"
#include "UI/HUD/AuraHUD.h"
#include "UI/WidgetController/AuraWidgetController.h"

// Role config (JSON)
#include "AbilitySystem/Data/RoleInfo.h"
#include "AuraDamageGameplayEffect.h"
#include "AuraAttributeGameplayEffect.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatRules.h"

#include "AuraAbilityGraph/Public/AbilityDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h" // IFileManager + FFileStatData for the reload sentinel poll
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimClassInterface.h"
#include "Materials/MaterialInstance.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "Aura/AuraLogChannels.h"

bool UAuraAbilitySystemLibrary::MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, AAuraHUD*& OutAuraHUD)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		OutAuraHUD = Cast<AAuraHUD>(PC->GetHUD());
		if (OutAuraHUD)
		{
			AAuraPlayerState* PS = PC->GetPlayerState<AAuraPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();

			OutWCParams.AttributeSet = AS;
			OutWCParams.AbilitySystemComponent = ASC;
			OutWCParams.PlayerState = PS;
			OutWCParams.PlayerController = PC;
			return true;
		}
	}
	return false;
}

UOverlayWidgetController* UAuraAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetOverlayWidgetController(WCParams);
	}
	return nullptr;
}

UAttributeMenuWidgetController* UAuraAbilitySystemLibrary::GetAttributeMenuWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetAttributeMenuWidgetController(WCParams);
	}
	return nullptr;
}

USpellMenuWidgetController* UAuraAbilitySystemLibrary::GetSpellMenuWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetSpellMenuWidgetController(WCParams);
	}
	return nullptr;
}

void UAuraAbilitySystemLibrary::TopOffVitalAttributes(UAbilitySystemComponent* ASC, const UObject* SourceAvatar)
{
	if (!ASC) return;
	const UAuraAttributeSet* AuraAS = Cast<UAuraAttributeSet>(ASC->GetAttributeSet(UAuraAttributeSet::StaticClass()));
	if (!AuraAS) return;

	// Apply a Vital.Health/Vital.Mana SetByCaller GE with the max values so current
	// Health/Mana are initialized to max. PostGameplayEffectExecute clamps and the
	// value-change delegates (OnHealthChanged/OnManaChanged) fire for the UI.
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceAvatar);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UAuraPickupGameplayEffect::StaticClass(), 1.f, Ctx);
	AssignDefaultAttributeMagnitudes(Spec);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Tags.Attributes_Vital_Health, AuraAS->GetMaxHealth());
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Tags.Attributes_Vital_Mana, AuraAS->GetMaxMana());
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UAuraAbilitySystemLibrary::AssignDefaultAttributeMagnitudes(const FGameplayEffectSpecHandle& Spec)
{
	if (!Spec.IsValid()) return;

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FGameplayTag AttributeTags[] =
	{
		Tags.Attributes_Primary_Strength,
		Tags.Attributes_Primary_Intelligence,
		Tags.Attributes_Primary_Resilience,
		Tags.Attributes_Primary_Vigor,
		Tags.Attributes_Secondary_Armor,
		Tags.Attributes_Secondary_ArmorPenetration,
		Tags.Attributes_Secondary_BlockChance,
		Tags.Attributes_Secondary_CriticalHitChance,
		Tags.Attributes_Secondary_CriticalHitDamage,
		Tags.Attributes_Secondary_CriticalHitResistance,
		Tags.Attributes_Secondary_HealthRegeneration,
		Tags.Attributes_Secondary_ManaRegeneration,
		Tags.Attributes_Secondary_MaxHealth,
		Tags.Attributes_Secondary_MaxMana,
		Tags.Attributes_Resistance_Fire,
		Tags.Attributes_Resistance_Lightning,
		Tags.Attributes_Resistance_Arcane,
		Tags.Attributes_Resistance_Physical,
		Tags.Attributes_Vital_Health,
		Tags.Attributes_Vital_Mana
	};

	for (const FGameplayTag& Tag : AttributeTags)
	{
		if (Tag.IsValid())
		{
			UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(Spec, Tag, 0.f);
		}
	}
}

void UAuraAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	// All attribute GEs are now C++ (UAuraAttributeGameplayEffect) with SetByCaller magnitudes.
	// Primary: zero defaults (legacy path didn't use SetByCaller for per-class; the role config path handles real values).
	// Secondary/Vital/Resistance: loaded from GameplayEffects.json by the caller's LoadAndApplySecondaryAttributes.
	FGameplayEffectContextHandle PrimaryContext = ASC->MakeEffectContext();
	PrimaryContext.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimarySpec = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), Level, PrimaryContext);
	AssignDefaultAttributeMagnitudes(PrimarySpec);
	ASC->ApplyGameplayEffectSpecToSelf(*PrimarySpec.Data.Get());

	// Secondary + Vital + Resistance from GameplayEffects.json
	FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("GameplayEffects.json"));
	FString JsonContent;
	if (FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		TSharedPtr<FJsonObject> RootObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
		if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
		{
			const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
			FGameplayEffectContextHandle SecContext = ASC->MakeEffectContext();
			SecContext.AddSourceObject(AvatarActor);
			const FGameplayEffectSpecHandle SecSpec = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), Level, SecContext);
			AssignDefaultAttributeMagnitudes(SecSpec);

			auto AssignFromJson = [&SecSpec](const TSharedPtr<FJsonObject>& Obj, FGameplayTag Tag, const FString& FieldName)
			{
				if (Obj.IsValid() && Obj->HasField(FieldName))
				{
					UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SecSpec, Tag, static_cast<float>(Obj->GetNumberField(FieldName)));
				}
			};

			const TSharedPtr<FJsonObject>& Secondary = RootObj->GetObjectField(TEXT("secondaryAttributes"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_Armor, TEXT("Armor"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_ArmorPenetration, TEXT("ArmorPenetration"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_BlockChance, TEXT("BlockChance"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitChance, TEXT("CriticalHitChance"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitDamage, TEXT("CriticalHitDamage"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_CriticalHitResistance, TEXT("CriticalHitResistance"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_HealthRegeneration, TEXT("HealthRegeneration"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_ManaRegeneration, TEXT("ManaRegeneration"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxHealth, TEXT("MaxHealth"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxMana, TEXT("MaxMana"));

			const TSharedPtr<FJsonObject>& Resistances = RootObj->GetObjectField(TEXT("resistances"));
			AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Fire, TEXT("Fire"));
			AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Lightning, TEXT("Lightning"));
			AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Arcane, TEXT("Arcane"));
			AssignFromJson(Resistances, GameplayTags.Attributes_Resistance_Physical, TEXT("Physical"));

			ASC->ApplyGameplayEffectSpecToSelf(*SecSpec.Data.Get());
		}
	}

	// Initialize current Health/Mana to MaxHealth/MaxMana (MaxHealth/MaxMana set above).
	TopOffVitalAttributes(ASC, AvatarActor);
}

void UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ULoadScreenSaveGame* SaveGame)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	const AActor* SourceAvatarActor = ASC->GetAvatarActor();

	// Primary attributes via C++ SetByCaller GE, magnitudes from save data.
	FGameplayEffectContextHandle EffectContexthandle = ASC->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, EffectContexthandle);
	AssignDefaultAttributeMagnitudes(SpecHandle);

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Strength, SaveGame->Strength);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Intelligence, SaveGame->Intelligence);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Resilience, SaveGame->Resilience);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Vigor, SaveGame->Vigor);

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	// Secondary + Vital: use Infinite variant for save-data path (matches legacy behavior).
	FGameplayEffectContextHandle SecContext = ASC->MakeEffectContext();
	SecContext.AddSourceObject(SourceAvatarActor);
	ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect_Infinite::StaticClass(), 1.f, SecContext);

	// Vital via C++ GE (same class, different magnitudes from JSON).
	FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("GameplayEffects.json"));
	FString JsonContent;
	if (FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		TSharedPtr<FJsonObject> RootObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
		if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
		{
			FGameplayEffectContextHandle VitalContext = ASC->MakeEffectContext();
			VitalContext.AddSourceObject(SourceAvatarActor);
			const FGameplayEffectSpecHandle VitalSpec = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, VitalContext);
			AssignDefaultAttributeMagnitudes(VitalSpec);

			const TSharedPtr<FJsonObject>& Secondary = RootObj->GetObjectField(TEXT("secondaryAttributes"));
			auto AssignFromJson = [&VitalSpec](const TSharedPtr<FJsonObject>& Obj, FGameplayTag Tag, const FString& FieldName)
			{
				if (Obj.IsValid() && Obj->HasField(FieldName))
				{
					UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(VitalSpec, Tag, static_cast<float>(Obj->GetNumberField(FieldName)));
				}
			};
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxHealth, TEXT("MaxHealth"));
			AssignFromJson(Secondary, GameplayTags.Attributes_Secondary_MaxMana, TEXT("MaxMana"));

			ASC->ApplyGameplayEffectSpecToSelf(*VitalSpec.Data.Get());
		}
	}

	// Initialize current Health/Mana to MaxHealth/MaxMana (MaxHealth/MaxMana set above).
	TopOffVitalAttributes(ASC, SourceAvatarActor);
}

void UAuraAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return;

	// Enemy abilities are configured independently from player RoleConfig because
	// ECharacterClass drives AI archetypes. Grant the XML set first and retain the
	// legacy data-asset path only as a fail-safe for malformed/missing config.
	TArray<UAuraAbilityDefinition*> EnemyDefinitions;
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/EnemyAbilityConfig.json"));
	FString ConfigContent;
	TSharedPtr<FJsonObject> ConfigObject;
	if (FFileHelper::LoadFileToString(ConfigContent, *ConfigPath))
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ConfigContent);
		FJsonSerializer::Deserialize(Reader, ConfigObject);
	}

	auto LoadDefinitionArray = [&EnemyDefinitions](const TArray<TSharedPtr<FJsonValue>>* Values)
	{
		if (!Values) return true;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Path;
			if (!Value.IsValid() || !Value->TryGetString(Path)) return false;
			UAuraAbilityDefinition* Definition = UAuraAbilitySystemLibrary::LoadAbilityDefinitionFromXMLFile(Path);
			if (!Definition) return false;
			EnemyDefinitions.Add(Definition);
		}
		return true;
	};

	bool bLoadedDataDriven = false;
	if (ConfigObject.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Common = nullptr;
		const TSharedPtr<FJsonObject>* Classes = nullptr;
		bLoadedDataDriven = ConfigObject->TryGetArrayField(TEXT("commonAbilityDefinitions"), Common) &&
			ConfigObject->TryGetObjectField(TEXT("classes"), Classes) && LoadDefinitionArray(Common);
		if (bLoadedDataDriven)
		{
			const TCHAR* ClassName = CharacterClass == ECharacterClass::Elementalist ? TEXT("Elementalist") :
				CharacterClass == ECharacterClass::Ranger ? TEXT("Ranger") : TEXT("Warrior");
			const TArray<TSharedPtr<FJsonValue>>* ClassValues = nullptr;
			bLoadedDataDriven = (*Classes)->TryGetArrayField(ClassName, ClassValues) && LoadDefinitionArray(ClassValues);
		}
	}

	if (bLoadedDataDriven && EnemyDefinitions.Num() >= 2)
	{
		if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(ASC))
		{
			const int32 AbilityLevel = ASC->GetAvatarActor()->Implements<UCombatInterface>()
				? ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor()) : 1;
			AuraASC->AddCharacterDataAbilities(EnemyDefinitions, AbilityLevel);
			return;
		}
	}
	UE_LOG(LogAura, Error, TEXT("[EnemyAbilities] Failed to load '%s'; retaining legacy Blueprint grants"), *ConfigPath);
	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}
	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor()));
			ASC->GiveAbility(AbilitySpec);
		}
	}
}

int32 UAuraAbilitySystemLibrary::GetXPRewardForClassAndLevel(const UObject* WorldContextObject, ECharacterClass CharacterClass, int32 CharacterLevel)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return 0;

	const FCharacterClassDefaultInfo& Info = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	const float XPReward = Info.XPReward.GetValueAtLevel(CharacterLevel);

	return static_cast<int32>(XPReward);
}

void UAuraAbilitySystemLibrary::SetIsRadialDamageEffectParam(FDamageEffectParams& DamageEffectParams, bool bIsRadial, float InnerRadius, float OuterRadius, FVector Origin)
{
	DamageEffectParams.bIsRadialDamage = bIsRadial;
	DamageEffectParams.RadialDamageInnerRadius = InnerRadius;
	DamageEffectParams.RadialDamageOuterRadius = OuterRadius;
	DamageEffectParams.RadialDamageOrigin = Origin;
}

void UAuraAbilitySystemLibrary::SetKnockbackDirection(FDamageEffectParams& DamageEffectParams, FVector KnockbackDirection, float Magnitude)
{
	KnockbackDirection.Normalize();
	if (Magnitude == 0.f)
	{
		DamageEffectParams.KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackForceMagnitude;
	}
	else
	{
		DamageEffectParams.KnockbackForce = KnockbackDirection * Magnitude;
	}
}

void UAuraAbilitySystemLibrary::SetDeathImpulseDirection(FDamageEffectParams& DamageEffectParams, FVector ImpulseDirection, float Magnitude)
{
	ImpulseDirection.Normalize();
	if (Magnitude == 0.f)
	{
		DamageEffectParams.DeathImpulse = ImpulseDirection * DamageEffectParams.DeathImpulseMagnitude;
	}
	else
	{
		DamageEffectParams.DeathImpulse = ImpulseDirection * Magnitude;
	}
}

void UAuraAbilitySystemLibrary::SetTargetEffectParamsASC(FDamageEffectParams& DamageEffectParams,
	UAbilitySystemComponent* InASC)
{
	DamageEffectParams.TargetAbilitySystemComponent = InASC;
}

UCharacterClassInfo* UAuraAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	const AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return nullptr;
	return AuraGameMode->CharacterClassInfo;
}

// File-scope process-lifetime cache for the client (no GameMode) path. Held via
// TStrongObjectPtr so it roots URuntimeAbilityInfo; cleared on PIE end by
// ClearProcessLifetimeCaches() so the editor's EndPlayMap stale-reference detector
// doesn't flag it (and its referenced assets) as a leak across PIE sessions.
static TStrongObjectPtr<URuntimeAbilityInfo> GClientAbilityInfoCache;

URuntimeAbilityInfo* UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(const UObject* WorldContextObject)
{
	// Server: get from GameMode's cached instance
	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		if (!AuraGameMode->RuntimeAbilityInfo)
		{
			// First access on server: load from JSON
			AuraGameMode->RuntimeAbilityInfo = LoadAbilityInfoFromJSON();
		}
		return AuraGameMode->RuntimeAbilityInfo;
	}

	// Client: use process-lifetime cache (reload if null)
	if (!GClientAbilityInfoCache.IsValid())
	{
		GClientAbilityInfoCache = TStrongObjectPtr<URuntimeAbilityInfo>(LoadAbilityInfoFromJSON());
	}
	return GClientAbilityInfoCache.Get();
}

URuntimeAbilityInfo* UAuraAbilitySystemLibrary::LoadAbilityInfoFromJSON()
{
	const FString JSONPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("AbilityInfo.json"));
	FString JSONContent;

	if (!FFileHelper::LoadFileToString(JSONContent, *JSONPath))
	{
		UE_LOG(LogAura, Error, TEXT("[AbilityInfo] Failed to load AbilityInfo.json from: %s"), *JSONPath);
		return nullptr;
	}

	URuntimeAbilityInfo* Info = NewObject<URuntimeAbilityInfo>(GetTransientPackage());
	if (!Info->LoadFromJSON(JSONContent))
	{
		UE_LOG(LogAura, Error, TEXT("[AbilityInfo] Rejected invalid AbilityInfo.json at: %s"), *JSONPath);
		return nullptr;
	}

	UE_LOG(LogAura, Log, TEXT("[AbilityInfo] Successfully loaded AbilityInfo.json from: %s"), *JSONPath);
	return Info;
}

UAuraAbilityDefinition* UAuraAbilitySystemLibrary::LoadAbilityDefinitionFromXMLFile(const FString& FilePath)
{
	// Resolve the path to a real filesystem path (mirror BehaviorU's path resolution)
	// Handles three forms:
	//   /Game/AbilityDefinitions/FireGun.xml  → <ProjectContentDir>/AbilityDefinitions/FireGun.xml
	//   relative/path.xml  → <ProjectContentDir>/relative/path.xml
	//   C:/absolute/path.xml → used as-is
	FString ResolvedPath = FilePath;
	if (ResolvedPath.StartsWith(TEXT("/Game/")))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath.Mid(6 /*len("/Game/")*/));
	}
	else if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath);
	}
	ResolvedPath = FPaths::ConvertRelativePathToFull(ResolvedPath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ResolvedPath))
	{
		UE_LOG(LogAura, Error, TEXT("[AbilityDefinition] Failed to read XML file: %s"), *ResolvedPath);
		return nullptr;
	}

	// Create a transient definition object (not saved to asset registry; held alive by caller)
	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	Definition->AbilityName = FName(*FPaths::GetBaseFilename(ResolvedPath));

	if (!Definition->LoadFromXML(FileContent))
	{
		UE_LOG(LogAura, Error, TEXT("[AbilityDefinition] XML parse failed for: %s"), *ResolvedPath);
		return nullptr;
	}

	UE_LOG(LogAura, Log, TEXT("[AbilityDefinition] Loaded ability definition from XML: %s (Tag=%s)"),
		*ResolvedPath, *Definition->AbilityTag.ToString());

	RegisterAbilityDefinition(Definition);
	return Definition;
}

// Process-lifetime registry of ability definitions keyed by AbilityTag. TWeakObjectPtr so
// a reloaded RoleConfig (new transient definition objects) doesn't keep the stale ones alive;
// the map simply re-points the tag at the newest definition on the next load.
namespace AuraAbilityDefRegistryPrivate
{
	TMap<FGameplayTag, TWeakObjectPtr<UAuraAbilityDefinition>> GDefinitionRegistry;
}

void UAuraAbilitySystemLibrary::RegisterAbilityDefinition(UAuraAbilityDefinition* Definition)
{
	if (!Definition || !Definition->AbilityTag.IsValid())
	{
		return;
	}
	AuraAbilityDefRegistryPrivate::GDefinitionRegistry.Add(Definition->AbilityTag, Definition);
}

const UAuraAbilityDefinition* UAuraAbilitySystemLibrary::FindAbilityDefinitionByTag(const FGameplayTag& AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<UAuraAbilityDefinition>* Found = AuraAbilityDefRegistryPrivate::GDefinitionRegistry.Find(AbilityTag))
	{
		return Found->Get();
	}
	return nullptr;
}

namespace RoleConfigReloadPrivate
{
	// Process-lifetime cache of URoleInfo for contexts with no GameMode (pure clients on the
	// battleground or the Login map). Promoted from a function-static so ReloadRoleConfig can
	// reset it by name. Held via TStrongObjectPtr because URoleInfo is a transient UObject.
	static TStrongObjectPtr<URoleInfo> GClientRoleInfoCache;

	// Editor "Reload Role Config" tool writes this sentinel to Content/Config/.RoleConfig.reload.
	// Runtimes poll it (cheap stat) and reload when it appears/changes.
	static FString GetReloadSentinelPath()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT(".RoleConfig.reload"));
	}

	// Throttle the sentinel stat so per-frame callers (e.g. GetRoleInfo) cost nothing.
	static double GLastSentinelCheckTime = 0.0;
	static FDateTime GLastSentinelModTime; // Default-constructed (0 ticks) => any real mtime is "newer".
	static constexpr double SentinelCheckIntervalSeconds = 0.5;
}

void UAuraAbilitySystemLibrary::ClearProcessLifetimeCaches()
{
	// Drop the process-lifetime TStrongObjectPtr caches so the transient UObjects they root
	// (URoleInfo + its UAuraAbilityDefinition array and subobjects, URuntimeAbilityInfo) can be
	// garbage-collected when the PIE world tears down. Without this the editor's EndPlayMap
	// stale-reference detector reports them as leaks across PIE sessions. The weak definition
	// registry is emptied too (it doesn't root objects, but clearing avoids dangling entries).
	// Next access lazily rebuilds each cache from JSON. Intended to be called from EndPIE.
	RoleConfigReloadPrivate::GClientRoleInfoCache.Reset();
	GClientAbilityInfoCache.Reset();
	AuraAbilityDefRegistryPrivate::GDefinitionRegistry.Empty();
	UE_LOG(LogAura, Log, TEXT("[AbilitySystemLibrary] Cleared process-lifetime RoleInfo/AbilityInfo/Definition caches (PIE end)."));
}

URoleInfo* UAuraAbilitySystemLibrary::GetRoleInfo(const UObject* WorldContextObject)
{
	// Pick up an editor-triggered RoleConfig.json reload (writes a sentinel file) before
	// handing out the cached URoleInfo, so callers on pure clients (Login screen, battleground)
	// see the new defaultRole/assets on the next call after the button is clicked. Throttled.
	PollRoleConfigReload(WorldContextObject);

	if (const AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		if (AuraGameMode->RoleInfo)
		{
			return AuraGameMode->RoleInfo;
		}
	}

	// No GameMode (e.g. a client on the battleground map) or GameMode hasn't loaded RoleInfo yet:
	// load RoleConfig.json directly and cache it for the process lifetime. RoleConfig.json is
	// packaged with the client (same as LevelConfig.json), so this works client-side.
	if (!RoleConfigReloadPrivate::GClientRoleInfoCache.IsValid())
	{
		RoleConfigReloadPrivate::GClientRoleInfoCache.Reset(LoadRoleInfoFromConfig(WorldContextObject));
	}
	return RoleConfigReloadPrivate::GClientRoleInfoCache.Get();
}

FName UAuraAbilitySystemLibrary::GetDefaultRole(const UObject* WorldContextObject)
{
	if (const URoleInfo* RoleInfo = GetRoleInfo(WorldContextObject))
	{
		return RoleInfo->DefaultRole;
	}
	return NAME_None;
}

void UAuraAbilitySystemLibrary::ReloadRoleConfig(const UObject* WorldContextObject)
{
	const FAuraRoleLoadResult Candidate = LoadRoleInfoCandidate(WorldContextObject);
	URoleInfo* Reloaded = nullptr;

	// On the server (GameMode present), refresh the authoritative AAuraGameModeBase::RoleInfo
	// that every server-side spawn/login reads. The GameMode keeps its own poll timer, but this
	// path also covers a direct console-command invocation on a listen/PIE server.
	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		URoleInfo* Current = AuraGameMode->RoleInfo;
		if (TryPublishRoleInfo(Current, Candidate))
		{
			AuraGameMode->RoleInfo = Current;
			Reloaded = Current;
		}
	}
	else
	{
		if (Candidate.bCanPublish && Candidate.Candidate)
		{
			Reloaded = Candidate.Candidate;
			RoleConfigReloadPrivate::GClientRoleInfoCache.Reset(Reloaded);
		}
	}

	if (Reloaded)
	{
		UE_LOG(LogAura, Log, TEXT("[RoleConfig] Reloaded: %d role(s), defaultRole='%s'."),
			Reloaded->RoleInformation.Num(),
			*Reloaded->DefaultRole.ToString());
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Reload rejected; retaining last known-good registry: %s"), *Candidate.ToLogString());
	}
}

void UAuraAbilitySystemLibrary::PollRoleConfigReload(const UObject* WorldContextObject)
{
	using namespace RoleConfigReloadPrivate;

	const double Now = FPlatformTime::Seconds();
	if (Now - GLastSentinelCheckTime < SentinelCheckIntervalSeconds)
	{
		return; // Throttled: avoid a file stat on every GetRoleInfo call.
	}
	GLastSentinelCheckTime = Now;

	const FString SentinelPath = GetReloadSentinelPath();
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*SentinelPath))
	{
		return; // No reload requested since last consumption.
	}

	// Stat the sentinel; reload only when its mtime advances (the editor writes a fresh
	// timestamp body on each click, which updates mtime). This avoids re-loading on a
	// stale/leftover sentinel from a previous session.
	const FFileStatData StatData = FileManager.GetStatData(*SentinelPath);
	if (!StatData.bIsValid)
	{
		return;
	}

	const FDateTime SentinelModTime = StatData.ModificationTime;
	if (SentinelModTime <= GLastSentinelModTime)
	{
		return;
	}

	GLastSentinelModTime = SentinelModTime;

	UE_LOG(LogAura, Log, TEXT("[RoleConfig] Reload sentinel detected; reloading RoleConfig.json."));
	ReloadRoleConfig(WorldContextObject);

	// Consume the sentinel so we don't reload again until the next editor button click.
	// IFileManager::Delete(Filename, RequireExists, EvenReadOnly, Quiet).
	FileManager.Delete(*SentinelPath, false, true, true);
}

namespace RoleConfigPrivate
{
	static void AddIssue(FAuraRoleLoadResult& Result, EAuraRoleValidationSeverity Severity, FName RoleId, const FString& Path, const FString& Message)
	{
		FAuraRoleValidationIssue& Issue = Result.Issues.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.RoleId = RoleId;
		Issue.JsonPath = Path;
		Issue.Message = Message;
	}

	static bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out, FAuraRoleLoadResult& Result, FName RoleId, const FString& BasePath, bool bRequired, bool bAllowEmpty = false)
	{
		const TSharedPtr<FJsonValue>* FieldValue = Object.IsValid() ? Object->Values.Find(Field) : nullptr;
		if (FieldValue && FieldValue->IsValid() && (*FieldValue)->Type == EJson::String && (*FieldValue)->TryGetString(Out))
		{
			if (bRequired && !bAllowEmpty && Out.IsEmpty())
			{
				AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, BasePath + TEXT(".") + Field, TEXT("must be a non-empty string"));
				return false;
			}
			return true;
		}
		if (bRequired || (Object.IsValid() && Object->HasField(Field)))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, BasePath + TEXT(".") + Field, TEXT("is missing or is not a string"));
		}
		Out.Reset();
		return false;
	}

	static bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Out, FAuraRoleLoadResult& Result, FName RoleId, const FString& BasePath, bool bRequired)
	{
		const TSharedPtr<FJsonValue>* FieldValue = Object.IsValid() ? Object->Values.Find(Field) : nullptr;
		if (FieldValue && FieldValue->IsValid() && (*FieldValue)->Type == EJson::Boolean && (*FieldValue)->TryGetBool(Out)) return true;
		if (bRequired || (Object.IsValid() && Object->HasField(Field)))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, BasePath + TEXT(".") + Field, TEXT("is missing or is not a boolean"));
		}
		return false;
	}

	static bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& Out, FAuraRoleLoadResult& Result, FName RoleId, const FString& BasePath)
	{
		double Value = 0.0;
		const TSharedPtr<FJsonValue>* FieldValue = Object.IsValid() ? Object->Values.Find(Field) : nullptr;
		if (!FieldValue || !FieldValue->IsValid() || (*FieldValue)->Type != EJson::Number || !(*FieldValue)->TryGetNumber(Value) || !FMath::IsFinite(Value))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, BasePath + TEXT(".") + Field, TEXT("is missing, is not numeric, or is non-finite"));
			return false;
		}
		Out = static_cast<float>(Value);
		return true;
	}

	static bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& Out, FAuraRoleLoadResult& Result, FName RoleId, const FString& BasePath, bool bRequired)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			if (bRequired || (Object.IsValid() && Object->HasField(Field)))
			{
				AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, BasePath + TEXT(".") + Field, TEXT("is missing or is not an array"));
			}
			return false;
		}
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			FString Value;
			if (!(*Values)[Index].IsValid() || (*Values)[Index]->Type != EJson::String || !(*Values)[Index]->TryGetString(Value))
			{
				AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId,
					FString::Printf(TEXT("%s.%s[%d]"), *BasePath, Field, Index), TEXT("must be a string"));
				continue;
			}
			if (!Value.IsEmpty()) Out.Add(Value);
		}
		return true;
	}

	static FGameplayTag ReadSupportedTag(const FString& Value, const TCHAR* Category, const TSet<FString>& Supported,
		FAuraRoleLoadResult& Result, FName RoleId, const FString& Path)
	{
		if (!Supported.Contains(Value) || !Value.StartsWith(FString(Category) + TEXT(".")))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, Path,
				FString::Printf(TEXT("'%s' is not a supported full %s Gameplay Tag"), *Value, Category));
			return FGameplayTag();
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Value), false);
		if (!Tag.IsValid())
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, Path,
				FString::Printf(TEXT("Gameplay Tag '%s' is not registered"), *Value));
		}
		return Tag;
	}

	static UAuraAbilityDefinition* LoadAbilityDefinition(const FString& DefPath)
	{
		if (DefPath.IsEmpty()) return nullptr;
		if (DefPath.EndsWith(TEXT(".xml")) || DefPath.Contains(TEXT("/AbilityDefinitions/")))
		{
			return UAuraAbilitySystemLibrary::LoadAbilityDefinitionFromXMLFile(DefPath);
		}
		return LoadObject<UAuraAbilityDefinition>(nullptr, *DefPath);
	}

	static bool ProjectileDefinitionExists(const FString& Id)
	{
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("ProjectileDefinitions.json")))) return false;
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content), Root) || !Root.IsValid()) return false;
		const TSharedPtr<FJsonObject>* Definitions = nullptr;
		return Root->TryGetObjectField(TEXT("projectiles"), Definitions) && Definitions && (*Definitions)->HasField(Id);
	}

	static void ValidateDefinition(UAuraAbilityDefinition* Def, const FString& DefPath, bool bPassive, bool bLMB,
		FAuraRoleLoadResult& Result, FName RoleId, const FString& JsonPath, TSet<FGameplayTag>& UsedTags, TSet<FGameplayTag>& UsedInputs, bool& bHasOffensive)
	{
		if (!Def)
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, FString::Printf(TEXT("failed to load ability definition '%s'"), *DefPath));
			return;
		}
		if (!Def->AbilityTag.IsValid()) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, TEXT("definition has no registered ability tag"));
		else if (UsedTags.Contains(Def->AbilityTag)) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, FString::Printf(TEXT("duplicates ability tag '%s'"), *Def->AbilityTag.ToString()));
		else UsedTags.Add(Def->AbilityTag);
		if (Def->InputTag.IsValid())
		{
			if (UsedInputs.Contains(Def->InputTag)) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, FString::Printf(TEXT("duplicates input slot '%s'"), *Def->InputTag.ToString()));
			else UsedInputs.Add(Def->InputTag);
		}
		if (bLMB && !Def->InputTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB"), false)))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, TEXT("LMB definition must use InputTag.LMB"));
		}
		const bool bIsPassive = Def->AbilityType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Abilities.Type.Passive"), false));
		const bool bIsOffensive = Def->AbilityType.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Abilities.Type.Offensive"), false));
		if (bPassive != bIsPassive) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, TEXT("definition grant category does not match its ability type"));
		bHasOffensive |= bIsOffensive;
		if (Def->SourceXML.Contains(TEXT("ProjectileDefinition")))
		{
			const FString Marker = TEXT("name=\"ProjectileDefinition\" value=\"");
			const int32 Start = Def->SourceXML.Find(Marker);
			const int32 ValueStart = Start == INDEX_NONE ? INDEX_NONE : Start + Marker.Len();
			const int32 End = ValueStart == INDEX_NONE ? INDEX_NONE : Def->SourceXML.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			const FString ProjectileId = End == INDEX_NONE ? FString() : Def->SourceXML.Mid(ValueStart, End - ValueStart);
			if (ProjectileId.IsEmpty() || !ProjectileDefinitionExists(ProjectileId))
			{
				AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleId, JsonPath, FString::Printf(TEXT("missing projectile definition '%s'"), *ProjectileId));
			}
		}
	}
}

FAuraRoleLoadResult UAuraAbilitySystemLibrary::ParseRoleInfoJson(const UObject* WorldContextObject, const FString& JsonContent, const FString& SourceLabel)
{
	using namespace RoleConfigPrivate;
	FAuraRoleLoadResult Result;
	Result.PublishedVersion = 2;
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$"), FString::Printf(TEXT("malformed JSON in %s"), *SourceLabel));
		return Result;
	}

	double VersionNumber = 1.0;
	if (RootObject->HasField(TEXT("roleDefinitionVersion")) && (!RootObject->TryGetNumberField(TEXT("roleDefinitionVersion"), VersionNumber) || !FMath::IsFinite(VersionNumber) || FMath::FloorToDouble(VersionNumber) != VersionNumber))
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$.roleDefinitionVersion"), TEXT("must be an integer"));
	}
	Result.DetectedVersion = static_cast<int32>(VersionNumber);
	if (Result.DetectedVersion != 1 && Result.DetectedVersion != 2)
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$.roleDefinitionVersion"), TEXT("only versions 1 and 2 are supported"));
	}
	if (Result.DetectedVersion == 1)
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Warning, NAME_None, TEXT("$.roleDefinitionVersion"), TEXT("legacy role schema migrated deterministically to version 2"));
	}

	const TArray<TSharedPtr<FJsonValue>>* RolesArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("roles"), RolesArray) || RolesArray == nullptr)
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$.roles"), TEXT("is missing or is not an array"));
		return Result;
	}

	URoleInfo* RoleInfo = NewObject<URoleInfo>(GetTransientPackage());
	RoleInfo->RoleDefinitionVersion = 2;
	Result.Candidate = RoleInfo;

	FString DefaultRoleStr;
	ReadString(RootObject, TEXT("defaultRole"), DefaultRoleStr, Result, NAME_None, TEXT("$"), true);
	RoleInfo->DefaultRole = FName(*DefaultRoleStr);

	const TSet<FString> EntityTags = { TEXT("Entity.Player"), TEXT("Entity.AmbientNPC") };
	const TSet<FString> ControlTags = { TEXT("Control.Player"), TEXT("Control.EnemyAI"), TEXT("Control.CivilianAI") };
	const TSet<FString> CombatTags = { TEXT("Combat.Unassigned"), TEXT("Combat.Magic"), TEXT("Combat.Gun"), TEXT("Combat.Civilian") };
	const TSet<FString> FactionTags = { TEXT("Faction.Player"), TEXT("Faction.Enemy"), TEXT("Faction.Civilian") };
	const TSet<FString> DeathTags = { TEXT("Death.PlayerRespawn"), TEXT("Death.EnemyLoot"), TEXT("Death.PopulationRespawn") };
	const TSet<FString> EconomyTags = { TEXT("Economy.None"), TEXT("Economy.Ambient"), TEXT("Economy.CommerceCapable") };
	const TSet<FString> InteractionTags = { TEXT("Interaction.Combatant"), TEXT("Interaction.Civilian") };
	TSet<FName> SeenRoles;

	for (int32 RoleIndex = 0; RoleIndex < RolesArray->Num(); ++RoleIndex)
	{
		const TSharedPtr<FJsonValue>& RoleValue = (*RolesArray)[RoleIndex];
		const FString BasePath = FString::Printf(TEXT("$.roles[%d]"), RoleIndex);
		const TSharedPtr<FJsonObject>* RoleObjPtr = nullptr;
		if (!RoleValue.IsValid() || !RoleValue->TryGetObject(RoleObjPtr) || !RoleObjPtr->IsValid())
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, BasePath, TEXT("must be an object"));
			continue;
		}
		const TSharedPtr<FJsonObject> RoleObj = *RoleObjPtr;

		FString RoleNameStr;
		ReadString(RoleObj, TEXT("role"), RoleNameStr, Result, NAME_None, BasePath, true);
		const FName RoleName = FName(*RoleNameStr);
		if (RoleName.IsNone()) continue;
		if (SeenRoles.Contains(RoleName))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".role"), TEXT("duplicate stable role ID; first entry retained"));
			continue;
		}
		SeenRoles.Add(RoleName);

		FRoleDefaultInfo Info;
		ReadString(RoleObj, TEXT("displayName"), Info.DisplayName, Result, RoleName, BasePath, true);

		FString Entity, Control, Combat, Faction, Death, Economy, Interaction;
		const bool bLegacyKnown = Result.DetectedVersion == 1 && (RoleName == TEXT("Aura") || RoleName == TEXT("BungeeMan"));
		auto LegacyString = [&](const TCHAR* Field, FString& Out, const TCHAR* AuraValue, const TCHAR* BungeeValue)
		{
			if (!ReadString(RoleObj, Field, Out, Result, RoleName, BasePath, !bLegacyKnown) && bLegacyKnown)
			{
				Out = RoleName == TEXT("Aura") ? AuraValue : BungeeValue;
			}
		};
		LegacyString(TEXT("entityType"), Entity, TEXT("Entity.Player"), TEXT("Entity.Player"));
		LegacyString(TEXT("controlType"), Control, TEXT("Control.Player"), TEXT("Control.Player"));
		LegacyString(TEXT("combatProfile"), Combat, TEXT("Combat.Magic"), TEXT("Combat.Gun"));
		LegacyString(TEXT("faction"), Faction, TEXT("Faction.Player"), TEXT("Faction.Player"));
		LegacyString(TEXT("deathPolicy"), Death, TEXT("Death.PlayerRespawn"), TEXT("Death.PlayerRespawn"));
		LegacyString(TEXT("economyProfile"), Economy, TEXT("Economy.None"), TEXT("Economy.None"));
		LegacyString(TEXT("interactionProfile"), Interaction, TEXT("Interaction.Combatant"), TEXT("Interaction.Combatant"));
		Info.EntityType = ReadSupportedTag(Entity, TEXT("Entity"), EntityTags, Result, RoleName, BasePath + TEXT(".entityType"));
		Info.ControlType = ReadSupportedTag(Control, TEXT("Control"), ControlTags, Result, RoleName, BasePath + TEXT(".controlType"));
		Info.CombatProfile = ReadSupportedTag(Combat, TEXT("Combat"), CombatTags, Result, RoleName, BasePath + TEXT(".combatProfile"));
		Info.Faction = ReadSupportedTag(Faction, TEXT("Faction"), FactionTags, Result, RoleName, BasePath + TEXT(".faction"));
		Info.DeathPolicy = ReadSupportedTag(Death, TEXT("Death"), DeathTags, Result, RoleName, BasePath + TEXT(".deathPolicy"));
		Info.EconomyProfile = ReadSupportedTag(Economy, TEXT("Economy"), EconomyTags, Result, RoleName, BasePath + TEXT(".economyProfile"));
		Info.InteractionProfile = ReadSupportedTag(Interaction, TEXT("Interaction"), InteractionTags, Result, RoleName, BasePath + TEXT(".interactionProfile"));

		auto LegacyBool = [&](const TCHAR* Field, bool& Out, bool DefaultValue)
		{
			if (!ReadBool(RoleObj, Field, Out, Result, RoleName, BasePath, !bLegacyKnown) && bLegacyKnown) Out = DefaultValue;
		};
		LegacyBool(TEXT("playerSelectable"), Info.bPlayerSelectable, true);
		LegacyBool(TEXT("targetable"), Info.bTargetable, true);
		LegacyBool(TEXT("canAttack"), Info.bCanAttack, true);
		LegacyBool(TEXT("canBeDamaged"), Info.bCanBeDamaged, true);
		LegacyBool(TEXT("allowFriendlyFire"), Info.bAllowFriendlyFire, false);

		FString MeshPath, AnimPath;
		ReadString(RoleObj, TEXT("mesh"), MeshPath, Result, RoleName, BasePath, true);
		ReadString(RoleObj, TEXT("animBlueprint"), AnimPath, Result, RoleName, BasePath, true);
		if (!MeshPath.IsEmpty()) Info.SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!AnimPath.IsEmpty()) Info.AnimBlueprintClass = LoadClass<UAnimInstance>(nullptr, *AnimPath);
		if (!MeshPath.IsEmpty() && !Info.SkeletalMesh) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".mesh"), FString::Printf(TEXT("failed to load '%s'"), *MeshPath));
		if (!AnimPath.IsEmpty() && !Info.AnimBlueprintClass) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".animBlueprint"), FString::Printf(TEXT("failed to load '%s'"), *AnimPath));
		if (Info.SkeletalMesh && Info.AnimBlueprintClass)
		{
			const IAnimClassInterface* AnimInterface = IAnimClassInterface::GetFromClass(Info.AnimBlueprintClass);
			if (!AnimInterface || !AnimInterface->GetTargetSkeleton() || AnimInterface->GetTargetSkeleton() != Info.SkeletalMesh->GetSkeleton())
			{
				AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".animBlueprint"), TEXT("Anim Blueprint skeleton is incompatible with the role mesh"));
			}
		}

		const bool bHasEquipment = RoleObj->HasField(TEXT("equipment"));
		const bool bHasLegacyEquipment = RoleObj->HasField(TEXT("weaponMesh")) || RoleObj->HasField(TEXT("weaponSocket")) || RoleObj->HasField(TEXT("weaponTipSocket"));
		if ((Result.DetectedVersion == 1 && bHasEquipment) || (Result.DetectedVersion == 2 && bHasLegacyEquipment))
		{
			AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment"), TEXT("mixed/legacy equipment shape is ambiguous for this schema version"));
		}
		TSharedPtr<FJsonObject> Equipment;
		if (Result.DetectedVersion == 2 && bHasEquipment)
		{
			const TSharedPtr<FJsonObject>* EquipmentPtr = nullptr;
			if (RoleObj->TryGetObjectField(TEXT("equipment"), EquipmentPtr) && EquipmentPtr) Equipment = *EquipmentPtr;
			else AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment"), TEXT("must be an object"));
		}
		const TSharedPtr<FJsonObject>& EquipmentSource = Result.DetectedVersion == 1 ? RoleObj : Equipment;
		const TCHAR* WeaponField = Result.DetectedVersion == 1 ? TEXT("weaponMesh") : TEXT("weaponMesh");
		const TCHAR* AttachField = Result.DetectedVersion == 1 ? TEXT("weaponSocket") : TEXT("attachSocket");
		const TCHAR* TipField = Result.DetectedVersion == 1 ? TEXT("weaponTipSocket") : TEXT("tipSocket");
		FString WeaponMeshPath, AttachSocket, TipSocket;
		if (EquipmentSource.IsValid())
		{
			const bool bVersion2Equipment = Result.DetectedVersion == 2;
			const FString EquipmentPath = BasePath + (bVersion2Equipment ? TEXT(".equipment") : TEXT(""));
			ReadString(EquipmentSource, WeaponField, WeaponMeshPath, Result, RoleName, EquipmentPath, bVersion2Equipment);
			ReadString(EquipmentSource, AttachField, AttachSocket, Result, RoleName, EquipmentPath, bVersion2Equipment);
			ReadString(EquipmentSource, TipField, TipSocket, Result, RoleName, EquipmentPath, bVersion2Equipment, true);
		}
		if (!WeaponMeshPath.IsEmpty()) Info.WeaponMesh = LoadObject<USkeletalMesh>(nullptr, *WeaponMeshPath);
		Info.WeaponSocketName = FName(*AttachSocket);
		Info.WeaponTipSocketName = FName(*TipSocket);
		if (!WeaponMeshPath.IsEmpty() && !Info.WeaponMesh) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment.weaponMesh"), FString::Printf(TEXT("failed to load '%s'"), *WeaponMeshPath));
		if (!AttachSocket.IsEmpty() && Info.SkeletalMesh && !Info.SkeletalMesh->FindSocket(Info.WeaponSocketName)) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment.attachSocket"), TEXT("socket does not exist on body mesh"));
		if (!TipSocket.IsEmpty() && Info.WeaponMesh && !Info.WeaponMesh->FindSocket(Info.WeaponTipSocketName)) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment.tipSocket"), TEXT("socket does not exist on weapon mesh"));
		if (!WeaponMeshPath.IsEmpty() && AttachSocket.IsEmpty()) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".equipment.attachSocket"), TEXT("equipment requires an attach socket"));

		FString Optional;
		ReadString(RoleObj, TEXT("leftHandSocket"), Optional, Result, RoleName, BasePath, false); Info.LeftHandSocketName = FName(*Optional);
		ReadString(RoleObj, TEXT("rightHandSocket"), Optional, Result, RoleName, BasePath, false); Info.RightHandSocketName = FName(*Optional);
		ReadString(RoleObj, TEXT("tailSocket"), Optional, Result, RoleName, BasePath, false); Info.TailSocketName = FName(*Optional);
		FString DissolvePath, WeaponDissolvePath, BloodPath, DeathSoundPath;
		ReadString(RoleObj, TEXT("dissolve"), DissolvePath, Result, RoleName, BasePath, false);
		ReadString(RoleObj, TEXT("weaponDissolve"), WeaponDissolvePath, Result, RoleName, BasePath, false);
		ReadString(RoleObj, TEXT("bloodEffect"), BloodPath, Result, RoleName, BasePath, false);
		ReadString(RoleObj, TEXT("deathSound"), DeathSoundPath, Result, RoleName, BasePath, false);
		if (!WeaponMeshPath.IsEmpty()) Info.WeaponMesh = LoadObject<USkeletalMesh>(nullptr, *WeaponMeshPath);
		if (!DissolvePath.IsEmpty()) Info.DissolveMaterialInstance = LoadObject<UMaterialInstance>(nullptr, *DissolvePath);
		if (!WeaponDissolvePath.IsEmpty()) Info.WeaponDissolveMaterialInstance = LoadObject<UMaterialInstance>(nullptr, *WeaponDissolvePath);
		if (!BloodPath.IsEmpty()) Info.BloodEffect = LoadObject<UNiagaraSystem>(nullptr, *BloodPath);
		if (!DeathSoundPath.IsEmpty()) Info.DeathSound = LoadObject<USoundBase>(nullptr, *DeathSoundPath);

		const TSharedPtr<FJsonObject>* AttrsPtr = nullptr;
		if (RoleObj->TryGetObjectField(TEXT("attributes"), AttrsPtr) && AttrsPtr && AttrsPtr->IsValid())
		{
			ReadNumber(*AttrsPtr, TEXT("strength"), Info.Strength, Result, RoleName, BasePath + TEXT(".attributes"));
			ReadNumber(*AttrsPtr, TEXT("intelligence"), Info.Intelligence, Result, RoleName, BasePath + TEXT(".attributes"));
			ReadNumber(*AttrsPtr, TEXT("resilience"), Info.Resilience, Result, RoleName, BasePath + TEXT(".attributes"));
			ReadNumber(*AttrsPtr, TEXT("vigor"), Info.Vigor, Result, RoleName, BasePath + TEXT(".attributes"));
		}
		else AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".attributes"), TEXT("is missing or is not an object"));

		TArray<FString> StartupClasses, PassiveClasses, UnlockableClasses, StartupDefs, PassiveDefs;
		const bool bArraysRequired = Result.DetectedVersion == 2;
		ReadStringArray(RoleObj, TEXT("startupAbilities"), StartupClasses, Result, RoleName, BasePath, bArraysRequired);
		ReadStringArray(RoleObj, TEXT("startupPassiveAbilities"), PassiveClasses, Result, RoleName, BasePath, bArraysRequired);
		ReadStringArray(RoleObj, TEXT("unlockableAbilities"), UnlockableClasses, Result, RoleName, BasePath, bArraysRequired);
		ReadStringArray(RoleObj, TEXT("startupAbilityDefinitions"), StartupDefs, Result, RoleName, BasePath, bArraysRequired);
		ReadStringArray(RoleObj, TEXT("startupPassiveAbilityDefinitions"), PassiveDefs, Result, RoleName, BasePath, bArraysRequired);
		TSet<FString> UsedClassPaths;
		auto LoadClasses = [&](const TArray<FString>& Paths, TArray<TSubclassOf<UGameplayAbility>>& Output, const FString& Field)
		{
			for (int32 Index = 0; Index < Paths.Num(); ++Index)
			{
				if (UsedClassPaths.Contains(Paths[Index])) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, FString::Printf(TEXT("%s.%s[%d]"), *BasePath, *Field, Index), TEXT("duplicates another ability class path"));
				UsedClassPaths.Add(Paths[Index]);
				const TSubclassOf<UGameplayAbility> Class = LoadClass<UGameplayAbility>(nullptr, *Paths[Index]);
				if (!Class) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, FString::Printf(TEXT("%s.%s[%d]"), *BasePath, *Field, Index), FString::Printf(TEXT("failed to load '%s'"), *Paths[Index]));
				else Output.Add(Class);
			}
		};
		LoadClasses(StartupClasses, Info.StartupAbilities, TEXT("startupAbilities"));
		LoadClasses(PassiveClasses, Info.StartupPassiveAbilities, TEXT("startupPassiveAbilities"));
		LoadClasses(UnlockableClasses, Info.UnlockableAbilities, TEXT("unlockableAbilities"));

		TSet<FGameplayTag> UsedTags, UsedInputs;
		bool bHasOffensive = false;
		for (int32 Index = 0; Index < StartupDefs.Num(); ++Index)
		{
			UAuraAbilityDefinition* Def = LoadAbilityDefinition(StartupDefs[Index]);
			ValidateDefinition(Def, StartupDefs[Index], false, false, Result, RoleName, FString::Printf(TEXT("%s.startupAbilityDefinitions[%d]"), *BasePath, Index), UsedTags, UsedInputs, bHasOffensive);
			if (Def) { Info.StartupAbilityDefinitionPaths.Add(StartupDefs[Index]); Info.StartupAbilityDefinitions.Add(Def); }
		}
		for (int32 Index = 0; Index < PassiveDefs.Num(); ++Index)
		{
			UAuraAbilityDefinition* Def = LoadAbilityDefinition(PassiveDefs[Index]);
			ValidateDefinition(Def, PassiveDefs[Index], true, false, Result, RoleName, FString::Printf(TEXT("%s.startupPassiveAbilityDefinitions[%d]"), *BasePath, Index), UsedTags, UsedInputs, bHasOffensive);
			if (Def) { Info.StartupPassiveAbilityDefinitionPaths.Add(PassiveDefs[Index]); Info.StartupPassiveAbilityDefinitions.Add(Def); }
		}
		FString LMBDefPath, LMBAbilityPath;
		ReadString(RoleObj, TEXT("lmbAbilityDefinition"), LMBDefPath, Result, RoleName, BasePath, Result.DetectedVersion == 2, true);
		ReadString(RoleObj, TEXT("lmbAbility"), LMBAbilityPath, Result, RoleName, BasePath, Result.DetectedVersion == 2, true);
		if (!LMBDefPath.IsEmpty() && !LMBAbilityPath.IsEmpty()) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".lmbAbility"), TEXT("lmbAbility and lmbAbilityDefinition are mutually exclusive"));
		if (!LMBDefPath.IsEmpty())
		{
			Info.DefaultLMBAbilityDefinitionPath = LMBDefPath;
			Info.DefaultLMBAbilityDefinition = LoadAbilityDefinition(LMBDefPath);
			ValidateDefinition(Cast<UAuraAbilityDefinition>(Info.DefaultLMBAbilityDefinition), LMBDefPath, false, true, Result, RoleName, BasePath + TEXT(".lmbAbilityDefinition"), UsedTags, UsedInputs, bHasOffensive);
		}
		if (!LMBAbilityPath.IsEmpty())
		{
			Info.DefaultLMBAbility = LoadClass<UGameplayAbility>(nullptr, *LMBAbilityPath);
			if (!Info.DefaultLMBAbility) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".lmbAbility"), FString::Printf(TEXT("failed to load '%s'"), *LMBAbilityPath));
			else bHasOffensive = true;
		}
		if (Info.bCanAttack && !bHasOffensive) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".canAttack"), TEXT("attacking role needs at least one valid offensive spawn grant"));
		if (!Info.bCanAttack && bHasOffensive) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".canAttack"), TEXT("non-attacking role cannot have offensive spawn grants"));
		if (RoleName == TEXT("Civilian") && UnlockableClasses.Num() > 0) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".unlockableAbilities"), TEXT("Civilian offensive unlock catalog must be empty"));
		const bool bPlayerIdentity = Entity == TEXT("Entity.Player") && Control == TEXT("Control.Player") && Faction == TEXT("Faction.Player") && Death == TEXT("Death.PlayerRespawn");
		const bool bCivilianIdentity = Entity == TEXT("Entity.AmbientNPC") && Control == TEXT("Control.CivilianAI") && Faction == TEXT("Faction.Civilian") && Death == TEXT("Death.PopulationRespawn") && Combat == TEXT("Combat.Civilian");
		if (!bPlayerIdentity && !bCivilianIdentity) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath, TEXT("identity/profile tag combination is not supported"));
		if (Info.bPlayerSelectable && !bPlayerIdentity) AddIssue(Result, EAuraRoleValidationSeverity::Error, RoleName, BasePath + TEXT(".playerSelectable"), TEXT("only Entity.Player/Control.Player roles may be player-selectable"));

		RoleInfo->RoleInformation.Add(RoleName, Info);
	}

	FString DefaultError;
	if (!ValidatePlayerRoleSelection(RoleInfo, RoleInfo->DefaultRole, DefaultError))
	{
		AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$.defaultRole"), DefaultError);
	}
	Result.bCanPublish = !Result.Issues.ContainsByPredicate([](const FAuraRoleValidationIssue& Issue) { return Issue.Severity == EAuraRoleValidationSeverity::Error; });
	return Result;
}

FAuraRoleLoadResult UAuraAbilitySystemLibrary::LoadRoleInfoCandidate(const UObject* WorldContextObject)
{
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("RoleConfig.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		FAuraRoleLoadResult Result;
		RoleConfigPrivate::AddIssue(Result, EAuraRoleValidationSeverity::Error, NAME_None, TEXT("$"), FString::Printf(TEXT("failed to read %s"), *ConfigPath));
		return Result;
	}
	FAuraRoleLoadResult Result = ParseRoleInfoJson(WorldContextObject, JsonContent, ConfigPath);
	if (!Result.bCanPublish)
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Candidate diagnostics: %s"), *Result.ToLogString());
	}
	return Result;
}

URoleInfo* UAuraAbilitySystemLibrary::LoadRoleInfoFromConfig(const UObject* WorldContextObject)
{
	const FAuraRoleLoadResult Result = LoadRoleInfoCandidate(WorldContextObject);
	if (!Result.bCanPublish)
	{
		UE_LOG(LogAura, Error, TEXT("[RoleConfig] Candidate rejected atomically: %s"), *Result.ToLogString());
		return nullptr;
	}
	UE_LOG(LogAura, Log, TEXT("[RoleConfig] Candidate valid: %s"), *Result.ToLogString());
	return Result.Candidate;
}

bool UAuraAbilitySystemLibrary::TryPublishRoleInfo(URoleInfo*& Current, const FAuraRoleLoadResult& Result)
{
	if (!Result.bCanPublish || !Result.Candidate) return false;
	Current = Result.Candidate;
	return true;
}

bool UAuraAbilitySystemLibrary::ValidatePlayerRoleSelection(const URoleInfo* RoleInfo, FName RoleId, FString& OutError)
{
	if (!RoleInfo)
	{
		OutError = TEXT("The authoritative role service is unavailable. Check RoleConfig.json validation errors.");
		return false;
	}
	if (RoleId.IsNone())
	{
		OutError = TEXT("No role was requested. Choose a valid player role and reconnect.");
		return false;
	}
	if (!RoleInfo->RoleInformation.Contains(RoleId))
	{
		OutError = FString::Printf(TEXT("Role '%s' is unknown. Refresh the local role configuration and reconnect."), *RoleId.ToString());
		return false;
	}
	if (!RoleInfo->IsPlayerRoleSelectable(RoleId))
	{
		OutError = FString::Printf(TEXT("Role '%s' is not a configured, player-selectable Player role."), *RoleId.ToString());
		return false;
	}
	OutError.Reset();
	return true;
}

ULootTiers* UAuraAbilitySystemLibrary::GetLootTiers(const UObject* WorldContextObject)
{
	const AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return nullptr;
	return AuraGameMode->LootTiers;
}

bool UAuraAbilitySystemLibrary::IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->IsBlockedHit();
	}
	return false;
}

bool UAuraAbilitySystemLibrary::IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->IsSuccessfulDebuff();
	}
	return false;
}

float UAuraAbilitySystemLibrary::GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetDebuffDamage();
	}
	return 0.f;
}

float UAuraAbilitySystemLibrary::GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetDebuffDuration();
	}
	return 0.f;
}

float UAuraAbilitySystemLibrary::GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetDebuffFrequency();
	}
	return 0.f;
}

FGameplayTag UAuraAbilitySystemLibrary::GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		if (AuraEffectContext->GetDamageType().IsValid())
		{
			return *AuraEffectContext->GetDamageType();
		}
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemLibrary::GetAbilityTag(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetAbilityTag();
	}
	return FGameplayTag();
}

FName UAuraAbilitySystemLibrary::GetSourceRoleId(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetSourceRoleId();
	}
	return NAME_None;
}

AController* UAuraAbilitySystemLibrary::GetSourceController(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetSourceController();
	}
	return nullptr;
}

APlayerState* UAuraAbilitySystemLibrary::GetSourcePlayerState(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetSourcePlayerState();
	}
	return nullptr;
}

FName UAuraAbilitySystemLibrary::GetBattleZoneId(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetBattleZoneId();
	}
	return NAME_None;
}

FName UAuraAbilitySystemLibrary::GetBattleEventId(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetBattleEventId();
	}
	return NAME_None;
}

FVector UAuraAbilitySystemLibrary::GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetDeathImpulse();
	}
	return FVector::ZeroVector;
}

FVector UAuraAbilitySystemLibrary::GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetKnockbackForce();
	}
	return FVector::ZeroVector;
}

bool UAuraAbilitySystemLibrary::IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->IsCriticalHit();
	}
	return false;
}

bool UAuraAbilitySystemLibrary::IsRadialDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->IsRadialDamage();
	}
	return false;
}

float UAuraAbilitySystemLibrary::GetRadialDamageInnerRadius(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetRadialDamageInnerRadius();
	}
	return 0.f;
}

float UAuraAbilitySystemLibrary::GetRadialDamageOuterRadius(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetRadialDamageOuterRadius();
	}
	return 0.f;
}

FVector UAuraAbilitySystemLibrary::GetRadialDamageOrigin(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraEffectContext = static_cast<const FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return AuraEffectContext->GetRadialDamageOrigin();
	}
	return FVector::ZeroVector;
}

void UAuraAbilitySystemLibrary::SetIsBlockedHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetIsBlockedHit(bInIsBlockedHit);
	}
}

void UAuraAbilitySystemLibrary::SetIsCriticalHit(FGameplayEffectContextHandle& EffectContextHandle,
	bool bInIsCriticalHit)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetIsCriticalHit(bInIsCriticalHit);
	}
}

void UAuraAbilitySystemLibrary::SetIsSuccessfulDebuff(FGameplayEffectContextHandle& EffectContextHandle,
	bool bInSuccessfulDebuff)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetIsSuccessfulDebuff(bInSuccessfulDebuff);
	}
}

void UAuraAbilitySystemLibrary::SetDebuffDamage(FGameplayEffectContextHandle& EffectContextHandle, float InDamage)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetDebuffDamage(InDamage);
	}
}

void UAuraAbilitySystemLibrary::SetDebuffDuration(FGameplayEffectContextHandle& EffectContextHandle, float InDuration)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetDebuffDuration(InDuration);
	}
}

void UAuraAbilitySystemLibrary::SetDebuffFrequency(FGameplayEffectContextHandle& EffectContextHandle, float InFrequency)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetDebuffFrequency(InFrequency);
	}
}

void UAuraAbilitySystemLibrary::SetDamageType(FGameplayEffectContextHandle& EffectContextHandle,
	const FGameplayTag& InDamageType)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		const TSharedPtr<FGameplayTag> DamageType = MakeShared<FGameplayTag>(InDamageType);
		AuraEffectContext->SetDamageType(DamageType);
	}
}

void UAuraAbilitySystemLibrary::SetAbilityTag(FGameplayEffectContextHandle& EffectContextHandle,
	const FGameplayTag& InAbilityTag)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetAbilityTag(InAbilityTag);
	}
}

void UAuraAbilitySystemLibrary::SetSourceRoleId(FGameplayEffectContextHandle& EffectContextHandle, FName InSourceRoleId)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetSourceRoleId(InSourceRoleId);
	}
}

void UAuraAbilitySystemLibrary::SetSourceController(FGameplayEffectContextHandle& EffectContextHandle, AController* InSourceController)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetSourceController(InSourceController);
	}
}

void UAuraAbilitySystemLibrary::SetSourcePlayerState(FGameplayEffectContextHandle& EffectContextHandle, APlayerState* InSourcePlayerState)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetSourcePlayerState(InSourcePlayerState);
	}
}

void UAuraAbilitySystemLibrary::SetBattleZoneId(FGameplayEffectContextHandle& EffectContextHandle, FName InBattleZoneId)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetBattleZoneId(InBattleZoneId);
	}
}

void UAuraAbilitySystemLibrary::SetBattleEventId(FGameplayEffectContextHandle& EffectContextHandle, FName InBattleEventId)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetBattleEventId(InBattleEventId);
	}
}

void UAuraAbilitySystemLibrary::SetDeathImpulse(FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InImpulse)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetDeathImpulse(InImpulse);
	}
}

void UAuraAbilitySystemLibrary::SetKnockbackForce(FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InForce)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetKnockbackForce(InForce);
	}
}

void UAuraAbilitySystemLibrary::SetIsRadialDamage(FGameplayEffectContextHandle& EffectContextHandle,
	bool bInIsRadialDamage)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetIsRadialDamage(bInIsRadialDamage);
	}
}

void UAuraAbilitySystemLibrary::SetRadialDamageInnerRadius(FGameplayEffectContextHandle& EffectContextHandle,
	float InInnerRadius)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetRadialDamageInnerRadius(InInnerRadius);
	}
}

void UAuraAbilitySystemLibrary::SetRadialDamageOuterRadius(FGameplayEffectContextHandle& EffectContextHandle,
	float InOuterRadius)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetRadialDamageOuterRadius(InOuterRadius);
	}
}

void UAuraAbilitySystemLibrary::SetRadialDamageOrigin(FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InOrigin)
{
	if (FAuraGameplayEffectContext* AuraEffectContext = static_cast<FAuraGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		AuraEffectContext->SetRadialDamageOrigin(InOrigin);
	}
}

void UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(const UObject* WorldContextObject,
                                                           TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius,
                                                           const FVector& SphereOrigin)
{
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);
	
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SphereOrigin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);
			for (FOverlapResult& Overlap : Overlaps)
			{
				AActor* OverlapActor = Overlap.GetActor();
				if (!OverlapActor || !OverlapActor->Implements<UCombatInterface>())
				{
					continue;
				}

				AActor* AvatarActor = nullptr;
				if (OverlapActor->GetClass()->IsNative())
				{
					if (ICombatInterface* NativeCombat = Cast<ICombatInterface>(OverlapActor))
					{
						AvatarActor = NativeCombat->GetAvatar_Implementation();
					}
				}
				else
				{
					AvatarActor = ICombatInterface::Execute_GetAvatar(OverlapActor);
				}

				if (AvatarActor && !ICombatInterface::Execute_IsDead(OverlapActor))
				{
					OutOverlappingActors.AddUnique(AvatarActor);
				}
			}
	}
}

void UAuraAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin)
{
	if (Actors.Num() <= MaxTargets)
	{
		OutClosestTargets = Actors;
		return;
	}

	TArray<AActor*> ActorsToCheck = Actors;
	int32 NumTargetsFound = 0;

	while (NumTargetsFound < MaxTargets)
	{
		if (ActorsToCheck.Num() == 0) break;
		double ClosestDistance = TNumericLimits<double>::Max();
		AActor* ClosestActor;
		for (AActor* PotentialTarget : ActorsToCheck)
		{
			const double Distance = (PotentialTarget->GetActorLocation() - Origin).Length();
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestActor = PotentialTarget;
			}
		}
		ActorsToCheck.Remove(ClosestActor);
		OutClosestTargets.AddUnique(ClosestActor);
		++NumTargetsFound;
	}
}

bool UAuraAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	FAuraCombatRuleContext Context;
	Context.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	Context.TrustedWorldContext = FirstActor;
	Context.SourceActor = FirstActor;
	Context.TargetActor = SecondActor;
	const FAuraCombatRuleResult Result = FAuraCombatRules::CanDamage(FirstActor, SecondActor, Context);
	return Result.bCanDamage;
}

AActor* UAuraAbilitySystemLibrary::GetSafeAvatarActor(const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		return nullptr;
	}

	return AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
}

FGameplayEffectContextHandle UAuraAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	FGameplayEffectContextHandle InvalidContext;

	UAbilitySystemComponent* SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
	UAbilitySystemComponent* TargetASC = DamageEffectParams.TargetAbilitySystemComponent;
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		UE_LOG(LogAura, Warning, TEXT("[DamageBoundary] Rejected damage: source or target ASC is invalid."));
		return InvalidContext;
	}

	const AActor* SourceAvatarActor = GetSafeAvatarActor(SourceASC);
	const AActor* TargetAvatarActor = GetSafeAvatarActor(TargetASC);
	if (!IsValid(SourceAvatarActor) || !IsValid(TargetAvatarActor)
		|| !SourceAvatarActor->HasAuthority() || !TargetAvatarActor->HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[DamageBoundary] Rejected damage: source/target avatar is invalid or non-authoritative. Source=%s Target=%s"),
			*GetNameSafe(SourceAvatarActor), *GetNameSafe(TargetAvatarActor));
		return InvalidContext;
	}

	FAuraCombatRuleContext RuleContext = DamageEffectParams.CombatRuleContext;
	RuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	RuleContext.TrustedWorldContext = SourceAvatarActor;
	RuleContext.SourceActor = SourceAvatarActor;
	RuleContext.TargetActor = TargetAvatarActor;
	const FAuraCombatRuleResult RuleResult = FAuraCombatRules::CanDamage(SourceAvatarActor, TargetAvatarActor, RuleContext);
	if (!RuleResult.bCanDamage)
	{
		UE_LOG(LogAura, Verbose, TEXT("[DamageBoundary] Rejected damage Source=%s Target=%s Reason=%s Ability=%s"),
			*GetNameSafe(SourceAvatarActor),
			*GetNameSafe(TargetAvatarActor),
			*StaticEnum<EAuraCombatRuleRejectionReason>()->GetValueAsString(RuleResult.RejectionReason),
			*DamageEffectParams.AbilityTag.ToString());
		return InvalidContext;
	}

	FName SourceRoleId = NAME_None;
	if (const AAuraCharacterBase* SourceCharacter = Cast<AAuraCharacterBase>(SourceAvatarActor))
	{
		SourceRoleId = SourceCharacter->GetCharacterRole();
	}

	AController* SourceController = nullptr;
	if (const APawn* SourcePawn = Cast<APawn>(SourceAvatarActor))
	{
		SourceController = SourcePawn->GetController();
	}
	if (!SourceController && SourceASC->AbilityActorInfo.IsValid())
	{
		SourceController = SourceASC->AbilityActorInfo->PlayerController.Get();
	}
	APlayerState* SourcePlayerState = SourceController
		? SourceController->PlayerState.Get()
		: Cast<APlayerState>(const_cast<AActor*>(SourceAvatarActor));
	
	FGameplayEffectContextHandle EffectContexthandle = SourceASC->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	SetAbilityTag(EffectContexthandle, DamageEffectParams.AbilityTag);
	SetDamageType(EffectContexthandle, DamageEffectParams.DamageType);
	SetSourceRoleId(EffectContexthandle, SourceRoleId);
	SetSourceController(EffectContexthandle, SourceController);
	SetSourcePlayerState(EffectContexthandle, SourcePlayerState);
	FName BattleZoneId = RuleContext.BattleZoneId;
	FName BattleEventId = RuleContext.BattleEventId;
	if (RuleContext.HasPolicySnapshot())
	{
		if (BattleZoneId.IsNone()) BattleZoneId = RuleContext.GetPolicySnapshot().GetBattleZoneId();
		if (BattleEventId.IsNone()) BattleEventId = RuleContext.GetPolicySnapshot().GetBattleEventId();
	}
	SetBattleZoneId(EffectContexthandle, BattleZoneId);
	SetBattleEventId(EffectContexthandle, BattleEventId);
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);

	SetIsRadialDamage(EffectContexthandle, DamageEffectParams.bIsRadialDamage);
	SetRadialDamageInnerRadius(EffectContexthandle, DamageEffectParams.RadialDamageInnerRadius);
	SetRadialDamageOuterRadius(EffectContexthandle, DamageEffectParams.RadialDamageOuterRadius);
	SetRadialDamageOrigin(EffectContexthandle, DamageEffectParams.RadialDamageOrigin);
	
	const TSubclassOf<UGameplayEffect> EffectiveGEClass = DamageEffectParams.DamageGameplayEffectClass
		? TSubclassOf<UGameplayEffect>(DamageEffectParams.DamageGameplayEffectClass)
		: TSubclassOf<UGameplayEffect>(UAuraDamageGameplayEffect::StaticClass());

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectiveGEClass, DamageEffectParams.AbilityLevel, EffectContexthandle);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[DamageBoundary] Rejected damage: failed to create gameplay effect spec. Ability=%s"),
			*DamageEffectParams.AbilityTag.ToString());
		return InvalidContext;
	}

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Frequency, DamageEffectParams.DebuffFrequency);
	
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return EffectContexthandle;
}

TArray<FRotator> UAuraAbilitySystemLibrary::EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators)
{
	TArray<FRotator> Rotators;
	
	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumRotators > 1)
	{
		const float DeltaSpread = Spread / (NumRotators - 1);
		for (int32 i = 0; i < NumRotators; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
			Rotators.Add(Direction.Rotation());
		}
	}
	else
	{
		Rotators.Add(Forward.Rotation());
	}
	return Rotators;
}

TArray<FVector> UAuraAbilitySystemLibrary::EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors)
{
	TArray<FVector> Vectors;
	
	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumVectors > 1)
	{
		const float DeltaSpread = Spread / (NumVectors - 1);
		for (int32 i = 0; i < NumVectors; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
			Vectors.Add(Direction);
		}
	}
	else
	{
		Vectors.Add(Forward);
	}
	return Vectors;
}
