// Copyright Druid Mechanics

#include "Battle/AuraBattleZoneConfig.h"

#include "Combat/AuraCombatRules.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Game/AuraGameModeBase.h"
#include "World/AuraPopulationManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AuraBattleZoneConfigPrivate
{
	bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& Out)
	{
		return Object.IsValid() && Object->TryGetNumberField(Field, Out) && FMath::IsFinite(Out);
	}

	bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Out, bool DefaultValue)
	{
		Out = DefaultValue;
		return !Object.IsValid() || !Object->HasField(Field) || Object->TryGetBoolField(Field, Out);
	}

	bool ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& Out, const bool bRequirePositive)
	{
		const TSharedPtr<FJsonObject>* VectorObject = nullptr;
		if (!Object.IsValid() || !Object->TryGetObjectField(Field, VectorObject) || !VectorObject || !VectorObject->IsValid()) return false;
		double X = 0.0, Y = 0.0, Z = 0.0;
		if (!ReadNumber(*VectorObject, TEXT("x"), X) || !ReadNumber(*VectorObject, TEXT("y"), Y) || !ReadNumber(*VectorObject, TEXT("z"), Z)) return false;
		Out = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
		return FMath::IsFinite(Out.X) && FMath::IsFinite(Out.Y) && FMath::IsFinite(Out.Z)
			&& (!bRequirePositive || (Out.X > 0.f && Out.Y > 0.f && Out.Z > 0.f));
	}
}

FString UAuraBattleZoneConfig::GetConfigPath()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Config"), TEXT("BattleZones.json"));
}

bool UAuraBattleZoneConfig::Load(FString& OutError)
{
	OutError.Empty();
	Zones.Reset();
	SchemaVersion = 0;
	ConfigHash.Empty();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetConfigPath()))
	{
		OutError = FString::Printf(TEXT("Unable to read '%s'."), *GetConfigPath());
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("BattleZones.json is not valid JSON.");
		return false;
	}
	double Version = 0.0;
	if (!AuraBattleZoneConfigPrivate::ReadNumber(Root, TEXT("schemaVersion"), Version) || !FMath::IsNearlyEqual(Version, 1.0))
	{
		OutError = TEXT("BattleZones.json.schemaVersion must be 1.");
		return false;
	}
	SchemaVersion = 1;
	ConfigHash = FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Json));
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("zones"), Values) || !Values || Values->Num() == 0)
	{
		OutError = TEXT("BattleZones.json.zones must be a non-empty array.");
		return false;
	}
	TSet<FName> SeenIds;
	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = (*Values)[Index].IsValid() ? (*Values)[Index]->AsObject() : nullptr;
		if (!Object.IsValid()) { OutError += TEXT("zone entry must be an object.\n"); continue; }
		FString Id;
		if (!Object->TryGetStringField(TEXT("id"), Id) || Id.TrimStartAndEnd().IsEmpty()) { OutError += TEXT("zone.id must be a non-empty string.\n"); continue; }
		const FName ZoneId(*Id.TrimStartAndEnd());
		if (SeenIds.Contains(ZoneId)) { OutError += FString::Printf(TEXT("duplicate zone id '%s'.\n"), *Id); continue; }
		FAuraBattleZoneDefinition Zone;
		Zone.ZoneId = ZoneId;
		FString MapId;
		if (!Object->TryGetStringField(TEXT("mapId"), MapId) || MapId.TrimStartAndEnd().IsEmpty())
		{
			OutError += FString::Printf(TEXT("zone '%s' mapId must be a non-empty string.\n"), *Id); continue;
		}
		Zone.MapId = FName(*MapId.TrimStartAndEnd());
		double Priority = 0.0;
		if (!AuraBattleZoneConfigPrivate::ReadNumber(Object, TEXT("priority"), Priority) || !FMath::IsNearlyEqual(Priority, FMath::RoundToDouble(Priority)))
		{
			OutError += FString::Printf(TEXT("zone '%s' priority must be an integer.\n"), *Id); continue;
		}
		Zone.Priority = FMath::RoundToInt(static_cast<float>(Priority));
		if (!AuraBattleZoneConfigPrivate::ReadVector(Object, TEXT("center"), Zone.Center, false)
			|| !AuraBattleZoneConfigPrivate::ReadVector(Object, TEXT("extent"), Zone.Extent, true))
		{
			OutError += FString::Printf(TEXT("zone '%s' center/extent is invalid.\n"), *Id); continue;
		}
		if (!AuraBattleZoneConfigPrivate::ReadBool(Object, TEXT("safeZone"), Zone.bSafeZone, false))
		{
			OutError += FString::Printf(TEXT("zone '%s' safeZone must be a boolean.\n"), *Id); continue;
		}
		const TSharedPtr<FJsonObject>* Policy = nullptr;
		if (!Object->TryGetObjectField(TEXT("policy"), Policy) || !Policy || !Policy->IsValid()) { OutError += FString::Printf(TEXT("zone '%s' policy is missing.\n"), *Id); continue; }
		if (!AuraBattleZoneConfigPrivate::ReadBool(*Policy, TEXT("allowPvP"), Zone.bAllowPvP, false)
			|| !AuraBattleZoneConfigPrivate::ReadBool(*Policy, TEXT("allowPlayerToCivilian"), Zone.bAllowPlayerToCivilian, false)
			|| !AuraBattleZoneConfigPrivate::ReadBool(*Policy, TEXT("allowEnemyToCivilian"), Zone.bAllowEnemyToCivilian, false)
			|| !AuraBattleZoneConfigPrivate::ReadBool(*Policy, TEXT("targetProtected"), Zone.bTargetProtected, true))
		{
			OutError += FString::Printf(TEXT("zone '%s' policy contains an invalid boolean.\n"), *Id); continue;
		}
		SeenIds.Add(ZoneId);
		Zones.Add(Zone);
	}
	if (!OutError.IsEmpty() || Zones.Num() == 0)
	{
		Zones.Reset();
		return false;
	}
	return true;
}

FName UAuraBattleZoneConfig::ResolveCurrentMapId(const UObject* WorldContext)
{
	const UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	if (!World) return NAME_None;
	if (const AAuraGameModeBase* GameMode = World->GetAuthGameMode<AAuraGameModeBase>())
	{
		if (const UAuraPopulationManager* PopulationManager = GameMode->GetPopulationManager())
		{
			const FName ConfiguredMapId = PopulationManager->GetCurrentMapId();
			if (!ConfiguredMapId.IsNone()) return ConfiguredMapId;
		}
	}
	FString MapName = World->GetMapName();
	MapName.RemoveFromStart(World->StreamingLevelsPrefix);
	return FName(*MapName);
}

const FAuraBattleZoneDefinition* UAuraBattleZoneConfig::ResolveZone(const FVector& Location, const FName MapId) const
{
	if (MapId.IsNone()) return nullptr;
	const FAuraBattleZoneDefinition* Best = nullptr;
	for (const FAuraBattleZoneDefinition& Candidate : Zones)
	{
		if (Candidate.MapId != MapId || !Candidate.Contains(Location)) continue;
		if (!Best || (Candidate.bSafeZone && !Best->bSafeZone)
			|| (Candidate.bSafeZone == Best->bSafeZone && (Candidate.Priority > Best->Priority
				|| (Candidate.Priority == Best->Priority && Candidate.ZoneId.LexicalLess(Best->ZoneId)))))
		{
			Best = &Candidate;
		}
	}
	return Best;
}

FAuraCombatPolicySnapshot UAuraBattleZoneConfig::BuildPolicySnapshot(const UObject* WorldContext, const FVector& Location, EAuraBattlePhase Phase, FName EventId) const
{
	return BuildPolicySnapshotForMap(WorldContext, Location, ResolveCurrentMapId(WorldContext), Phase, EventId);
}

FAuraCombatPolicySnapshot UAuraBattleZoneConfig::BuildPolicySnapshotForMap(const UObject* WorldContext, const FVector& Location, const FName MapId, const EAuraBattlePhase Phase, const FName EventId) const
{
	const FAuraBattleZoneDefinition* Zone = ResolveZone(Location, MapId);
	// The v1 schema has one permissive policy per zone. It is authoritative only
	// during Conflict; Peace, Alert, Cleanup, and unknown maps fail closed.
	const bool bConflict = Phase == EAuraBattlePhase::Conflict;
	return FAuraCombatRules::MakeAuthoritativePolicySnapshot(
		WorldContext,
		Zone && bConflict ? Zone->bAllowPvP : false,
		Zone && bConflict ? Zone->bAllowPlayerToCivilian : false,
		Zone && bConflict ? Zone->bAllowEnemyToCivilian : false,
		!Zone || !bConflict ? true : Zone->bTargetProtected,
		Zone ? Zone->ZoneId : NAME_None,
		EventId);
}
