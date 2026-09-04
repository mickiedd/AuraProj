// Copyright Druid Mechanics

#include "Gameplay/AuraGameplayDefinitionRegistry.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AuraGameplayDefinitionRegistryPrivate
{
	constexpr int32 SchemaVersion = 1;
	constexpr TCHAR Profile[] = TEXT("GameplayExpansionV1");
	constexpr TCHAR DefinitionRevision[] = TEXT("days-42-60-foundation-v1");

	bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
	{
		return Object.IsValid() && Object->TryGetStringField(Field, Out) && !Out.IsEmpty();
	}

	bool ReadPositiveInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
			|| Number < 1.0 || Number > static_cast<double>(MAX_int32) || FMath::FloorToDouble(Number) != Number)
		{
			return false;
		}
		Out = static_cast<int32>(Number);
		return true;
	}

	bool ReadFloatInRange(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float Minimum, float Maximum,
		float& Out)
	{
		double Number = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
			|| Number < static_cast<double>(Minimum) || Number > static_cast<double>(Maximum))
		{
			return false;
		}
		Out = static_cast<float>(Number);
		return FMath::IsFinite(Out);
	}

	bool ReadRequiredBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Out)
	{
		return Object.IsValid() && Object->TryGetBoolField(Field, Out);
	}

	bool ReadUniqueStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FName>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() == 0)
		{
			return false;
		}
		TSet<FName> Seen;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const FString StringValue = Value.IsValid() ? Value->AsString() : FString();
			const FName NameValue(*StringValue);
			if (StringValue.IsEmpty() || NameValue.IsNone() || Seen.Contains(NameValue))
			{
				return false;
			}
			Seen.Add(NameValue);
			Out.Add(NameValue);
		}
		return true;
	}

	bool IsKnownObjective(const FString& Objective)
	{
		return Objective == TEXT("Clear") || Objective == TEXT("Escort") || Objective == TEXT("InteractHold");
	}

	bool HasRequiredArchetypeSet(const TMap<FName, FAuraGameplayEnemyArchetypeDefinition>& Archetypes)
	{
		for (const TCHAR* RequiredId : { TEXT("Raider"), TEXT("Lancer"), TEXT("Bulwark"), TEXT("Disruptor") })
		{
			if (!Archetypes.Contains(FName(RequiredId))) return false;
		}
		return true;
	}
}

void UAuraGameplayDefinitionRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetDefinitions();
}

void UAuraGameplayDefinitionRegistry::Deinitialize()
{
	ResetDefinitions();
	Super::Deinitialize();
}

void UAuraGameplayDefinitionRegistry::ResetDefinitions()
{
	Missions.Reset();
	Encounters.Reset();
	Archetypes.Reset();
	EncounterPolicy = FAuraGameplayEncounterPolicyDefinition();
	RoleLoadouts.Reset();
	Layouts.Reset();
	DefinitionRevision.Reset();
	DefinitionsHash.Reset();
	RuntimeBlocker.Reset();
	bDefinitionsLoaded = false;
	bRuntimeReady = false;
}

bool UAuraGameplayDefinitionRegistry::LoadJsonObject(const FString& RelativePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	FString Text;
	const FString Path = FPaths::ProjectDir() / RelativePath;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("DefinitionMissing:%s"), *RelativePath);
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = FString::Printf(TEXT("DefinitionInvalidJson:%s"), *RelativePath);
		return false;
	}
	return CheckHeader(OutObject, RelativePath, OutError);
}

bool UAuraGameplayDefinitionRegistry::CheckHeader(const TSharedPtr<FJsonObject>& Object, const FString& RelativePath, FString& OutError)
{
	double Schema = 0.0;
	FString Profile;
	FString Revision;
	if (!Object->TryGetNumberField(TEXT("schemaVersion"), Schema) || Schema != AuraGameplayDefinitionRegistryPrivate::SchemaVersion
		|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("gameplayProfile"), Profile)
		|| Profile != AuraGameplayDefinitionRegistryPrivate::Profile
		|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("definitionRevision"), Revision)
		|| Revision != AuraGameplayDefinitionRegistryPrivate::DefinitionRevision
		|| (!DefinitionRevision.IsEmpty() && DefinitionRevision != Revision))
	{
		OutError = FString::Printf(TEXT("DefinitionHeaderRejected:%s"), *RelativePath);
		return false;
	}
	DefinitionRevision = Revision;
	return true;
}

bool UAuraGameplayDefinitionRegistry::LoadDefinitions(FString& OutError)
{
	OutError.Reset();
	ResetDefinitions();

	TSharedPtr<FJsonObject> MissionsRoot;
	TSharedPtr<FJsonObject> RoleLoadoutsRoot;
	TSharedPtr<FJsonObject> EncountersRoot;
	TSharedPtr<FJsonObject> ArchetypesRoot;
	TSharedPtr<FJsonObject> LayoutsRoot;
	const TArray<FString> Paths = {
		TEXT("Content/Config/GameplayMissionDefinitions.json"),
		TEXT("Content/Config/GameplayRoleLoadouts.json"),
		TEXT("Content/Config/GameplayEncounterDefinitions.json"),
		TEXT("Content/Config/GameplayEnemyArchetypes.json"),
		TEXT("Content/Config/GameplayArenaLayouts.json")};
	if (!LoadJsonObject(Paths[0], MissionsRoot, OutError) || !LoadJsonObject(Paths[1], RoleLoadoutsRoot, OutError)
		|| !LoadJsonObject(Paths[2], EncountersRoot, OutError) || !LoadJsonObject(Paths[3], ArchetypesRoot, OutError)
		|| !LoadJsonObject(Paths[4], LayoutsRoot, OutError))
	{
		return false;
	}

	FSHA1 DefinitionsHasher;
	for (const FString& RelativePath : Paths)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / RelativePath)))
		{
			OutError = FString::Printf(TEXT("DefinitionMissing:%s"), *RelativePath);
			ResetDefinitions();
			return false;
		}
		const FString CanonicalInput = RelativePath + TEXT("\n") + Text + TEXT("\n");
		FTCHARToUTF8 Utf8(*CanonicalInput);
		DefinitionsHasher.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}
	DefinitionsHasher.Final();
	uint8 DefinitionsDigest[FSHA1::DigestSize];
	DefinitionsHasher.GetHash(DefinitionsDigest);
	DefinitionsHash = BytesToHex(DefinitionsDigest, FSHA1::DigestSize);

	const TArray<TSharedPtr<FJsonValue>>* MissionValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* RoleLoadoutValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* EncounterValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ArchetypeValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LayoutValues = nullptr;
	if (!MissionsRoot->TryGetArrayField(TEXT("missions"), MissionValues) || !MissionValues || MissionValues->Num() == 0
		|| !RoleLoadoutsRoot->TryGetArrayField(TEXT("roles"), RoleLoadoutValues) || !RoleLoadoutValues || RoleLoadoutValues->Num() == 0
		|| !EncountersRoot->TryGetArrayField(TEXT("encounters"), EncounterValues) || !EncounterValues || EncounterValues->Num() == 0
		|| !ArchetypesRoot->TryGetArrayField(TEXT("archetypes"), ArchetypeValues) || !ArchetypeValues || ArchetypeValues->Num() == 0
		|| !LayoutsRoot->TryGetArrayField(TEXT("layouts"), LayoutValues) || !LayoutValues || LayoutValues->Num() == 0)
	{
		OutError = TEXT("DefinitionArrayMissing");
		ResetDefinitions();
		return false;
	}

	const TSharedPtr<FJsonObject>* PolicyObject = nullptr;
	FString SupportFieldOwnershipPolicy;
	if (!EncountersRoot->TryGetObjectField(TEXT("policy"), PolicyObject) || !PolicyObject || !PolicyObject->IsValid()
		|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(*PolicyObject, TEXT("maxConcurrentHeavyAttacks"),
			EncounterPolicy.MaxConcurrentHeavyAttacks)
		|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(*PolicyObject, TEXT("maxHeavyTokensPerParticipant"),
			EncounterPolicy.MaxHeavyTokensPerParticipant)
		|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(*PolicyObject, TEXT("heavyCoverageRangeUnits"),
			1.0f, 100000.0f, EncounterPolicy.HeavyCoverageRangeUnits)
		|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(*PolicyObject, TEXT("maxDisruptors"),
			EncounterPolicy.MaxDisruptors)
		|| !AuraGameplayDefinitionRegistryPrivate::ReadString(*PolicyObject, TEXT("supportFieldOwnershipPolicy"),
			SupportFieldOwnershipPolicy)
		|| EncounterPolicy.MaxConcurrentHeavyAttacks != 2
		|| EncounterPolicy.MaxHeavyTokensPerParticipant != 2
		|| !FMath::IsNearlyEqual(EncounterPolicy.HeavyCoverageRangeUnits, 600.0f)
		|| EncounterPolicy.MaxDisruptors != 2
		|| SupportFieldOwnershipPolicy != TEXT("LowestLiveSourceLease"))
	{
		OutError = TEXT("EncounterPolicyInvalid");
		ResetDefinitions();
		return false;
	}
	EncounterPolicy.SupportFieldOwnershipPolicy = FName(*SupportFieldOwnershipPolicy);

	for (const TSharedPtr<FJsonValue>& Value : *ArchetypeValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		FString Counter;
		bool bRequiresInterrupt = false;
		bool bRequiresEvade = false;
		bool bHasSupportField = false;
		const TArray<TSharedPtr<FJsonValue>>* AttackValues = nullptr;
		if (!AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("id"), Id)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("counterVerb"), Counter)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(Object, TEXT("requiresInterrupt"), bRequiresInterrupt)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(Object, TEXT("requiresEvade"), bRequiresEvade)
			|| !Object->TryGetArrayField(TEXT("attacks"), AttackValues) || !AttackValues || AttackValues->Num() == 0
			|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(Object, TEXT("hasSupportField"), bHasSupportField))
		{
			OutError = TEXT("ArchetypeInvalid");
			ResetDefinitions();
			return false;
		}
		if (Archetypes.Contains(FName(*Id)))
		{
			OutError = TEXT("ArchetypeDuplicateId");
			ResetDefinitions();
			return false;
		}
		FAuraGameplayEnemyArchetypeDefinition& Definition = Archetypes.Add(FName(*Id));
		Definition.Id = FName(*Id);
		Definition.CounterVerb = FName(*Counter);
		Definition.bRequiresInterrupt = bRequiresInterrupt;
		Definition.bRequiresEvade = bRequiresEvade;
		Definition.bHasSupportField = bHasSupportField;

		TSet<FName> AttackIds;
		for (const TSharedPtr<FJsonValue>& AttackValue : *AttackValues)
		{
			const TSharedPtr<FJsonObject> AttackObject = AttackValue.IsValid() ? AttackValue->AsObject() : nullptr;
			FString AttackId;
			FString ShapeId;
			FString TelegraphId;
			FString AttackCounter;
			float WindupSeconds = 0.0f;
			float RecoverySeconds = 0.0f;
			bool bShapeFrozen = false;
			bool bConsumesHeavyAdmission = false;
			bool bInterruptible = false;
			if (!AuraGameplayDefinitionRegistryPrivate::ReadString(AttackObject, TEXT("id"), AttackId)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadString(AttackObject, TEXT("attackShapeId"), ShapeId)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadString(AttackObject, TEXT("telegraphProfileId"), TelegraphId)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadString(AttackObject, TEXT("counterVerb"), AttackCounter)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(AttackObject, TEXT("windupSeconds"), 0.85f,
					3600.0f, WindupSeconds)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(AttackObject, TEXT("recoverySeconds"), 0.0f,
					3600.0f, RecoverySeconds)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(AttackObject, TEXT("shapeFrozenDuringWindup"), bShapeFrozen)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(AttackObject, TEXT("consumesHeavyAdmission"),
					bConsumesHeavyAdmission)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(AttackObject, TEXT("interruptible"), bInterruptible))
			{
				OutError = TEXT("ArchetypeAttackInvalid");
				ResetDefinitions();
				return false;
			}
			const FName AttackName(*AttackId);
			if (AttackIds.Contains(AttackName))
			{
				OutError = TEXT("ArchetypeAttackDuplicateId");
				ResetDefinitions();
				return false;
			}
			AttackIds.Add(AttackName);
			FAuraEnemyAttackDefinition& Attack = Definition.Attacks.AddDefaulted_GetRef();
			Attack.Id = AttackName;
			Attack.AttackShapeId = FName(*ShapeId);
			Attack.TelegraphProfileId = FName(*TelegraphId);
			Attack.CounterVerb = FName(*AttackCounter);
			Attack.WindupSeconds = WindupSeconds;
			Attack.RecoverySeconds = RecoverySeconds;
			Attack.bShapeFrozenDuringWindup = bShapeFrozen;
			Attack.bConsumesHeavyAdmission = bConsumesHeavyAdmission;
			Attack.bInterruptible = bInterruptible;
		}

		if (Id == TEXT("Bulwark"))
		{
			if (!AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(Object, TEXT("frontArcDegrees"), 0.0f, 360.0f,
				Definition.FrontArcDegrees)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(Object, TEXT("frontDamageReductionFraction"),
					0.0f, 1.0f, Definition.FrontDamageReductionFraction)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(Object, TEXT("slamRecoverySeconds"), 0.0f, 3600.0f,
					Definition.SlamRecoverySeconds)
				|| !FMath::IsNearlyEqual(Definition.FrontArcDegrees, 120.0f)
				|| !FMath::IsNearlyEqual(Definition.FrontDamageReductionFraction, 0.60f)
				|| !FMath::IsNearlyEqual(Definition.SlamRecoverySeconds, 1.20f))
			{
				OutError = TEXT("BulwarkCounterContractInvalid");
				ResetDefinitions();
				return false;
			}
		}

		if (bHasSupportField)
		{
			const TSharedPtr<FJsonObject>* SupportObject = nullptr;
			FString SupportId;
			FString OwnershipPolicy;
			float DurationSeconds = 0.0f;
			float DamageReductionFraction = 0.0f;
			float RangeUnits = 0.0f;
			int32 MaxAllies = 0;
			bool bInterruptible = false;
			bool bRemoveOnRangeDeparture = false;
			bool bRemoveOnOwnerDeath = false;
			if (!Object->TryGetObjectField(TEXT("supportField"), SupportObject) || !SupportObject || !SupportObject->IsValid()
				|| !AuraGameplayDefinitionRegistryPrivate::ReadString(*SupportObject, TEXT("id"), SupportId)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(*SupportObject, TEXT("durationSeconds"), 0.0f,
					3600.0f, DurationSeconds)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(*SupportObject, TEXT("damageReductionFraction"),
					0.0f, 1.0f, DamageReductionFraction)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadFloatInRange(*SupportObject, TEXT("rangeUnits"), 0.0f,
					100000.0f, RangeUnits)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(*SupportObject, TEXT("maxAllies"), MaxAllies)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadString(*SupportObject, TEXT("ownershipPolicy"), OwnershipPolicy)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(*SupportObject, TEXT("interruptible"), bInterruptible)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(*SupportObject, TEXT("removeOnRangeDeparture"),
					bRemoveOnRangeDeparture)
				|| !AuraGameplayDefinitionRegistryPrivate::ReadRequiredBool(*SupportObject, TEXT("removeOnOwnerDeath"),
					bRemoveOnOwnerDeath)
				|| !FMath::IsNearlyEqual(DurationSeconds, 2.0f)
				|| !FMath::IsNearlyEqual(DamageReductionFraction, 0.20f)
				|| !FMath::IsNearlyEqual(RangeUnits, 600.0f) || MaxAllies != 2
				|| OwnershipPolicy != TEXT("LowestLiveSourceLease") || !bInterruptible || !bRemoveOnRangeDeparture
				|| !bRemoveOnOwnerDeath)
			{
				OutError = TEXT("SupportFieldContractInvalid");
				ResetDefinitions();
				return false;
			}
			Definition.SupportField.Id = FName(*SupportId);
			Definition.SupportField.DurationSeconds = DurationSeconds;
			Definition.SupportField.DamageReductionFraction = DamageReductionFraction;
			Definition.SupportField.RangeUnits = RangeUnits;
			Definition.SupportField.MaxAllies = MaxAllies;
			Definition.SupportField.OwnershipPolicy = FName(*OwnershipPolicy);
			Definition.SupportField.bInterruptible = bInterruptible;
			Definition.SupportField.bRemoveOnRangeDeparture = bRemoveOnRangeDeparture;
			Definition.SupportField.bRemoveOnOwnerDeath = bRemoveOnOwnerDeath;
		}
	}

	if (!AuraGameplayDefinitionRegistryPrivate::HasRequiredArchetypeSet(Archetypes))
	{
		OutError = TEXT("RequiredArchetypeMissing");
		ResetDefinitions();
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *RoleLoadoutValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		int32 EmergencyRefillCharges = 0;
		TArray<FName> AbilityIds;
		if (!AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("id"), Id)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadUniqueStringArray(Object, TEXT("abilityIds"), AbilityIds)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(Object, TEXT("emergencyRefillCharges"), EmergencyRefillCharges))
		{
			OutError = TEXT("RoleLoadoutInvalid");
			ResetDefinitions();
			return false;
		}
		if (RoleLoadouts.Contains(FName(*Id)))
		{
			OutError = TEXT("RoleLoadoutDuplicateId");
			ResetDefinitions();
			return false;
		}
		FAuraGameplayRoleLoadoutDefinition& Definition = RoleLoadouts.Add(FName(*Id));
		Definition.RoleId = FName(*Id);
		Definition.AbilityIds = MoveTemp(AbilityIds);
		Definition.EmergencyRefillCharges = EmergencyRefillCharges;
	}

	for (const TSharedPtr<FJsonValue>& Value : *EncounterValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		FString Archetype;
		FString Anchor;
		int32 SpawnCount = 0;
		if (!AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("id"), Id)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("archetypeId"), Archetype)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(Object, TEXT("spawnCount"), SpawnCount)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("layoutAnchorId"), Anchor)
			|| !Archetypes.Contains(FName(*Archetype)))
		{
			OutError = TEXT("EncounterInvalid");
			ResetDefinitions();
			return false;
		}
		if (Encounters.Contains(FName(*Id)))
		{
			OutError = TEXT("EncounterDuplicateId");
			ResetDefinitions();
			return false;
		}
		FAuraGameplayEncounterDefinition& Definition = Encounters.Add(FName(*Id));
		Definition.Id = FName(*Id);
		Definition.ArchetypeId = FName(*Archetype);
		Definition.SpawnCount = SpawnCount;
		Definition.LayoutAnchorId = FName(*Anchor);
	}

	for (const TSharedPtr<FJsonValue>& Value : *MissionValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		FString Objective;
		int32 RunCap = 0;
		int32 Extraction = 0;
		const TArray<TSharedPtr<FJsonValue>>* CellValues = nullptr;
		if (!AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("id"), Id)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("objectiveType"), Objective)
			|| !AuraGameplayDefinitionRegistryPrivate::IsKnownObjective(Objective)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(Object, TEXT("runCapSeconds"), RunCap)
			|| !AuraGameplayDefinitionRegistryPrivate::ReadPositiveInt(Object, TEXT("extractionSeconds"), Extraction)
			|| !Object->TryGetArrayField(TEXT("cellEncounterIds"), CellValues) || !CellValues || CellValues->Num() != 2)
		{
			OutError = TEXT("MissionInvalid");
			ResetDefinitions();
			return false;
		}
		if (Missions.Contains(FName(*Id)))
		{
			OutError = TEXT("MissionDuplicateId");
			ResetDefinitions();
			return false;
		}
		FAuraGameplayMissionDefinition& Definition = Missions.Add(FName(*Id));
		Definition.Id = FName(*Id);
		Definition.ObjectiveType = FName(*Objective);
		Definition.RunCapSeconds = RunCap;
		Definition.ExtractionSeconds = Extraction;
		Object->TryGetBoolField(TEXT("requiresSurveyedAnchors"), Definition.bRequiresSurveyedAnchors);
		TSet<FName> EncounterIds;
		for (const TSharedPtr<FJsonValue>& CellValue : *CellValues)
		{
			const FString EncounterId = CellValue.IsValid() ? CellValue->AsString() : FString();
			const FName EncounterName(*EncounterId);
			if (EncounterId.IsEmpty() || EncounterName.IsNone() || EncounterIds.Contains(EncounterName)
				|| !Encounters.Contains(EncounterName))
			{
				OutError = TEXT("MissionEncounterReferenceInvalid");
				ResetDefinitions();
				return false;
			}
			Definition.CellEncounterIds.Add(EncounterName);
			EncounterIds.Add(EncounterName);
			if (Definition.ObjectiveType == TEXT("Clear") && Encounters[EncounterName].SpawnCount != 3)
			{
				OutError = TEXT("ClearCellSpawnCountInvalid");
				ResetDefinitions();
				return false;
			}
		}
	}

	for (const TSharedPtr<FJsonValue>& Value : *LayoutValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		bool bVerified = false;
		const TArray<TSharedPtr<FJsonValue>>* Anchors = nullptr;
		if (!AuraGameplayDefinitionRegistryPrivate::ReadString(Object, TEXT("id"), Id)
			|| !Object->TryGetBoolField(TEXT("verified"), bVerified)
			|| !Object->TryGetArrayField(TEXT("requiredAnchors"), Anchors) || !Anchors)
		{
			OutError = TEXT("LayoutInvalid");
			ResetDefinitions();
			return false;
		}
		if (Layouts.Contains(FName(*Id)))
		{
			OutError = TEXT("LayoutDuplicateId");
			ResetDefinitions();
			return false;
		}
		FAuraGameplayArenaLayoutDefinition& Definition = Layouts.Add(FName(*Id));
		Definition.Id = FName(*Id);
		Definition.bVerified = bVerified;
		for (const TSharedPtr<FJsonValue>& Anchor : *Anchors)
		{
			const FString AnchorId = Anchor.IsValid() ? Anchor->AsString() : FString();
			const FName AnchorName(*AnchorId);
			if (AnchorId.IsEmpty() || AnchorName.IsNone() || Definition.RequiredAnchors.Contains(AnchorName))
			{
				OutError = TEXT("LayoutAnchorInvalid");
				ResetDefinitions();
				return false;
			}
			Definition.RequiredAnchors.Add(AnchorName);
		}
	}

	bool bHasVerifiedLayout = false;
	for (const TPair<FName, FAuraGameplayEncounterDefinition>& EncounterPair : Encounters)
	{
		bool bAnchorFound = false;
		for (const TPair<FName, FAuraGameplayArenaLayoutDefinition>& Pair : Layouts)
		{
			if (!Pair.Value.bVerified) continue;
			bHasVerifiedLayout = true;
			for (const FName& Anchor : Pair.Value.RequiredAnchors)
			{
				if (Anchor == EncounterPair.Value.LayoutAnchorId)
				{
					bAnchorFound = true;
					break;
				}
			}
			if (bAnchorFound) break;
		}
		if (bHasVerifiedLayout && !bAnchorFound)
		{
			OutError = TEXT("EncounterLayoutAnchorReferenceInvalid");
			ResetDefinitions();
			return false;
		}
	}

	bDefinitionsLoaded = true;
	bRuntimeReady = bHasVerifiedLayout;
	RuntimeBlocker = bHasVerifiedLayout ? FString() : TEXT("ANCHOR_SURVEY_UNVERIFIED");
	for (const TPair<FName, FAuraGameplayArenaLayoutDefinition>& Pair : Layouts)
	{
		if (!Pair.Value.bVerified || Pair.Value.RequiredAnchors.Num() == 0)
		{
			bRuntimeReady = false;
			RuntimeBlocker = TEXT("ANCHOR_SURVEY_UNVERIFIED");
			break;
		}
	}
	if (bRuntimeReady)
	{
		RuntimeBlocker.Reset();
	}
	return true;
}
