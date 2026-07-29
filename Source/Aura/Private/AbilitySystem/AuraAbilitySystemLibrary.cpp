// Copyright Druid Mechanics


#include "AbilitySystem/AuraAbilitySystemLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityTypes.h"
#include "AuraGameplayTags.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Game/AuraGameModeBase.h"
#include "Game/LoadScreenSaveGame.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AuraPlayerState.h"
#include "UI/HUD/AuraHUD.h"
#include "UI/WidgetController/AuraWidgetController.h"

// Role config (JSON)
#include "AbilitySystem/Data/RoleInfo.h"
#include "AuraDamageGameplayEffect.h"
#include "AuraAttributeGameplayEffect.h"
#include "AbilitySystem/Data/AbilityInfo.h"

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

void UAuraAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	// All attribute GEs are now C++ (UAuraAttributeGameplayEffect) with SetByCaller magnitudes.
	// Primary: zero defaults (legacy path didn't use SetByCaller for per-class; the role config path handles real values).
	// Secondary/Vital/Resistance: loaded from GameplayEffects.json by the caller's LoadAndApplySecondaryAttributes.
	FGameplayEffectContextHandle PrimaryContext = ASC->MakeEffectContext();
	PrimaryContext.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimarySpec = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), Level, PrimaryContext);
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
}

void UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ULoadScreenSaveGame* SaveGame)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	const AActor* SourceAvatarActor = ASC->GetAvatarActor();

	// Primary attributes via C++ SetByCaller GE, magnitudes from save data.
	FGameplayEffectContextHandle EffectContexthandle = ASC->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UAuraAttributeGameplayEffect::StaticClass(), 1.f, EffectContexthandle);

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
}

void UAuraAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return;
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

URuntimeAbilityInfo* UAuraAbilitySystemLibrary::GetRuntimeAbilityInfo(const UObject* WorldContextObject)
{
	// Process-lifetime cache for pure clients (no GameMode)
	static TStrongObjectPtr<URuntimeAbilityInfo> GClientAbilityInfoCache;

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
		// Return empty object so callers don't crash
		return NewObject<URuntimeAbilityInfo>(GetTransientPackage());
	}

	URuntimeAbilityInfo* Info = NewObject<URuntimeAbilityInfo>(GetTransientPackage());
	if (!Info->LoadFromJSON(JSONContent))
	{
		UE_LOG(LogAura, Error, TEXT("[AbilityInfo] Failed to parse AbilityInfo.json"));
	}
	else
	{
		UE_LOG(LogAura, Log, TEXT("[AbilityInfo] Successfully loaded AbilityInfo.json from: %s"), *JSONPath);
	}

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
	return Definition;
}

UAbilityInfo* UAuraAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	// DEPRECATED: Legacy UAsset-based ability info. Kept for backward compatibility.
	// New code should call GetRuntimeAbilityInfo() instead.
	const AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return nullptr;
	return AuraGameMode->AbilityInfo;
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
	// Drop the pure-client static cache so the next GetRoleInfo call on a client rebuilds it.
	RoleConfigReloadPrivate::GClientRoleInfoCache.Reset();

	URoleInfo* Reloaded = nullptr;

	// On the server (GameMode present), refresh the authoritative AAuraGameModeBase::RoleInfo
	// that every server-side spawn/login reads. The GameMode keeps its own poll timer, but this
	// path also covers a direct console-command invocation on a listen/PIE server.
	if (AAuraGameModeBase* AuraGameMode = Cast<AAuraGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		AuraGameMode->RoleInfo = LoadRoleInfoFromConfig(WorldContextObject);
		Reloaded = AuraGameMode->RoleInfo;
	}
	else
	{
		// Pure client (no GameMode): rebuild the static cache immediately so the next
		// GetDefaultRole/GetRoleInfo call returns the new config without waiting on a poll.
		Reloaded = LoadRoleInfoFromConfig(WorldContextObject);
		RoleConfigReloadPrivate::GClientRoleInfoCache.Reset(Reloaded);
	}

	if (Reloaded)
	{
		UE_LOG(LogAura, Log, TEXT("[RoleConfig] Reloaded: %d role(s), defaultRole='%s'."),
			Reloaded->RoleInformation.Num(),
			*Reloaded->DefaultRole.ToString());
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Reload requested but LoadRoleInfoFromConfig returned null; keeping prior state."));
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
	static void LoadAbilityClasses(const TArray<TSharedPtr<FJsonValue>>& Paths, TArray<TSubclassOf<UGameplayAbility>>& OutClasses, const FString& RoleName, const FString& FieldLabel)
	{
		for (const TSharedPtr<FJsonValue>& PathValue : Paths)
		{
			if (!PathValue.IsValid()) continue;
			const FString Path = PathValue->AsString();
			if (Path.IsEmpty()) continue;
			const TSubclassOf<UGameplayAbility> Loaded = LoadClass<UGameplayAbility>(nullptr, *Path);
			if (Loaded)
			{
				OutClasses.AddUnique(Loaded);
			}
			else
			{
				UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load %s class '%s'."), *RoleName, *FieldLabel, *Path);
			}
		}
	}

	static UAuraAbilityDefinition* LoadAbilityDefinition(const FString& DefPath, const FString& RoleName)
	{
		if (DefPath.IsEmpty())
		{
			return nullptr;
		}

		// Prioritize XML loading (new data-driven approach)
		if (DefPath.EndsWith(TEXT(".xml")) || DefPath.Contains(TEXT("/AbilityDefinitions/")))
		{
			// Use the centralized XML loader (mirrors BehaviorU pattern)
			if (UAuraAbilityDefinition* Def = UAuraAbilitySystemLibrary::LoadAbilityDefinitionFromXMLFile(DefPath))
			{
				UE_LOG(LogAura, Log, TEXT("[RoleConfig] Role '%s': loaded definition XML '%s' (Tag=%s)."),
					*RoleName, *DefPath, *Def->AbilityTag.ToString());
				return Def;
			}
			UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load definition XML '%s'."), *RoleName, *DefPath);
			return nullptr;
		}

		// Fallback: legacy UAsset loading (for backward compatibility during migration)
		if (UAuraAbilityDefinition* Def = LoadObject<UAuraAbilityDefinition>(nullptr, *DefPath))
		{
			UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': loaded LEGACY UAsset definition '%s'. Migrate to XML!"), *RoleName, *DefPath);
			return Def;
		}

		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load definition '%s' (not XML or UAsset)."), *RoleName, *DefPath);
		return nullptr;
	}
}

URoleInfo* UAuraAbilitySystemLibrary::LoadRoleInfoFromConfig(const UObject* WorldContextObject)
{
	const FString ConfigPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("RoleConfig.json"));
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Failed to read role config file: %s"), *ConfigPath);
		return nullptr;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Failed to parse role config JSON: %s"), *ConfigPath);
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* RolesArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("roles"), RolesArray) || RolesArray == nullptr)
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] No 'roles' array in %s"), *ConfigPath);
		return nullptr;
	}

	URoleInfo* RoleInfo = NewObject<URoleInfo>(GetTransientPackage());

	// Top-level "defaultRole": role used when no role is explicitly chosen (no save / no UI).
	// Stored as a name string; validated against the loaded roles below.
	FString DefaultRoleStr;
	if (RootObject->TryGetStringField(TEXT("defaultRole"), DefaultRoleStr) && !DefaultRoleStr.IsEmpty())
	{
		RoleInfo->DefaultRole = FName(*DefaultRoleStr);
	}

	for (const TSharedPtr<FJsonValue>& RoleValue : *RolesArray)
	{
		const TSharedPtr<FJsonObject>* RoleObjPtr = nullptr;
		if (!RoleValue.IsValid() || !RoleValue->TryGetObject(RoleObjPtr) || !RoleObjPtr->IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject> RoleObj = *RoleObjPtr;

		FString RoleNameStr;
		if (!RoleObj->TryGetStringField(TEXT("role"), RoleNameStr) || RoleNameStr.IsEmpty())
		{
			UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role entry missing 'role' name; skipping."));
			continue;
		}
		const FName RoleName = FName(*RoleNameStr);

		FRoleDefaultInfo Info;

		// Visuals: asset paths resolved synchronously (loader runs once at GameMode BeginPlay).
		const FString MeshPath = RoleObj->GetStringField(TEXT("mesh"));
		const FString AnimPath = RoleObj->GetStringField(TEXT("animBlueprint"));
		const FString WeaponMeshPath = RoleObj->GetStringField(TEXT("weaponMesh"));
		const FString DissolvePath = RoleObj->GetStringField(TEXT("dissolve"));
		const FString WeaponDissolvePath = RoleObj->GetStringField(TEXT("weaponDissolve"));
		const FString BloodPath = RoleObj->GetStringField(TEXT("bloodEffect"));
		const FString DeathSoundPath = RoleObj->GetStringField(TEXT("deathSound"));

		if (!MeshPath.IsEmpty()) Info.SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!AnimPath.IsEmpty()) Info.AnimBlueprintClass = LoadClass<UAnimInstance>(nullptr, *AnimPath);
		if (!WeaponMeshPath.IsEmpty()) Info.WeaponMesh = LoadObject<USkeletalMesh>(nullptr, *WeaponMeshPath);
		if (!DissolvePath.IsEmpty()) Info.DissolveMaterialInstance = LoadObject<UMaterialInstance>(nullptr, *DissolvePath);
		if (!WeaponDissolvePath.IsEmpty()) Info.WeaponDissolveMaterialInstance = LoadObject<UMaterialInstance>(nullptr, *WeaponDissolvePath);
		if (!BloodPath.IsEmpty()) Info.BloodEffect = LoadObject<UNiagaraSystem>(nullptr, *BloodPath);
		if (!DeathSoundPath.IsEmpty()) Info.DeathSound = LoadObject<USoundBase>(nullptr, *DeathSoundPath);

		if (!MeshPath.IsEmpty() && Info.SkeletalMesh == nullptr) UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load mesh '%s'."), *RoleNameStr, *MeshPath);
		if (!AnimPath.IsEmpty() && Info.AnimBlueprintClass == nullptr) UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load anim blueprint '%s'."), *RoleNameStr, *AnimPath);

		// Sockets.
		const FString WeaponSocket = RoleObj->GetStringField(TEXT("weaponSocket"));
		Info.WeaponSocketName = WeaponSocket.IsEmpty() ? FName("WeaponHandSocket") : FName(*WeaponSocket);
		Info.WeaponTipSocketName = FName(*RoleObj->GetStringField(TEXT("weaponTipSocket")));
		Info.LeftHandSocketName = FName(*RoleObj->GetStringField(TEXT("leftHandSocket")));
		Info.RightHandSocketName = FName(*RoleObj->GetStringField(TEXT("rightHandSocket")));
		Info.TailSocketName = FName(*RoleObj->GetStringField(TEXT("tailSocket")));

		// Attributes (numeric, applied via PrimaryAttributes_SetByCaller at init).
		const TSharedPtr<FJsonObject> Attrs = RoleObj->GetObjectField(TEXT("attributes"));
		if (Attrs.IsValid())
		{
			Info.Strength = static_cast<float>(Attrs->GetNumberField(TEXT("strength")));
			Info.Intelligence = static_cast<float>(Attrs->GetNumberField(TEXT("intelligence")));
			Info.Resilience = static_cast<float>(Attrs->GetNumberField(TEXT("resilience")));
			Info.Vigor = static_cast<float>(Attrs->GetNumberField(TEXT("vigor")));
		}

		// Abilities (class paths; Blueprint classes need the _C suffix).
		const TArray<TSharedPtr<FJsonValue>>& StartupArr = RoleObj->GetArrayField(TEXT("startupAbilities"));
		RoleConfigPrivate::LoadAbilityClasses(StartupArr, Info.StartupAbilities, RoleNameStr, TEXT("startup ability"));

		const TArray<TSharedPtr<FJsonValue>>& PassiveArr = RoleObj->GetArrayField(TEXT("startupPassiveAbilities"));
		RoleConfigPrivate::LoadAbilityClasses(PassiveArr, Info.StartupPassiveAbilities, RoleNameStr, TEXT("startup passive ability"));

		// Data-driven ability definitions (UAuraAbilityDefinition asset paths or .xml file paths).
		const TArray<TSharedPtr<FJsonValue>>& StartupDefArr = RoleObj->GetArrayField(TEXT("startupAbilityDefinitions"));
		for (const TSharedPtr<FJsonValue>& DefValue : StartupDefArr)
		{
			if (!DefValue.IsValid()) continue;
			const FString DefPath = DefValue->AsString();
			if (DefPath.IsEmpty()) continue;
			if (UAuraAbilityDefinition* Def = RoleConfigPrivate::LoadAbilityDefinition(DefPath, RoleNameStr))
			{
				Info.StartupAbilityDefinitionPaths.Add(DefPath);
				Info.StartupAbilityDefinitions.Add(Def);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>& PassiveDefArr = RoleObj->GetArrayField(TEXT("startupPassiveAbilityDefinitions"));
		for (const TSharedPtr<FJsonValue>& DefValue : PassiveDefArr)
		{
			if (!DefValue.IsValid()) continue;
			const FString DefPath = DefValue->AsString();
			if (DefPath.IsEmpty()) continue;
			if (UAuraAbilityDefinition* Def = RoleConfigPrivate::LoadAbilityDefinition(DefPath, RoleNameStr))
			{
				Info.StartupPassiveAbilityDefinitionPaths.Add(DefPath);
				Info.StartupPassiveAbilityDefinitions.Add(Def);
			}
		}

		// LMB default skill: try data-driven definition first (asset or .xml), fall back to class path.
		const FString LMBDefPath = RoleObj->GetStringField(TEXT("lmbAbilityDefinition"));
		if (!LMBDefPath.IsEmpty())
		{
			Info.DefaultLMBAbilityDefinitionPath = LMBDefPath;
			Info.DefaultLMBAbilityDefinition = RoleConfigPrivate::LoadAbilityDefinition(LMBDefPath, RoleNameStr);
		}

		// Legacy LMB class path (backward compatibility).
		const FString LMBAbilityPath = RoleObj->GetStringField(TEXT("lmbAbility"));
		if (!LMBAbilityPath.IsEmpty())
		{
			Info.DefaultLMBAbility = LoadClass<UGameplayAbility>(nullptr, *LMBAbilityPath);
			if (Info.DefaultLMBAbility == nullptr)
			{
				UE_LOG(LogAura, Warning, TEXT("[RoleConfig] Role '%s': failed to load LMB ability '%s'."), *RoleNameStr, *LMBAbilityPath);
			}
		}

		RoleInfo->RoleInformation.Add(RoleName, Info);
		UE_LOG(LogAura, Log, TEXT("[RoleConfig] Loaded role '%s' (mesh=%s, anim=%s, abilities=%d)."),
			*RoleNameStr,
			*GetNameSafe(Info.SkeletalMesh.Get()),
			*GetNameSafe(Info.AnimBlueprintClass.Get()),
			Info.StartupAbilities.Num());
	}

	// Validate the default role references a role that was actually loaded.
	if (RoleInfo->DefaultRole.IsNone())
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] No 'defaultRole' set in %s; login will require an explicit role."), *ConfigPath);
	}
	else if (!RoleInfo->RoleInformation.Contains(RoleInfo->DefaultRole))
	{
		UE_LOG(LogAura, Warning, TEXT("[RoleConfig] defaultRole '%s' does not match any role entry in %s; login will be refused until it is configured."),
			*RoleInfo->DefaultRole.ToString(), *ConfigPath);
	}
	else
	{
		UE_LOG(LogAura, Log, TEXT("[RoleConfig] Default role = '%s'."), *RoleInfo->DefaultRole.ToString());
	}

	UE_LOG(LogAura, Log, TEXT("[RoleConfig] Loaded %d role(s) from %s."), RoleInfo->RoleInformation.Num(), *ConfigPath);
	return RoleInfo;
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
			if (Overlap.GetActor()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
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
	const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	const bool bBothAreEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));
	const bool bFriends = bBothArePlayers || bBothAreEnemies;
	return !bFriends;
}

FGameplayEffectContextHandle UAuraAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
	
	FGameplayEffectContextHandle EffectContexthandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);

	SetIsRadialDamage(EffectContexthandle, DamageEffectParams.bIsRadialDamage);
	SetRadialDamageInnerRadius(EffectContexthandle, DamageEffectParams.RadialDamageInnerRadius);
	SetRadialDamageOuterRadius(EffectContexthandle, DamageEffectParams.RadialDamageOuterRadius);
	SetRadialDamageOrigin(EffectContexthandle, DamageEffectParams.RadialDamageOrigin);
	
	const TSubclassOf<UGameplayEffect> EffectiveGEClass = DamageEffectParams.DamageGameplayEffectClass
		? TSubclassOf<UGameplayEffect>(DamageEffectParams.DamageGameplayEffectClass)
		: TSubclassOf<UGameplayEffect>(UAuraDamageGameplayEffect::StaticClass());

	const FGameplayEffectSpecHandle SpecHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeOutgoingSpec(EffectiveGEClass, DamageEffectParams.AbilityLevel, EffectContexthandle);

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Frequency, DamageEffectParams.DebuffFrequency);
	
	DamageEffectParams.TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
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
