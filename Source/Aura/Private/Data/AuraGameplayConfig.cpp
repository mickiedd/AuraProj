// Copyright Druid Mechanics

#include "Data/AuraGameplayConfig.h"

#include "Actor/AuraProjectile.h"
#include "Actor/AuraEffectActor.h"
#include "AuraGameplayTags.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AuraGameplayConfigPrivate
{
	struct FCache
	{
		bool bLoaded = false;
		bool bValid = false;
		int32 LoadCount = 0;
		FString Error;
		TMap<FName, FAuraProjectileDefinition> Projectiles;
		TMap<FName, FAuraPickupDefinition> Pickups;
		TMap<FName, FAuraPickupEffectDefinition> Effects;
		TArray<FAuraLootDefinition> Loot;
	};

	FCache Cache;

	bool ReadJson(const FString& Filename, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutError = FString::Printf(TEXT("Unable to read %s"), *Filename);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			OutError = FString::Printf(TEXT("Malformed JSON in %s"), *Filename);
			return false;
		}
		return true;
	}

	bool ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& OutValue)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 3) return false;
		double X = 0., Y = 0., Z = 0.;
		if (!(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y) || !(*Values)[2]->TryGetNumber(Z)) return false;
		OutValue = FVector(X, Y, Z);
		return true;
	}

	bool ReadRotator(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FRotator& OutValue)
	{
		FVector Value;
		if (!ReadVector(Object, Field, Value)) return false;
		OutValue = FRotator(Value.X, Value.Y, Value.Z);
		return true;
	}

	bool ValidateAssetPath(const FSoftObjectPath& Path, const FString& Context, FString& OutError)
	{
		if (Path.IsNull()) return true;
		if (!Path.IsValid() || !Path.TryLoad())
		{
			OutError = FString::Printf(TEXT("%s references missing asset '%s'"), *Context, *Path.ToString());
			return false;
		}
		return true;
	}

	bool ParseEffects(FCache& Target, FString& OutError)
	{
		const FString Filename = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/GameplayEffects.json"));
		TSharedPtr<FJsonObject> Root;
		if (!ReadJson(Filename, Root, OutError)) return false;
		const TSharedPtr<FJsonObject>* EffectsObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("pickupEffects"), EffectsObject) || !EffectsObject || !EffectsObject->IsValid())
		{
			OutError = TEXT("GameplayEffects.json is missing pickupEffects");
			return false;
		}

		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*EffectsObject)->Values)
		{
			const TSharedPtr<FJsonObject> Object = Pair.Value->AsObject();
			FAuraPickupEffectDefinition Definition;
			Definition.Name = FName(*Pair.Key);
			if (!Object.IsValid() || !Object->TryGetStringField(TEXT("duration"), Definition.DurationType))
			{
				OutError = FString::Printf(TEXT("Pickup effect '%s' is missing duration"), *Pair.Key);
				return false;
			}
			Definition.DurationType.ToLowerInline();
			if (Definition.DurationType != TEXT("instant") && Definition.DurationType != TEXT("duration") && Definition.DurationType != TEXT("infinite"))
			{
				OutError = FString::Printf(TEXT("Pickup effect '%s' has unsupported duration '%s'"), *Pair.Key, *Definition.DurationType);
				return false;
			}
			double Number = 0.;
			if (Object->TryGetNumberField(TEXT("durationValue"), Number)) Definition.Duration = Number;
			if (Definition.DurationType == TEXT("duration") && Definition.Duration <= 0.f)
			{
				OutError = FString::Printf(TEXT("Pickup effect '%s' requires durationValue > 0"), *Pair.Key);
				return false;
			}
			if (Object->TryGetNumberField(TEXT("period"), Number)) Definition.Period = Number;
			if (Definition.Period < 0.f || (Definition.Period > 0.f && Definition.DurationType == TEXT("instant")))
			{
				OutError = FString::Printf(TEXT("Pickup effect '%s' has invalid period"), *Pair.Key);
				return false;
			}
			Object->TryGetBoolField(TEXT("executeOnApplication"), Definition.bExecuteOnApplication);
			if (Object->TryGetNumberField(TEXT("health"), Number)) Definition.Magnitudes.Add(Tags.Attributes_Vital_Health, Number);
			if (Object->TryGetNumberField(TEXT("mana"), Number)) Definition.Magnitudes.Add(Tags.Attributes_Vital_Mana, Number);
			const TSharedPtr<FJsonObject>* Attributes = nullptr;
			if (Object->TryGetObjectField(TEXT("attributes"), Attributes) && Attributes && Attributes->IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Attribute : (*Attributes)->Values)
				{
					double Magnitude = 0.;
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Attribute.Key), false);
					if (!Tag.IsValid() || !Attribute.Value->TryGetNumber(Magnitude))
					{
						OutError = FString::Printf(TEXT("Pickup effect '%s' has invalid attribute '%s'"), *Pair.Key, *Attribute.Key);
						return false;
					}
					Definition.Magnitudes.Add(Tag, Magnitude);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* AssetTags = nullptr;
			if (Object->TryGetArrayField(TEXT("assetTags"), AssetTags) && AssetTags)
			{
				for (const TSharedPtr<FJsonValue>& Value : *AssetTags)
				{
					FString TagString;
					const FGameplayTag Tag = Value->TryGetString(TagString) ? FGameplayTag::RequestGameplayTag(FName(*TagString), false) : FGameplayTag();
					if (!Tag.IsValid())
					{
						OutError = FString::Printf(TEXT("Pickup effect '%s' has unknown asset tag '%s'"), *Pair.Key, *TagString);
						return false;
					}
					Definition.AssetTags.AddTag(Tag);
				}
			}
			Target.Effects.Add(Definition.Name, MoveTemp(Definition));
		}
		return true;
	}

	bool ParseProjectiles(FCache& Target, const TSharedPtr<FJsonObject>& Root, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Definitions = nullptr;
		if (!Root->TryGetObjectField(TEXT("projectiles"), Definitions) || !Definitions || !Definitions->IsValid())
		{
			OutError = TEXT("ProjectileDefinitions.json is missing projectiles");
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Definitions)->Values)
		{
			const TSharedPtr<FJsonObject> Object = Pair.Value->AsObject();
			FAuraProjectileDefinition Definition;
			Definition.Name = FName(*Pair.Key);
			FString ClassPath;
			if (!Object.IsValid() || !Object->TryGetStringField(TEXT("nativeClass"), ClassPath) || !ClassPath.StartsWith(TEXT("/Script/")))
			{
				OutError = FString::Printf(TEXT("Projectile '%s' requires a native /Script class"), *Pair.Key);
				return false;
			}
			Definition.NativeClass = LoadClass<AAuraProjectile>(nullptr, *ClassPath);
			if (!Definition.NativeClass || Definition.NativeClass->ClassGeneratedBy != nullptr)
			{
				OutError = FString::Printf(TEXT("Projectile '%s' nativeClass '%s' is invalid or Blueprint-generated"), *Pair.Key, *ClassPath);
				return false;
			}
			double Number = 0.;
			if (Object->TryGetNumberField(TEXT("collisionRadius"), Number)) Definition.CollisionRadius = Number;
			if (Object->TryGetNumberField(TEXT("initialSpeed"), Number)) Definition.InitialSpeed = Number;
			if (Object->TryGetNumberField(TEXT("maxSpeed"), Number)) Definition.MaxSpeed = Number;
			if (Object->TryGetNumberField(TEXT("gravityScale"), Number)) Definition.GravityScale = Number;
			if (Object->TryGetNumberField(TEXT("lifeSpan"), Number)) Definition.LifeSpan = Number;
			if (Object->TryGetNumberField(TEXT("outboundDistance"), Number)) Definition.OutboundDistance = Number;
			if (Object->TryGetNumberField(TEXT("outboundDuration"), Number)) Definition.OutboundDuration = Number;
			if (Object->TryGetNumberField(TEXT("returnSpeed"), Number)) Definition.ReturnSpeed = Number;
			if (Object->TryGetNumberField(TEXT("returnDistance"), Number)) Definition.ReturnDistance = Number;
			if (Definition.CollisionRadius <= 0.f || Definition.MaxSpeed < 0.f || Definition.InitialSpeed < 0.f || Definition.LifeSpan <= 0.f)
			{
				OutError = FString::Printf(TEXT("Projectile '%s' has an invalid numeric range"), *Pair.Key);
				return false;
			}
			FString Collision = TEXT("Block");
			Object->TryGetStringField(TEXT("worldStaticResponse"), Collision);
			if (Collision.Equals(TEXT("Block"), ESearchCase::IgnoreCase)) Definition.WorldStaticResponse = ECR_Block;
			else if (Collision.Equals(TEXT("Ignore"), ESearchCase::IgnoreCase)) Definition.WorldStaticResponse = ECR_Ignore;
			else if (Collision.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) Definition.WorldStaticResponse = ECR_Overlap;
			else { OutError = FString::Printf(TEXT("Projectile '%s' has unsupported worldStaticResponse '%s'"), *Pair.Key, *Collision); return false; }
			FString Path;
			if (Object->TryGetStringField(TEXT("mesh"), Path)) Definition.Mesh = FSoftObjectPath(Path);
			ReadVector(Object, TEXT("meshScale"), Definition.MeshScale);
			if (Object->TryGetStringField(TEXT("flightTrail"), Path)) Definition.FlightTrail = FSoftObjectPath(Path);
			if (Object->TryGetStringField(TEXT("impactEffect"), Path)) Definition.ImpactEffect = FSoftObjectPath(Path);
			if (Object->TryGetStringField(TEXT("impactSound"), Path)) Definition.ImpactSound = FSoftObjectPath(Path);
			if (Object->TryGetStringField(TEXT("loopingSound"), Path)) Definition.LoopingSound = FSoftObjectPath(Path);
			const FString Context = FString::Printf(TEXT("Projectile '%s'"), *Pair.Key);
			if (!ValidateAssetPath(Definition.Mesh, Context, OutError) || !ValidateAssetPath(Definition.FlightTrail, Context, OutError) ||
				!ValidateAssetPath(Definition.ImpactEffect, Context, OutError) || !ValidateAssetPath(Definition.ImpactSound, Context, OutError) ||
				!ValidateAssetPath(Definition.LoopingSound, Context, OutError)) return false;
			Target.Projectiles.Add(Definition.Name, MoveTemp(Definition));
		}
		return true;
	}

	bool ParsePickups(FCache& Target, const TSharedPtr<FJsonObject>& Root, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Definitions = nullptr;
		if (!Root->TryGetObjectField(TEXT("pickups"), Definitions) || !Definitions || !Definitions->IsValid())
		{
			OutError = TEXT("PickupDefinitions.json is missing pickups");
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Definitions)->Values)
		{
			const TSharedPtr<FJsonObject> Object = Pair.Value->AsObject();
			FAuraPickupDefinition Definition;
			Definition.Name = FName(*Pair.Key);
			FString ClassPath;
			FString Effect;
			if (!Object.IsValid() || !Object->TryGetStringField(TEXT("nativeClass"), ClassPath) || !ClassPath.StartsWith(TEXT("/Script/")) ||
				!Object->TryGetStringField(TEXT("effect"), Effect))
			{
				OutError = FString::Printf(TEXT("Pickup '%s' requires nativeClass and effect"), *Pair.Key);
				return false;
			}
			Definition.NativeClass = LoadClass<AAuraEffectActor>(nullptr, *ClassPath);
			Definition.EffectName = FName(*Effect);
			if (!Definition.NativeClass || Definition.NativeClass->ClassGeneratedBy || !Target.Effects.Contains(Definition.EffectName))
			{
				OutError = FString::Printf(TEXT("Pickup '%s' has invalid nativeClass or unknown effect '%s'"), *Pair.Key, *Effect);
				return false;
			}
			Object->TryGetBoolField(TEXT("applyOnEndOverlap"), Definition.bApplyOnEndOverlap);
			Object->TryGetBoolField(TEXT("removeOnEndOverlap"), Definition.bRemoveOnEndOverlap);
			Object->TryGetBoolField(TEXT("destroyOnApplication"), Definition.bDestroyOnApplication);
			Object->TryGetBoolField(TEXT("applyToEnemies"), Definition.bApplyToEnemies);
			Object->TryGetBoolField(TEXT("rotates"), Definition.bRotates);
			Object->TryGetBoolField(TEXT("sinusoidalMovement"), Definition.bSinusoidalMovement);
			double Number = 0.;
			if (Object->TryGetNumberField(TEXT("actorLevel"), Number)) Definition.ActorLevel = Number;
			if (Object->TryGetNumberField(TEXT("rotationRate"), Number)) Definition.RotationRate = Number;
			if (Object->TryGetNumberField(TEXT("sineAmplitude"), Number)) Definition.SineAmplitude = Number;
			if (Object->TryGetNumberField(TEXT("sinePeriodConstant"), Number)) Definition.SinePeriodConstant = Number;
			FString Shape = TEXT("sphere"); Object->TryGetStringField(TEXT("collisionShape"), Shape);
			if (Shape.Equals(TEXT("sphere"), ESearchCase::IgnoreCase)) Definition.CollisionShape = EAuraConfiguredCollisionShape::Sphere;
			else if (Shape.Equals(TEXT("box"), ESearchCase::IgnoreCase)) Definition.CollisionShape = EAuraConfiguredCollisionShape::Box;
			else if (Shape.Equals(TEXT("capsule"), ESearchCase::IgnoreCase)) Definition.CollisionShape = EAuraConfiguredCollisionShape::Capsule;
			else { OutError = FString::Printf(TEXT("Pickup '%s' has unsupported collisionShape '%s'"), *Pair.Key, *Shape); return false; }
			ReadVector(Object, TEXT("collisionSize"), Definition.CollisionSize);
			if (Definition.CollisionSize.GetMin() <= 0.f) { OutError = FString::Printf(TEXT("Pickup '%s' has invalid collisionSize"), *Pair.Key); return false; }
			FString Path;
			if (Object->TryGetStringField(TEXT("mesh"), Path)) Definition.Mesh = FSoftObjectPath(Path);
			ReadVector(Object, TEXT("meshOffset"), Definition.MeshOffset); ReadRotator(Object, TEXT("meshRotation"), Definition.MeshRotation); ReadVector(Object, TEXT("meshScale"), Definition.MeshScale);
			const TArray<TSharedPtr<FJsonValue>>* Materials = nullptr;
			if (Object->TryGetArrayField(TEXT("materials"), Materials) && Materials) for (const TSharedPtr<FJsonValue>& Value : *Materials) { FString ValuePath; if (Value->TryGetString(ValuePath)) Definition.Materials.Add(FSoftObjectPath(ValuePath)); }
			if (Object->TryGetStringField(TEXT("vfx"), Path)) Definition.Vfx = FSoftObjectPath(Path);
			ReadVector(Object, TEXT("vfxOffset"), Definition.VfxOffset);
			if (Object->TryGetStringField(TEXT("secondaryVfx"), Path)) Definition.SecondaryVfx = FSoftObjectPath(Path);
			ReadVector(Object, TEXT("secondaryVfxOffset"), Definition.SecondaryVfxOffset);
			const FString Context = FString::Printf(TEXT("Pickup '%s'"), *Pair.Key);
			if (!ValidateAssetPath(Definition.Mesh, Context, OutError) || !ValidateAssetPath(Definition.Vfx, Context, OutError) || !ValidateAssetPath(Definition.SecondaryVfx, Context, OutError)) return false;
			for (const FSoftObjectPath& Material : Definition.Materials) if (!ValidateAssetPath(Material, Context, OutError)) return false;
			Target.Pickups.Add(Definition.Name, MoveTemp(Definition));
		}

		const TArray<TSharedPtr<FJsonValue>>* LootValues = nullptr;
		if (Root->TryGetArrayField(TEXT("loot"), LootValues) && LootValues)
		{
			for (const TSharedPtr<FJsonValue>& Value : *LootValues)
			{
				const TSharedPtr<FJsonObject> Object = Value->AsObject();
				FAuraLootDefinition Loot;
				FString Name;
				double Number = 0.;
				if (!Object.IsValid() || !Object->TryGetStringField(TEXT("pickup"), Name) || !Target.Pickups.Contains(FName(*Name))) { OutError = TEXT("Loot entry references an unknown pickup"); return false; }
				Loot.PickupDefinition = FName(*Name);
				if (Object->TryGetNumberField(TEXT("chanceToSpawn"), Number)) Loot.ChanceToSpawn = Number;
				if (Object->TryGetNumberField(TEXT("maxNumberToSpawn"), Number)) Loot.MaxNumberToSpawn = Number;
				Object->TryGetBoolField(TEXT("lootLevelOverride"), Loot.bLootLevelOverride);
				if (Loot.ChanceToSpawn < 0.f || Loot.ChanceToSpawn > 100.f || Loot.MaxNumberToSpawn < 0) { OutError = TEXT("Loot entry has an invalid numeric range"); return false; }
				Target.Loot.Add(Loot);
			}
		}
		return true;
	}

	void EnsureLoaded()
	{
		if (Cache.bLoaded) return;
		Cache.bLoaded = true;
		++Cache.LoadCount;
		TSharedPtr<FJsonObject> ProjectileRoot;
		TSharedPtr<FJsonObject> PickupRoot;
		FString Error;
		const bool bSuccess = ParseEffects(Cache, Error) &&
			ReadJson(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/ProjectileDefinitions.json")), ProjectileRoot, Error) && ParseProjectiles(Cache, ProjectileRoot, Error) &&
			ReadJson(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config/PickupDefinitions.json")), PickupRoot, Error) && ParsePickups(Cache, PickupRoot, Error);
		Cache.bValid = bSuccess;
		Cache.Error = Error;
		if (!bSuccess) UE_LOG(LogTemp, Error, TEXT("[AuraGameplayConfig] %s"), *Error);
	}
}

const FAuraProjectileDefinition* FAuraGameplayConfig::FindProjectile(FName Name) { AuraGameplayConfigPrivate::EnsureLoaded(); return AuraGameplayConfigPrivate::Cache.bValid ? AuraGameplayConfigPrivate::Cache.Projectiles.Find(Name) : nullptr; }
const FAuraPickupDefinition* FAuraGameplayConfig::FindPickup(FName Name) { AuraGameplayConfigPrivate::EnsureLoaded(); return AuraGameplayConfigPrivate::Cache.bValid ? AuraGameplayConfigPrivate::Cache.Pickups.Find(Name) : nullptr; }
const FAuraPickupEffectDefinition* FAuraGameplayConfig::FindPickupEffect(FName Name) { AuraGameplayConfigPrivate::EnsureLoaded(); return AuraGameplayConfigPrivate::Cache.bValid ? AuraGameplayConfigPrivate::Cache.Effects.Find(Name) : nullptr; }
const TArray<FAuraLootDefinition>& FAuraGameplayConfig::GetLootDefinitions() { AuraGameplayConfigPrivate::EnsureLoaded(); return AuraGameplayConfigPrivate::Cache.Loot; }
bool FAuraGameplayConfig::ValidateAll(FString& OutError) { AuraGameplayConfigPrivate::EnsureLoaded(); OutError = AuraGameplayConfigPrivate::Cache.Error; return AuraGameplayConfigPrivate::Cache.bValid; }
int32 FAuraGameplayConfig::GetLoadCount() { return AuraGameplayConfigPrivate::Cache.LoadCount; }
void FAuraGameplayConfig::ResetForTests() { AuraGameplayConfigPrivate::Cache = AuraGameplayConfigPrivate::FCache(); }
