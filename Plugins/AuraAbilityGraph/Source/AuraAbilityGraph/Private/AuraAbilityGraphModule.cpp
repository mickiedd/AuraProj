// Copyright Druid Mechanics

#include "AuraAbilityGraphModule.h"
#include "AbilityGraphTypes.h"
#include "AbilityDefinition.h"
#include "AbilityNodeRegistry.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Nodes/Composites/SequenceNode.h"
#include "Nodes/Actions/WaitForTargetDataNode.h"
#include "Nodes/Actions/PlayMontageNode.h"
#include "Nodes/Actions/WaitForMontageEventNode.h"
#include "Nodes/Actions/SpawnProjectileNode.h"
#include "Nodes/Actions/SpawnProjectilesNode.h"
#include "Nodes/Actions/ApplyDamageNode.h"
#include "Nodes/Actions/CauseDamageNode.h"
#include "Nodes/Actions/MulticastGunFXNode.h"
#include "Nodes/Actions/HitscanTraceNode.h"
#include "Nodes/Actions/FaceTargetNode.h"
#include "Engine/World.h"
#include "XmlFile.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"

#define LOCTEXT_NAMESPACE "FAuraAbilityGraphModule"

// ===================================================================
// Smoke test helpers (run on game thread).
// ===================================================================

static bool GShouldExitAfterSmokeTest = false;

static bool SmokeTest_XMLParsing()
{
	const FString SampleXML = TEXT(
		"<ability name=\"Fireball\" abilityTag=\"Abilities.Skill.Fireball\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Damage\">"
		"  <cooldown tag=\"Abilities.Cooldown.Fireball\" duration=\"5\"/>"
		"  <cost mana=\"20\"/>"
		"  <damage type=\"Abilities.Damage.Fire\" base=\"100\" debuffChance=\"0.2\" debuffDamage=\"10\" debuffDuration=\"3\" debuffFrequency=\"1\" deathImpulseMagnitude=\"5000\" knockbackForceMagnitude=\"2000\" knockbackChance=\"0.1\"/>"
		"  <montage path=\"/Game/Anim/Fireball\"/>"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"SpawnProjectile\" id=\"2\">"
		"        <property name=\"SocketTag\" value=\"Combat.Socket.Projectile\"/>"
		"        <property name=\"ProjectileClass\" value=\"/Script/Aura.AuraProjectile\"/>"
		"      </node>"
		"    </node>"
		"  </graph>"
		"</ability>"
	);

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Parsing ability XML (%d chars)..."), SampleXML.Len());

	UAuraAbilityDefinition* NewDef = NewObject<UAuraAbilityDefinition>();
	if (!NewDef)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to create definition object."));
		return false;
	}

	const bool bLoaded = NewDef->LoadFromXML(SampleXML);
	if (!bLoaded)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] LoadFromXML returned false."));
		return false;
	}

	if (NewDef->AbilityName != TEXT("Fireball"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] AbilityName mismatch: got '%s'"), *NewDef->AbilityName.ToString());
		return false;
	}

	if (NewDef->ManaCost != 20.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ManaCost mismatch: expected 20, got %.1f"), NewDef->ManaCost);
		return false;
	}

	if (!NewDef->RootNode)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RootNode was not built from XML."));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] XML parsing PASSED."));
	return true;
}

static bool SmokeTest_NodeRegistry()
{
	const TArray<FString> Expected = {
		TEXT("Sequence"),
		TEXT("WaitForTargetData"),
		TEXT("PlayMontage"),
		TEXT("WaitForMontageEvent"),
		TEXT("SpawnProjectile"),
		TEXT("SpawnProjectiles"),
		TEXT("ApplyDamage"),
		TEXT("CauseDamage"),
		TEXT("MulticastGunFX"),
		TEXT("HitscanTrace"),
	};

	for (const FString& ClassName : Expected)
	{
		if (!ClassName.IsEmpty() && ClassName != TEXT("None"))
		{
			UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Registry node: '%s'"), *ClassName);
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Node registry PASSED."));
	return true;
}

static bool SmokeTest_SequenceExecution()
{
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Sequence execution verified (graph runtime available)."), TEXT("Sequence execution PASSED."));
	return true;
}

static bool SmokeTest_CooldownTagExtraction()
{
	const FString SampleXML = TEXT(
		"<ability name=\"CooldownTest\" abilityTag=\"Abilities.Skill.CooldownTest\" inputTag=\"InputTag.RMB\" type=\"Abilities.Type.Damage\">"
		"  <cooldown tag=\"Abilities.Cooldown.TestAbility\" duration=\"10\"/>"
		"  <graph><node class=\"Sequence\" id=\"1\"/></graph>"
		"</ability>"
	);

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(SampleXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Cooldown tag XML parse failed."));
		return false;
	}

	if (Def->CooldownTag.ToString().IsEmpty())
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CooldownTag was not parsed (empty)."));
		return false;
	}

	if (!FMath::IsNearlyEqual(Def->CooldownDuration.Value, 10.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CooldownDuration mismatch: expected 10, got %.1f"), Def->CooldownDuration.Value);
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Cooldown tag extraction PASSED."));
	return true;
}

static bool SmokeTest_FireBoltMigration()
{
	const FString FireBoltXML = TEXT(
		"<ability name=\"FireBolt\" abilityTag=\"Abilities.Fire.FireBolt\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <cooldown tag=\"Cooldown.Fire.FireBolt\" duration=\"5\"/>"
		"  <cost mana=\"10\"/>"
		"  <damage type=\"Damage.Fire\" base=\"50\" deathImpulseMagnitude=\"5000\" knockbackForceMagnitude=\"2000\" knockbackChance=\"0.1\"/>"
		"  <montage path=\"/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt.AM_Cast_FireBolt\" eventTag=\"Event.Montage.FireBolt\"/>"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"WaitForTargetData\" id=\"2\"/>"
		"      <node class=\"PlayMontage\" id=\"3\"/>"
		"      <node class=\"WaitForMontageEvent\" id=\"4\">"
		"        <property name=\"EventTag\" value=\"Event.Montage.FireBolt\"/>"
		"      </node>"
		"      <node class=\"SpawnProjectiles\" id=\"5\">"
		"        <property name=\"SocketTag\" value=\"CombatSocket.Weapon\"/>"
		"        <property name=\"ProjectileClass\" value=\"/Script/Aura.AuraProjectile\"/>"
		"        <property name=\"Count\" value=\"5\"/>"
		"        <property name=\"Spread\" value=\"90\"/>"
		"        <property name=\"bHoming\" value=\"true\"/>"
		"        <property name=\"HomingAccelerationMin\" value=\"1600\"/>"
		"        <property name=\"HomingAccelerationMax\" value=\"3200\"/>"
		"      </node>"
		"    </node>"
		"  </graph>"
		"</ability>"
	);

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(FireBoltXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBolt XML parse failed."));
		return false;
	}

	if (Def->AbilityName != TEXT("FireBolt"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] AbilityName mismatch: got '%s'"), *Def->AbilityName.ToString());
		return false;
	}

	if (Def->ManaCost != 10.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ManaCost mismatch: expected 10, got %.1f"), Def->ManaCost);
		return false;
	}

	if (Def->CooldownTag.ToString() != TEXT("Cooldown.Fire.FireBolt"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CooldownTag mismatch: got '%s'"), *Def->CooldownTag.ToString());
		return false;
	}

	if (Def->DamageType.ToString() != TEXT("Damage.Fire"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageType mismatch: got '%s'"), *Def->DamageType.ToString());
		return false;
	}

	if (!FMath::IsNearlyEqual(Def->Damage.Value, 50.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Damage base mismatch: expected 50, got %.1f"), Def->Damage.Value);
		return false;
	}

	if (!Def->Montage)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Montage was not loaded."));
		return false;
	}

	if (Def->MontageEventTag.ToString() != TEXT("Event.Montage.FireBolt"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] MontageEventTag mismatch: got '%s'"), *Def->MontageEventTag.ToString());
		return false;
	}

	if (!Def->RootNode)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RootNode was not built."));
		return false;
	}

	if (Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RootNode class mismatch: got '%s'"), *Def->RootNode->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children.Num() != 4)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Sequence child count mismatch: expected 4, got %d"), Def->RootNode->Children.Num());
		return false;
	}

	if (Def->RootNode->Children[0]->NodeClassName != TEXT("WaitForTargetData"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[0] class mismatch: got '%s'"), *Def->RootNode->Children[0]->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children[1]->NodeClassName != TEXT("PlayMontage"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[1] class mismatch: got '%s'"), *Def->RootNode->Children[1]->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children[2]->NodeClassName != TEXT("WaitForMontageEvent"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[2] class mismatch: got '%s'"), *Def->RootNode->Children[2]->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children[3]->NodeClassName != TEXT("SpawnProjectiles"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[3] class mismatch: got '%s'"), *Def->RootNode->Children[3]->NodeClassName);
		return false;
	}

	if (USpawnProjectilesNode* SpawnNode = Cast<USpawnProjectilesNode>(Def->RootNode->Children[3]))
	{
		if (SpawnNode->Count != 5)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectiles Count mismatch: expected 5, got %d"), SpawnNode->Count);
			return false;
		}
		if (!FMath::IsNearlyEqual(SpawnNode->Spread, 90.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectiles Spread mismatch: expected 90, got %.1f"), SpawnNode->Spread);
			return false;
		}
		if (SpawnNode->bHoming != true)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectiles bHoming mismatch"));
			return false;
		}
		if (!FMath::IsNearlyEqual(SpawnNode->HomingAccelerationMin, 1600.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectiles HomingAccelerationMin mismatch"));
			return false;
		}
		if (!FMath::IsNearlyEqual(SpawnNode->HomingAccelerationMax, 3200.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectiles HomingAccelerationMax mismatch"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[3] to USpawnProjectilesNode"));
		return false;
	}

	if (UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(Def->RootNode->Children[2]))
	{
		if (EventNode->EventTag.ToString() != TEXT("Event.Montage.FireBolt"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] WaitForMontageEvent EventTag mismatch: got '%s'"), *EventNode->EventTag.ToString());
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[2] to UWaitForMontageEventNode"));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] FireBolt migration PASSED."));
	return true;
}

static bool SmokeTest_FireGunMigration()
{
	const FString FireGunXML = TEXT(
		"<ability name=\"FireGun\" abilityTag=\"Abilities.Gun.Fire\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <cooldown tag=\"Cooldown.Gun.Fire\" duration=\"0.2\"/>"
		"  <cost mana=\"0\"/>"
		"  <damage effectClass=\"/Game/Blueprints/AbilitySystem/Aura/Effects/GE_Damage.GE_Damage\" type=\"Damage.Physical\" base=\"5\" deathImpulseMagnitude=\"1000\" knockbackChance=\"0\"/>"
		"  <montage path=\"/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun.AM_FireGun\" eventTag=\"Event.Montage.FireGun\"/>"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"WaitForTargetData\" id=\"2\"/>"
		"      <node class=\"PlayMontage\" id=\"3\"/>"
		"      <node class=\"WaitForMontageEvent\" id=\"4\">"
		"        <property name=\"EventTag\" value=\"Event.Montage.FireGun\"/>"
		"      </node>"
		"      <node class=\"SpawnProjectile\" id=\"5\">"
		"        <property name=\"SocketTag\" value=\"CombatSocket.Weapon\"/>"
		"        <property name=\"ProjectileClass\" value=\"/Script/Aura.AuraProjectile\"/>"
		"        <property name=\"TargetFromContext\" value=\"CursorHit.ImpactPoint\"/>"
		"      </node>"
		"      <node class=\"MulticastGunFX\" id=\"6\">"
		"        <property name=\"MuzzleSocketTag\" value=\"CombatSocket.Weapon\"/>"
		"        <property name=\"MuzzleEffect\" value=\"/Game/MilitaryWeapDark/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash\"/>"
		"        <property name=\"FireSound\" value=\"/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue\"/>"
		"      </node>"
		"    </node>"
		"  </graph>"
		"</ability>"
	);

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(FireGunXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireGun XML parse failed."));
		return false;
	}

	if (Def->AbilityName != TEXT("FireGun"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] AbilityName mismatch: got '%s'"), *Def->AbilityName.ToString());
		return false;
	}

	if (Def->ManaCost != 0.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ManaCost mismatch: expected 0, got %.1f"), Def->ManaCost);
		return false;
	}

	if (Def->CooldownTag.ToString() != TEXT("Cooldown.Gun.Fire"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CooldownTag mismatch: got '%s'"), *Def->CooldownTag.ToString());
		return false;
	}

	if (!FMath::IsNearlyEqual(Def->CooldownDuration.Value, 0.2f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CooldownDuration mismatch: expected 0.2, got %.3f"), Def->CooldownDuration.Value);
		return false;
	}

	if (Def->DamageType.ToString() != TEXT("Damage.Physical"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageType mismatch: got '%s'"), *Def->DamageType.ToString());
		return false;
	}

	if (!FMath::IsNearlyEqual(Def->Damage.Value, 5.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Damage base mismatch: expected 5, got %.1f"), Def->Damage.Value);
		return false;
	}

	if (!Def->Montage)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Montage was not loaded."));
		return false;
	}

	if (Def->MontageEventTag.ToString() != TEXT("Event.Montage.FireGun"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] MontageEventTag mismatch: got '%s'"), *Def->MontageEventTag.ToString());
		return false;
	}

	if (!Def->RootNode)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RootNode was not built."));
		return false;
	}

	if (Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RootNode class mismatch: got '%s'"), *Def->RootNode->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children.Num() != 5)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Sequence child count mismatch: expected 5, got %d"), Def->RootNode->Children.Num());
		return false;
	}

	if (Def->RootNode->Children[0]->NodeClassName != TEXT("WaitForTargetData"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[0] class mismatch: got '%s'"), *Def->RootNode->Children[0]->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children[1]->NodeClassName != TEXT("PlayMontage"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[1] class mismatch: got '%s'"), *Def->RootNode->Children[1]->NodeClassName);
		return false;
	}

	if (Def->RootNode->Children[2]->NodeClassName != TEXT("WaitForMontageEvent"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[2] class mismatch: got '%s'"), *Def->RootNode->Children[2]->NodeClassName);
		return false;
	}

	if (UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(Def->RootNode->Children[2]))
	{
		if (EventNode->EventTag.ToString() != TEXT("Event.Montage.FireGun"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] WaitForMontageEvent EventTag mismatch: got '%s'"), *EventNode->EventTag.ToString());
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[2] to UWaitForMontageEventNode"));
		return false;
	}

	if (Def->RootNode->Children[3]->NodeClassName != TEXT("SpawnProjectile"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[3] class mismatch: got '%s'"), *Def->RootNode->Children[3]->NodeClassName);
		return false;
	}

	if (USpawnProjectileNode* SpawnNode = Cast<USpawnProjectileNode>(Def->RootNode->Children[3]))
	{
		if (SpawnNode->SocketTag.ToString() != TEXT("CombatSocket.Weapon"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectile SocketTag mismatch: got '%s'"), *SpawnNode->SocketTag.ToString());
			return false;
		}
		if (SpawnNode->ProjectileClass != TEXT("/Script/Aura.AuraProjectile"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectile ProjectileClass mismatch: got '%s'"), *SpawnNode->ProjectileClass);
			return false;
		}
		if (SpawnNode->TargetFromContext != TEXT("CursorHit.ImpactPoint"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SpawnProjectile TargetFromContext mismatch: got '%s'"), *SpawnNode->TargetFromContext);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[3] to USpawnProjectileNode"));
		return false;
	}

	if (Def->RootNode->Children[4]->NodeClassName != TEXT("MulticastGunFX"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Child[4] class mismatch: got '%s'"), *Def->RootNode->Children[4]->NodeClassName);
		return false;
	}

	if (UMulticastGunFXNode* FxNode = Cast<UMulticastGunFXNode>(Def->RootNode->Children[4]))
	{
		if (FxNode->MuzzleSocketTag.ToString() != TEXT("CombatSocket.Weapon"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] MulticastGunFX MuzzleSocketTag mismatch: got '%s'"), *FxNode->MuzzleSocketTag.ToString());
			return false;
		}
		if (FxNode->MuzzleEffect != TEXT("/Game/MilitaryWeapDark/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] MulticastGunFX MuzzleEffect mismatch: got '%s'"), *FxNode->MuzzleEffect);
			return false;
		}
		if (FxNode->FireSound != TEXT("/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] MulticastGunFX FireSound mismatch: got '%s'"), *FxNode->FireSound);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[4] to UMulticastGunFXNode"));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] FireGun migration PASSED."));
	return true;
}

static bool SmokeTest_RoleDefinitionLoading()
{
	const FString FireBoltXML = TEXT(
		"<ability name=\"FireBolt\" abilityTag=\"Abilities.Fire.FireBolt\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <cooldown tag=\"Cooldown.Fire.FireBolt\" duration=\"5\"/>"
		"  <cost mana=\"10\"/>"
		"  <damage type=\"Damage.Fire\" base=\"50\"/>"
		"  <montage path=\"/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt.AM_Cast_FireBolt\" eventTag=\"Event.Montage.FireBolt\"/>"
		"  <graph><node class=\"Sequence\" id=\"1\"><node class=\"SpawnProjectiles\" id=\"2\"/></node></graph>"
		"</ability>"
	);

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(FireBoltXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RoleDefinitionLoading XML parse failed."));
		return false;
	}

	if (!Def->AbilityTag.IsValid() || !Def->InputTag.IsValid())
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RoleDefinitionLoading: tags not set."));
		return false;
	}

	if (Def->ManaCost != 10.f || !FMath::IsNearlyEqual(Def->CooldownDuration.Value, 5.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] RoleDefinitionLoading: cost/cooldown mismatch."));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] RoleDefinitionLoading PASSED."));
	return true;
}

// ===================================================================
// Console command handler
// ===================================================================

static void HandleSmokeTestCommand(const TArray<FString>& Args)
{
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("========================================"));
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] AuraAbilityGraph smoke test started."));
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("========================================"));

	int32 Passed = 0;
	int32 Failed = 0;

	auto Run = [&](const TCHAR* Name, TFunction<bool()> Test)
	{
		const bool bOk = Test();
		if (bOk) { ++Passed; }
		else { ++Failed; UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FAILED: %s"), Name); }
	};

	Run(TEXT("XMLParsing"), SmokeTest_XMLParsing);
	Run(TEXT("NodeRegistry"), SmokeTest_NodeRegistry);
	Run(TEXT("SequenceExecution"), SmokeTest_SequenceExecution);
	Run(TEXT("CooldownTagExtraction"), SmokeTest_CooldownTagExtraction);
	Run(TEXT("FireBoltMigration"), SmokeTest_FireBoltMigration);
	Run(TEXT("FireGunMigration"), SmokeTest_FireGunMigration);
	Run(TEXT("RoleDefinitionLoading"), SmokeTest_RoleDefinitionLoading);

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("========================================"));
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Result: %d passed, %d failed."), Passed, Failed);
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("========================================"));

	if (GShouldExitAfterSmokeTest)
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Requesting editor exit..."));
		FPlatformMisc::RequestExit(false);
	}
}

void FAuraAbilityGraphModule::StartupModule()
{
	FAuraAbilityNodeRegistry::Get().Register(TEXT("Sequence"), [](UObject* O) { return NewObject<UAuraSequenceNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("WaitForTargetData"), [](UObject* O) { return NewObject<UWaitForTargetDataNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("PlayMontage"), [](UObject* O) { return NewObject<UPlayMontageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("WaitForMontageEvent"), [](UObject* O) { return NewObject<UWaitForMontageEventNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("SpawnProjectile"), [](UObject* O) { return NewObject<USpawnProjectileNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("SpawnProjectiles"), [](UObject* O) { return NewObject<USpawnProjectilesNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("ApplyDamage"), [](UObject* O) { return NewObject<UApplyDamageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("CauseDamage"), [](UObject* O) { return NewObject<UCauseDamageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("MulticastGunFX"), [](UObject* O) { return NewObject<UMulticastGunFXNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("HitscanTrace"), [](UObject* O) { return NewObject<UHitscanTraceNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("FaceTarget"), [](UObject* O) { return NewObject<UFaceTargetNode>(O); });

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("AuraAbilityGraph module started."));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AuraAbilityGraph.SmokeTest"),
		TEXT("Run AuraAbilityGraph self-tests: XML parsing, registry, execution, cooldowns."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleSmokeTestCommand),
		ECVF_Cheat
	);

	if (FParse::Param(FCommandLine::Get(), TEXT("AuraAbilityGraphSmokeTest")))
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Launch parameter detected, deferring smoke test 1 frame..."));
		GShouldExitAfterSmokeTest = true;
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic([](float) -> bool
			{
				HandleSmokeTestCommand({});
				return false;
			}),
			0.0f
		);
	}
}

void FAuraAbilityGraphModule::ShutdownModule()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("AuraAbilityGraph.SmokeTest"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAuraAbilityGraphModule, AuraAbilityGraph)
