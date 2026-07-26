// Copyright Druid Mechanics


#include "AbilitySystem/Data/AbilityInfo.h"
#include "Aura/AuraLogChannels.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

bool URuntimeAbilityInfo::LoadFromJSON(const FString& JSONContent)
{
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONContent);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogAura, Error, TEXT("[RuntimeAbilityInfo] Failed to deserialize JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* AbilitiesArray;
	if (!RootObject->TryGetArrayField(TEXT("abilities"), AbilitiesArray))
	{
		UE_LOG(LogAura, Error, TEXT("[RuntimeAbilityInfo] JSON missing 'abilities' array"));
		return false;
	}

	AbilityInfoMap.Empty();

	for (const TSharedPtr<FJsonValue>& AbilityValue : *AbilitiesArray)
	{
		const TSharedPtr<FJsonObject> AbilityObj = AbilityValue->AsObject();
		if (!AbilityObj.IsValid())
		{
			UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] Skipping invalid ability entry"));
			continue;
		}

		FAuraAbilityInfo Info;

		// Parse abilityTag (required)
		FString AbilityTagStr;
		if (AbilityObj->TryGetStringField(TEXT("abilityTag"), AbilityTagStr))
		{
			Info.AbilityTag = FGameplayTag::RequestGameplayTag(FName(*AbilityTagStr), false);
			if (!Info.AbilityTag.IsValid())
			{
				UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] Invalid gameplay tag: %s"), *AbilityTagStr);
				continue;
			}
		}
		else
		{
			UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] Ability entry missing 'abilityTag'"));
			continue;
		}

		// Parse icon (optional)
		FString IconPath;
		if (AbilityObj->TryGetStringField(TEXT("icon"), IconPath) && !IconPath.IsEmpty())
		{
			Info.Icon = LoadObject<UTexture2D>(nullptr, *IconPath);
			if (!Info.Icon)
			{
				UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] Failed to load icon: %s"), *IconPath);
			}
		}

		// Parse backgroundMaterial (optional)
		FString MaterialPath;
		if (AbilityObj->TryGetStringField(TEXT("backgroundMaterial"), MaterialPath) && !MaterialPath.IsEmpty())
		{
			Info.BackgroundMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
			if (!Info.BackgroundMaterial)
			{
				UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] Failed to load material: %s"), *MaterialPath);
			}
		}

		// Parse levelRequirement (optional, defaults to 1)
		int32 LevelReq = 1;
		if (AbilityObj->TryGetNumberField(TEXT("levelRequirement"), LevelReq))
		{
			Info.LevelRequirement = LevelReq;
		}

		// Add to map
		AbilityInfoMap.Add(Info.AbilityTag, Info);
		UE_LOG(LogAura, Verbose, TEXT("[RuntimeAbilityInfo] Loaded ability: %s (Icon=%s, Material=%s, LevelReq=%d)"),
			*Info.AbilityTag.ToString(),
			Info.Icon ? *Info.Icon->GetName() : TEXT("None"),
			Info.BackgroundMaterial ? *Info.BackgroundMaterial->GetName() : TEXT("None"),
			Info.LevelRequirement);
	}

	UE_LOG(LogAura, Log, TEXT("[RuntimeAbilityInfo] Loaded %d abilities from JSON"), AbilityInfoMap.Num());
	return AbilityInfoMap.Num() > 0;
}

FAuraAbilityInfo URuntimeAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
	if (const FAuraAbilityInfo* Found = AbilityInfoMap.Find(AbilityTag))
	{
		return *Found;
	}

	if (bLogNotFound)
	{
		UE_LOG(LogAura, Warning, TEXT("[RuntimeAbilityInfo] No info found for tag: %s"), *AbilityTag.ToString());
	}

	return FAuraAbilityInfo();
}

// ============================================================================
// Legacy UAbilityInfo (UAsset-based) - kept for backward compatibility
// ============================================================================

FAuraAbilityInfo UAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
	for (const FAuraAbilityInfo& Info : AbilityInformation)
	{
		if (Info.AbilityTag == AbilityTag)
		{
			return Info;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogAura, Error, TEXT("Can't find info for AbilityTag [%s] on AbilityInfo [%s]"), *AbilityTag.ToString(), *GetNameSafe(this));
	}

	return FAuraAbilityInfo();
}
