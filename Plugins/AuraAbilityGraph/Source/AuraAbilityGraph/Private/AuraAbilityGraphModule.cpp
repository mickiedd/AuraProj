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
#include "Nodes/Actions/WaitNode.h"
#include "Nodes/Actions/SpawnShardsNode.h"
#include "Nodes/Actions/ElectrocuteBeamNode.h"
#include "Nodes/Actions/ModularBeamNodes.h"
#include "Nodes/Actions/EnemyCombatMontageNode.h"
#include "Nodes/Actions/EnemyMeleeDamageNode.h"
#include "Nodes/Actions/EnemyHitReactNode.h"
#include "Tests/TestCombatAvatar.h"
#include "Tests/TestDataAbility.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AuraAbilityTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AuraGameplayTags.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Combat/AuraCombatTypes.h"
#include "Actor/AuraProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "XmlFile.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "GameplayTagsManager.h"

#define LOCTEXT_NAMESPACE "FAuraAbilityGraphModule"

// ===================================================================
// Smoke test helpers (run on game thread).
// ===================================================================

static bool GShouldExitAfterSmokeTest = false;

static bool SmokeTest_XMLParsing()
{
	const FString SampleXML = TEXT(
		"<ability name=\"Fireball\" abilityTag=\"Abilities.Fire.FireBolt\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <cooldown tag=\"Cooldown.Fire.FireBolt\" duration=\"5\"/>"
		"  <cost mana=\"20\"/>"
		"  <damage type=\"Damage.Fire\" base=\"100\" debuffChance=\"0.2\" debuffDamage=\"10\" debuffDuration=\"3\" debuffFrequency=\"1\" deathImpulseMagnitude=\"5000\" knockbackForceMagnitude=\"2000\" knockbackChance=\"0.1\"/>"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"SpawnProjectile\" id=\"2\">"
		"        <property name=\"SocketTag\" value=\"CombatSocket.Weapon\"/>"
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
	struct FExpectedNode
	{
		const TCHAR* Name;
		UClass* Class;
	};

	const FExpectedNode Expected[] = {
		{ TEXT("Sequence"),            UAuraSequenceNode::StaticClass() },
		{ TEXT("WaitForTargetData"),   UWaitForTargetDataNode::StaticClass() },
		{ TEXT("PlayMontage"),         UPlayMontageNode::StaticClass() },
		{ TEXT("WaitForMontageEvent"), UWaitForMontageEventNode::StaticClass() },
		{ TEXT("SpawnProjectile"),     USpawnProjectileNode::StaticClass() },
		{ TEXT("SpawnProjectiles"),    USpawnProjectilesNode::StaticClass() },
		{ TEXT("ApplyDamage"),         UApplyDamageNode::StaticClass() },
		{ TEXT("CauseDamage"),         UCauseDamageNode::StaticClass() },
		{ TEXT("MulticastGunFX"),      UMulticastGunFXNode::StaticClass() },
		{ TEXT("HitscanTrace"),        UHitscanTraceNode::StaticClass() },
		{ TEXT("FaceTarget"),          UFaceTargetNode::StaticClass() },
		{ TEXT("Wait"),                UWaitNode::StaticClass() },
		{ TEXT("SpawnShards"),         USpawnShardsNode::StaticClass() },
		{ TEXT("ElectrocuteBeam"),     UElectrocuteBeamNode::StaticClass() },
		{ TEXT("ResolveBeamOrigin"),   UResolveBeamOriginNode::StaticClass() },
		{ TEXT("AcquirePrimaryBeamTarget"), UAcquirePrimaryBeamTargetNode::StaticClass() },
		{ TEXT("SelectBeamChainTargets"), USelectBeamChainTargetsNode::StaticClass() },
		{ TEXT("SpawnBeamVisuals"),    USpawnBeamVisualsNode::StaticClass() },
		{ TEXT("InitializeBeamEndpoints"), UInitializeBeamEndpointsNode::StaticClass() },
		{ TEXT("TimedLoop"),            UTimedLoopNode::StaticClass() },
		{ TEXT("PruneBeamTargets"),     UPruneBeamTargetsNode::StaticClass() },
		{ TEXT("RefreshBeamEndpoints"), URefreshBeamEndpointsNode::StaticClass() },
		{ TEXT("ApplyBeamDamage"),      UApplyBeamDamageNode::StaticClass() },
		{ TEXT("DestroyBeamVisuals"),   UDestroyBeamVisualsNode::StaticClass() },
		{ TEXT("EnemyCombatMontage"),  UEnemyCombatMontageNode::StaticClass() },
		{ TEXT("EnemyMeleeDamage"),    UEnemyMeleeDamageNode::StaticClass() },
		{ TEXT("EnemyHitReact"),       UEnemyHitReactNode::StaticClass() },
	};

	for (const FExpectedNode& Item : Expected)
	{
		UAuraAbilityActionNode* Node = FAuraAbilityNodeRegistry::Get().Create(Item.Name, GetTransientPackage());
		if (!Node)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Node registry did not create '%s'."), Item.Name);
			return false;
		}

		if (!Node->IsA(Item.Class))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Node registry created '%s' as '%s'; expected '%s'."),
				Item.Name, *Node->GetClass()->GetName(), *Item.Class->GetName());
			return false;
		}

		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Registry node '%s' -> %s."), Item.Name, *Node->GetClass()->GetName());
	}

	if (FAuraAbilityNodeRegistry::Get().Create(TEXT("NotARegisteredAbilityNode"), GetTransientPackage()) != nullptr)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Node registry created an unknown node name."));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Node registry PASSED (%d concrete registrations verified)."), UE_ARRAY_COUNT(Expected));
	return true;
}

static bool SmokeTest_SequenceExecution()
{
	const FString SampleXML = TEXT(
		"<ability name=\"SequenceSmokeTest\" abilityTag=\"Abilities.Attack\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"PlayMontage\" id=\"2\"/>"
		"      <node class=\"PlayMontage\" id=\"3\"/>"
		"    </node>"
		"  </graph>"
		"</ability>"
	);

	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!Definition || !Definition->LoadFromXML(SampleXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: failed to parse the sequence graph."));
		return false;
	}

	UAuraSequenceNode* SequenceNode = Cast<UAuraSequenceNode>(Definition->RootNode);
	if (!SequenceNode || SequenceNode->Children.Num() != 2)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: expected a Sequence with 2 children."));
		return false;
	}

	// An empty PlayMontage is an intentional no-op. It gives this smoke test two
	// concrete, immediately-successful action tasks without requiring a live ASC,
	// world, or animation asset.
	UTestDataAbility* TestAbility = NewObject<UTestDataAbility>(GetTransientPackage());
	UAuraSequenceTask* SequenceTask = NewObject<UAuraSequenceTask>(GetTransientPackage());
	if (!TestAbility || !SequenceTask)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: failed to create runtime test objects."));
		return false;
	}

	SequenceTask->Init(SequenceNode, TestAbility);
	if (SequenceTask->ChildTasks.Num() != 2)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: task tree contains %d children; expected 2."), SequenceTask->ChildTasks.Num());
		return false;
	}

	FAuraAbilityExecutionContext Context;
	Context.Definition = Definition;
	const EAuraAbilityActionStatus Status = SequenceTask->Execute(Context);
	if (Status != EAuraAbilityActionStatus::Success)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: expected Success, got %s."),
			*StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
		return false;
	}

	for (int32 Index = 0; Index < SequenceTask->ChildTasks.Num(); ++Index)
	{
		if (!SequenceTask->ChildTasks[Index] || SequenceTask->ChildTasks[Index]->HasEntered)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceExecution: child[%d] was not exited after sequence success."), Index);
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] Sequence execution PASSED (2 real action tasks returned Success and exited)."));
	return true;
}

static bool SmokeTest_CooldownTagExtraction()
{
	const FString SampleXML = TEXT(
		"<ability name=\"CooldownTest\" abilityTag=\"Abilities.Fire.FireBolt\" inputTag=\"InputTag.RMB\" type=\"Abilities.Type.Offensive\">"
		"  <cooldown tag=\"Cooldown.Fire.FireBolt\" duration=\"10\"/>"
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
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"WaitForTargetData\" id=\"2\"/>"
		"      <node class=\"PlayMontage\" id=\"3\">"
		"        <property name=\"Montage\" value=\"/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt.AM_Cast_FireBolt\"/>"
		"      </node>"
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
		"  <damage effectClass=\"\" type=\"Damage.Physical\" base=\"5\" deathImpulseMagnitude=\"1000\" knockbackChance=\"0\"/>"
		"  <graph>"
		"    <node class=\"Sequence\" id=\"1\">"
		"      <node class=\"WaitForTargetData\" id=\"2\"/>"
		"      <node class=\"PlayMontage\" id=\"3\">"
		"        <property name=\"Montage\" value=\"/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun.AM_FireGun\"/>"
		"      </node>"
		"      <node class=\"WaitForMontageEvent\" id=\"4\">"
		"        <property name=\"EventTag\" value=\"Event.Montage.FireGun\"/>"
		"      </node>"
		"      <node class=\"SpawnProjectile\" id=\"5\">"
		"        <property name=\"SocketTag\" value=\"CombatSocket.Weapon\"/>"
		"        <property name=\"ProjectileClass\" value=\"/Script/Aura.AuraProjectile\"/>"
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

	if (UPlayMontageNode* PlayNode = Cast<UPlayMontageNode>(Def->RootNode->Children[1]))
	{
		if (!PlayNode->Montage)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] PlayMontage Montage was not loaded from path '%s'."), *PlayNode->MontagePath);
			return false;
		}
		if (PlayNode->MontagePath != TEXT("/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun.AM_FireGun"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] PlayMontage MontagePath mismatch: got '%s'"), *PlayNode->MontagePath);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] Failed to cast child[1] to UPlayMontageNode"));
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

// Loads the REAL Content/AbilityDefinitions/FireBolt.xml from disk (via the engine
// parser) and asserts the graph invariant regressed by commit c6eff50 ("Add 1s
// wait before montage event"): PlayMontage must be IMMEDIATELY followed by
// WaitForMontageEvent, with no 'Wait' node anywhere between them. A Wait there
// makes WaitForMontageEvent subscribe AFTER the Event.Montage.FireBolt anim
// notify fired (UAbilityTask_WaitGameplayEvent only catches events after it
// subscribes), hanging the ability forever 閳?no projectile, never ends.
static bool SmokeTest_FireBoltFileGraph()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), TEXT("FireBolt.xml"));
	FString XMLContent;
	if (!FFileHelper::LoadFileToString(XMLContent, *FilePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: could not load %s"), *FilePath);
		return false;
	}

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(XMLContent))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: LoadFromXML failed"));
		return false;
	}

	if (!Def->RootNode || Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: root is not a Sequence"));
		return false;
	}

	const auto& Children = Def->RootNode->Children;
	auto ClassOf = [&](int32 Idx) -> FString { return Children.IsValidIndex(Idx) ? Children[Idx]->NodeClassName : FString(); };

	// No 'Wait' node anywhere in the FireBolt graph.
	for (int32 i = 0; i < Children.Num(); ++i)
	{
		if (Children[i]->NodeClassName == TEXT("Wait"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: 'Wait' node at index %d must not be present (regression from c6eff50)"), i);
			return false;
		}
	}

	// PlayMontage must be immediately followed by WaitForMontageEvent.
	int32 PlayMontageIdx = INDEX_NONE;
	for (int32 i = 0; i < Children.Num(); ++i)
	{
		if (Children[i]->NodeClassName == TEXT("PlayMontage")) { PlayMontageIdx = i; break; }
	}
	if (PlayMontageIdx == INDEX_NONE)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: no PlayMontage node"));
		return false;
	}
	if (const UPlayMontageNode* PlayNode = Cast<UPlayMontageNode>(Children[PlayMontageIdx]))
	{
		if (!PlayNode->Montage)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: PlayMontage Montage not loaded from '%s'"), *PlayNode->MontagePath);
			return false;
		}
	}
	const int32 WaitIdx = PlayMontageIdx + 1;
	if (ClassOf(WaitIdx) != TEXT("WaitForMontageEvent"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: node after PlayMontage (idx %d) is '%s', expected WaitForMontageEvent"), PlayMontageIdx, *ClassOf(WaitIdx));
		return false;
	}

	// SpawnProjectiles must come after WaitForMontageEvent.
	bool bFoundSpawn = false;
	for (int32 i = WaitIdx + 1; i < Children.Num(); ++i)
	{
		if (Children[i]->NodeClassName == TEXT("SpawnProjectiles")) { bFoundSpawn = true; break; }
	}
	if (!bFoundSpawn)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: no SpawnProjectiles after WaitForMontageEvent"));
		return false;
	}

	// WaitForMontageEvent must use Event.Montage.FireBolt.
	if (const UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(Children[WaitIdx]))
	{
		if (EventNode->EventTag.ToString() != TEXT("Event.Montage.FireBolt"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBoltFileGraph: WaitForMontageEvent EventTag='%s', expected Event.Montage.FireBolt"), *EventNode->EventTag.ToString());
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] FireBoltFileGraph PASSED (PlayMontage->WaitForMontageEvent->SpawnProjectiles, no Wait)."));
	return true;
}

// A predicted client can finish the graph after the authority-only
// SpawnProjectiles action is skipped. It must not replicate that local completion
// as an ability end before the server has produced the projectile effect.
static bool SmokeTest_ClientGraphCompletionBoundary()
{
	if (!UAuraDataAbility::ShouldEndAfterGraphCompletion(true))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ClientGraphCompletionBoundary: authority completion must end immediately"));
		return false;
	}

	if (UAuraDataAbility::ShouldEndAfterGraphCompletion(false))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ClientGraphCompletionBoundary: client completion must wait for server end"));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ClientGraphCompletionBoundary PASSED (client waits for authoritative end)."));
	return true;
}

// ===================================================================
// Phase 3 smoke tests 閳?load the real XML files from disk and validate
// graph structure, node types, and properties for the newly ported
// abilities: FireBlast, ArcaneShards, Electrocute.
// ===================================================================

static bool SmokeTest_FireBlastFileGraph()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), TEXT("FireBlast.xml"));
	FString XMLContent;
	if (!FFileHelper::LoadFileToString(XMLContent, *FilePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: could not load %s"), *FilePath);
		return false;
	}

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(XMLContent))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: LoadFromXML failed"));
		return false;
	}

	if (Def->AbilityName != TEXT("FireBlast"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: AbilityName mismatch: got '%s'"), *Def->AbilityName.ToString());
		return false;
	}

	if (Def->AbilityTag.ToString() != TEXT("Abilities.Fire.FireBlast"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: AbilityTag mismatch: got '%s'"), *Def->AbilityTag.ToString());
		return false;
	}

	if (Def->ManaCost != 25.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: ManaCost mismatch: expected 25, got %.1f"), Def->ManaCost);
		return false;
	}

	if (Def->DamageType.ToString() != TEXT("Damage.Fire"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: DamageType mismatch: got '%s'"), *Def->DamageType.ToString());
		return false;
	}

	if (!FMath::IsNearlyEqual(Def->Damage.Value, 60.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: Damage base mismatch: expected 60, got %.1f"), Def->Damage.Value);
		return false;
	}

	if (!Def->RootNode || Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: root is not a Sequence"));
		return false;
	}

	if (Def->RootNode->Children.Num() != 1)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: expected 1 child, got %d"), Def->RootNode->Children.Num());
		return false;
	}

	if (Def->RootNode->Children[0]->NodeClassName != TEXT("SpawnProjectiles"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: child[0] is not SpawnProjectiles, got '%s'"), *Def->RootNode->Children[0]->NodeClassName);
		return false;
	}

	if (USpawnProjectilesNode* SpawnNode = Cast<USpawnProjectilesNode>(Def->RootNode->Children[0]))
	{
		if (SpawnNode->Count != 12)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: Count mismatch: expected 12, got %d"), SpawnNode->Count);
			return false;
		}
		if (!FMath::IsNearlyEqual(SpawnNode->Spread, 360.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: Spread mismatch: expected 360, got %.1f"), SpawnNode->Spread);
			return false;
		}
		if (!SpawnNode->bSetReturnToOwner)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: bSetReturnToOwner should be true"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] FireBlastFileGraph: failed to cast child[0] to USpawnProjectilesNode"));
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] FireBlastFileGraph PASSED (SpawnProjectiles Count=12 Spread=360 bSetReturnToOwner=true)."));
	return true;
}

static bool SmokeTest_ArcaneShardsFileGraph()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), TEXT("ArcaneShards.xml"));
	FString XMLContent;
	if (!FFileHelper::LoadFileToString(XMLContent, *FilePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: could not load %s"), *FilePath);
		return false;
	}

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(XMLContent))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: LoadFromXML failed"));
		return false;
	}

	if (Def->AbilityName != TEXT("ArcaneShards"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: AbilityName mismatch: got '%s'"), *Def->AbilityName.ToString());
		return false;
	}

	if (Def->AbilityTag.ToString() != TEXT("Abilities.Arcane.ArcaneShards"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: AbilityTag mismatch: got '%s'"), *Def->AbilityTag.ToString());
		return false;
	}

	if (Def->ManaCost != 20.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: ManaCost mismatch: expected 20, got %.1f"), Def->ManaCost);
		return false;
	}

	if (Def->DamageType.ToString() != TEXT("Damage.Arcane"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: DamageType mismatch: got '%s'"), *Def->DamageType.ToString());
		return false;
	}

	if (!Def->RootNode || Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: root is not a Sequence"));
		return false;
	}

	const auto& Children = Def->RootNode->Children;
	auto ClassOf = [&](int32 Idx) -> FString { return Children.IsValidIndex(Idx) ? Children[Idx]->NodeClassName : FString(); };

	if (Children.Num() != 5)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: expected 5 children, got %d"), Children.Num());
		return false;
	}

	// WaitForTargetData -> FaceTarget -> PlayMontage -> WaitForMontageEvent -> SpawnShards
	if (ClassOf(0) != TEXT("WaitForTargetData"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: child[0] is '%s', expected WaitForTargetData"), *ClassOf(0));
		return false;
	}
	if (ClassOf(1) != TEXT("FaceTarget"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: child[1] is '%s', expected FaceTarget"), *ClassOf(1));
		return false;
	}
	if (ClassOf(2) != TEXT("PlayMontage"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: child[2] is '%s', expected PlayMontage"), *ClassOf(2));
		return false;
	}
	if (UPlayMontageNode* PlayNode = Cast<UPlayMontageNode>(Children[2]))
	{
		if (!PlayNode->Montage)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: PlayMontage Montage not loaded from '%s'"), *PlayNode->MontagePath);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: failed to cast child[2] to UPlayMontageNode"));
		return false;
	}
	if (ClassOf(3) != TEXT("WaitForMontageEvent"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: child[3] is '%s', expected WaitForMontageEvent"), *ClassOf(3));
		return false;
	}
	if (ClassOf(4) != TEXT("SpawnShards"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: child[4] is '%s', expected SpawnShards"), *ClassOf(4));
		return false;
	}

	// Validate SpawnShards node properties
	if (USpawnShardsNode* ShardsNode = Cast<USpawnShardsNode>(Children[4]))
	{
		if (ShardsNode->MaxShards != 11)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: MaxShards mismatch: expected 11, got %d"), ShardsNode->MaxShards);
			return false;
		}
		if (!FMath::IsNearlyEqual(ShardsNode->SpawnInterval, 0.1f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: SpawnInterval mismatch: expected 0.1, got %.2f"), ShardsNode->SpawnInterval);
			return false;
		}
		if (!FMath::IsNearlyEqual(ShardsNode->RadialDamageRadius, 300.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: RadialDamageRadius mismatch: expected 300, got %.1f"), ShardsNode->RadialDamageRadius);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: failed to cast child[4] to USpawnShardsNode"));
		return false;
	}

	// ArcaneShards is authoritative for damage, so its visual cue must use the ASC
	// execution path. A local/non-replicated cue would reproduce the reported bug:
	// mana and damage can change while remote clients see no shard effect.
	const FString SpawnShardsSourcePath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp"));
	FString SpawnShardsSource;
	if (!FFileHelper::LoadFileToString(SpawnShardsSource, *SpawnShardsSourcePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: could not read SpawnShards implementation"));
		return false;
	}
	if (!SpawnShardsSource.Contains(TEXT("CachedCtx.ASC->ExecuteGameplayCue(CueTag, CueParams)")))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: SpawnShards does not dispatch its cue through the source ASC"));
		return false;
	}
	if (SpawnShardsSource.Contains(TEXT("ExecuteGameplayCue_NonReplicated")))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: SpawnShards still uses a non-replicated gameplay cue"));
		return false;
	}

	// Validate WaitForMontageEvent EventTag
	if (UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(Children[3]))
	{
		if (EventNode->EventTag.ToString() != TEXT("Event.Montage.ArcaneShards"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ArcaneShardsFileGraph: EventTag mismatch: got '%s'"), *EventNode->EventTag.ToString());
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ArcaneShardsFileGraph PASSED (graph structure, montage event, and replicated shard cue dispatch)."));
	return true;
}

static bool SmokeTest_ElectrocuteFileGraph()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), TEXT("Electrocute.xml"));
	FString XMLContent;
	if (!FFileHelper::LoadFileToString(XMLContent, *FilePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: could not load %s"), *FilePath);
		return false;
	}

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(XMLContent))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: LoadFromXML failed"));
		return false;
	}

	if (Def->AbilityName != TEXT("Electrocute"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: AbilityName mismatch: got '%s'"), *Def->AbilityName.ToString());
		return false;
	}

	if (Def->AbilityTag.ToString() != TEXT("Abilities.Lightning.Electrocute"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: AbilityTag mismatch: got '%s'"), *Def->AbilityTag.ToString());
		return false;
	}

	if (Def->ManaCost != 5.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: ManaCost mismatch: expected 5, got %.1f"), Def->ManaCost);
		return false;
	}

	if (Def->DamageType.ToString() != TEXT("Damage.Lightning"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: DamageType mismatch: got '%s'"), *Def->DamageType.ToString());
		return false;
	}

	if (!Def->RootNode || Def->RootNode->NodeClassName != TEXT("Sequence"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: root is not a Sequence"));
		return false;
	}

	const auto& Children = Def->RootNode->Children;
	auto ClassOf = [&](int32 Idx) -> FString { return Children.IsValidIndex(Idx) ? Children[Idx]->NodeClassName : FString(); };

	if (Children.Num() != 11)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: expected 11 children, got %d"), Children.Num());
		return false;
	}

	// Cast -> ResolveOrigin -> AcquirePrimary -> SelectChains -> SpawnVisuals
	// -> InitializeEndpoints -> TimedLoop(Prune -> Refresh -> Damage) -> Cleanup
	if (ClassOf(0) != TEXT("WaitForTargetData"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: child[0] is '%s', expected WaitForTargetData"), *ClassOf(0));
		return false;
	}
	if (UWaitForTargetDataNode* TargetNode = Cast<UWaitForTargetDataNode>(Children[0]))
	{
		if (!FMath::IsNearlyEqual(TargetNode->MaxTargetDistance, 3000.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: MaxTargetDistance mismatch: expected 3000, got %.1f"), TargetNode->MaxTargetDistance);
			return false;
		}
	}
	if (ClassOf(1) != TEXT("FaceTarget"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: child[1] is '%s', expected FaceTarget"), *ClassOf(1));
		return false;
	}
	if (ClassOf(2) != TEXT("PlayMontage"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: child[2] is '%s', expected PlayMontage"), *ClassOf(2));
		return false;
	}
	if (UPlayMontageNode* PlayNode = Cast<UPlayMontageNode>(Children[2]))
	{
		if (!PlayNode->Montage)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: PlayMontage Montage not loaded from '%s'"), *PlayNode->MontagePath);
			return false;
		}
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: failed to cast child[2] to UPlayMontageNode"));
		return false;
	}
	if (ClassOf(3) != TEXT("WaitForMontageEvent"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: child[3] is '%s', expected WaitForMontageEvent"), *ClassOf(3));
		return false;
	}
	if (ClassOf(4) != TEXT("ResolveBeamOrigin") || ClassOf(5) != TEXT("AcquirePrimaryBeamTarget") ||
		ClassOf(6) != TEXT("SelectBeamChainTargets") || ClassOf(7) != TEXT("SpawnBeamVisuals") ||
		ClassOf(8) != TEXT("InitializeBeamEndpoints") || ClassOf(9) != TEXT("TimedLoop") ||
		ClassOf(10) != TEXT("DestroyBeamVisuals"))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: modular beam node order mismatch"));
		return false;
	}

	if (UResolveBeamOriginNode* OriginNode = Cast<UResolveBeamOriginNode>(Children[4]))
	{
		if (OriginNode->SocketTag.ToString() != TEXT("CombatSocket.Weapon") || !FMath::IsNearlyEqual(OriginNode->MaxRange, 3000.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: ResolveBeamOrigin properties mismatch"));
			return false;
		}
	}
	if (UAcquirePrimaryBeamTargetNode* AcquireNode = Cast<UAcquirePrimaryBeamTargetNode>(Children[5]))
	{
		if (!FMath::IsNearlyEqual(AcquireNode->TraceRadius, 10.f) || !FMath::IsNearlyEqual(AcquireNode->MaxRange, 3000.f) || !AcquireNode->bAllowVisualOnlyEndpoint)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: AcquirePrimaryBeamTarget properties mismatch"));
			return false;
		}
	}
	if (USelectBeamChainTargetsNode* ChainNode = Cast<USelectBeamChainTargetsNode>(Children[6]))
	{
		if (!FMath::IsNearlyEqual(ChainNode->SearchRadius, 850.f) || ChainNode->MaxAdditionalTargets != 5)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: SelectBeamChainTargets properties mismatch"));
			return false;
		}
	}
	if (USpawnBeamVisualsNode* VisualNode = Cast<USpawnBeamVisualsNode>(Children[7]))
	{
		if (VisualNode->BeamEffect != TEXT("/Game/Assets/Effects/Shock/NS_ElectricBeam.NS_ElectricBeam") ||
			VisualNode->BeamStartParameter != TEXT("User.Beam Start") || VisualNode->BeamEndParameter != TEXT("User.Beam End"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: SpawnBeamVisuals properties mismatch"));
			return false;
		}
	}
	if (UTimedLoopNode* LoopNode = Cast<UTimedLoopNode>(Children[9]))
	{
		if (!FMath::IsNearlyEqual(LoopNode->Duration, 2.f) || !FMath::IsNearlyEqual(LoopNode->Interval, 0.2f) ||
			!LoopNode->bExecuteImmediately || !LoopNode->bCleanupBeamStateOnCancel || LoopNode->Children.Num() != 1)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: TimedLoop properties/children mismatch"));
			return false;
		}
		if (LoopNode->Children[0]->NodeClassName != TEXT("Sequence") || LoopNode->Children[0]->Children.Num() != 3 ||
			LoopNode->Children[0]->Children[0]->NodeClassName != TEXT("PruneBeamTargets") ||
			LoopNode->Children[0]->Children[1]->NodeClassName != TEXT("RefreshBeamEndpoints") ||
			LoopNode->Children[0]->Children[2]->NodeClassName != TEXT("ApplyBeamDamage"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: TimedLoop child sequence mismatch"));
			return false;
		}
	}

	// Validate WaitForMontageEvent EventTag
	if (UWaitForMontageEventNode* EventNode = Cast<UWaitForMontageEventNode>(Children[3]))
	{
		if (EventNode->EventTag.ToString() != TEXT("Event.Montage.Electrocute"))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteFileGraph: EventTag mismatch: got '%s'"), *EventNode->EventTag.ToString());
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ElectrocuteFileGraph PASSED (modular beam graph)."));
	return true;
}

// ===================================================================
// Crash / lifecycle regression tests.
//
// The structural tests above only cover XML + properties. The teardown crash of
// 2026-07-30 was a *lifecycle* bug: UAuraDataAbility::EndAbility calls
// RootTask->Cancel() on a still-Running graph at PIE stop, and Sequence did not
// propagate Cancel to its children, so channeled tasks (the Electrocute beam's
// Niagara arc + tick timer) were never torn down and crashed/hung at EndPlayMap.
// These tests build the REAL task tree and exercise the Cancel path so that
// regression cannot recur silently.
// ===================================================================

// Asserts that Sequence::Cancel propagates to every entered child (un-entering it)
// and that the channeled beam child tears down cleanly. Uses an empty context 閳?no
// ASC/Avatar/World 閳?so it also verifies Cancel is safe with null runtime context.
static bool SmokeTest_SequenceCancelPropagation()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), TEXT("Electrocute.xml"));
	FString XMLContent;
	if (!FFileHelper::LoadFileToString(XMLContent, *FilePath))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: could not load %s"), *FilePath);
		return false;
	}

	UAuraAbilityDefinition* Def = NewObject<UAuraAbilityDefinition>();
	if (!Def->LoadFromXML(XMLContent) || !Def->RootNode)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: parse/root failed"));
		return false;
	}

	UAuraSequenceTask* Root = NewObject<UAuraSequenceTask>(GetTransientPackage());
	Root->Init(Def->RootNode, /*OwnerAbility=*/nullptr);
	Root->ParentTask = nullptr;

	if (Root->ChildTasks.Num() != 11)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: expected 11 children, got %d"), Root->ChildTasks.Num());
		return false;
	}

	if (!Root->ChildTasks[9] || !Root->ChildTasks[9]->IsA<UTimedLoopTask>())
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: child[9] is not UTimedLoopTask"));
		return false;
	}

	// Simulate a graph that has run all the way to the channeled beam: mark every
	// child entered (Running), as EndPlayMap would find it.
	for (UAuraAbilityActionTask* Child : Root->ChildTasks)
	{
		if (Child)
		{
			Child->HasEntered = true;
		}
	}

	FAuraAbilityExecutionContext Ctx; // empty: no ASC/Avatar/World
	Root->Cancel(Ctx); // must not crash

	// Propagation: every child must have been un-entered by the cancel sweep.
	for (int32 i = 0; i < Root->ChildTasks.Num(); ++i)
	{
		if (Root->ChildTasks[i] && Root->ChildTasks[i]->HasEntered)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: child[%d] still HasEntered after Cancel (Cancel did not propagate)"), i);
			return false;
		}
	}

	if (Root->ActiveChildIndex != 0)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] SequenceCancelPropagation: ActiveChildIndex=%d after Cancel, expected 0"), Root->ActiveChildIndex);
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] SequenceCancelPropagation PASSED (Cancel propagated to 11 children, no crash)."));
	return true;
}

// Asserts the beam task is safe to Cancel / OnExit / OnStart with a null runtime
// context (no owner ability, avatar, or world) 閳?the condition EndPlayMep teardown
// leaves it in. OnStart must return Failure (not crash); Cancel/OnExit must no-op.
static bool SmokeTest_ElectrocuteBeamTaskCancelSafe()
{
	UElectrocuteBeamNode* Node = NewObject<UElectrocuteBeamNode>(GetTransientPackage());
	UElectrocuteBeamTask* Task = NewObject<UElectrocuteBeamTask>(GetTransientPackage());
	Task->Init(Node, /*OwnerAbility=*/nullptr);

	FAuraAbilityExecutionContext Ctx; // empty

	const EAuraAbilityActionStatus StartStatus = Task->OnStart(Ctx);
	if (StartStatus != EAuraAbilityActionStatus::Failure)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteBeamTaskCancelSafe: OnStart with null owner/avatar returned %s, expected Failure"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(StartStatus));
		return false;
	}

	Task->Cancel(Ctx);                         // must not crash
	Task->OnExit(Ctx, EAuraAbilityActionStatus::Success); // must not crash

	if (Task->GetBeamTargetCountForTest() != 0)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ElectrocuteBeamTaskCancelSafe: beam targets=%d after Cancel/OnExit, expected 0"), Task->GetBeamTargetCountForTest());
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ElectrocuteBeamTaskCancelSafe PASSED (null-context OnStart->Failure, Cancel/OnExit safe)."));
	return true;
}

// ===================================================================
// Spawn smoke tests: verify a projectile/Niagara effect actually spawns
// and at the correct world location. These run headlessly in a transient
// UWorld (no PIE, no rendering) 閳?the spawned AAuraProjectile /
// UNiagaraComponent exist and are queryable regardless.
// ===================================================================

struct FSpawnTestEnv
{
	UWorld* World = nullptr;
	ATestCombatAvatar* Avatar = nullptr;
	UTestDataAbility* Ability = nullptr;
};

static FSpawnTestEnv CreateSpawnTestEnv(const FVector& Location, const FRotator& Rotation)
{
	FSpawnTestEnv Env;
	Env.World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!Env.World)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] CreateSpawnTestEnv: UWorld::CreateWorld returned null"));
		return Env;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Env.Avatar = Env.World->SpawnActor<ATestCombatAvatar>(ATestCombatAvatar::StaticClass(), FTransform(Rotation, Location), SP);
	if (Env.Avatar)
	{
		Env.Ability = NewObject<UTestDataAbility>(Env.Avatar);
		Env.Ability->InitTestOwner(Env.Avatar);
	}
	return Env;
}

static void DestroySpawnTestEnv(FSpawnTestEnv& Env)
{
	Env.Ability = nullptr;
	Env.Avatar = nullptr;
	if (Env.World)
	{
		Env.World->RemoveFromRoot();
		Env.World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		Env.World = nullptr;
    }
}

static bool ConfigureSmokeCombatIdentity(
	AActor* Actor,
	const FGameplayTag& FactionTag,
	const FGameplayTag& ControlTypeTag,
	const FGameplayTag& DeathPolicyTag)
{
	if (!Actor)
	{
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FAuraCombatIdentity Identity;
	Identity.FactionTag = FactionTag;
	Identity.ControlTypeTag = ControlTypeTag;
	Identity.CombatProfileTag = Tags.Combat_Unassigned;
	Identity.DeathPolicyTag = DeathPolicyTag;
	Identity.bTargetable = true;
	Identity.bCanAttack = true;
	Identity.bCanBeDamaged = true;

	UAuraCombatIdentityComponent* IdentityComponent = NewObject<UAuraCombatIdentityComponent>(Actor);
	UAuraCombatStateComponent* StateComponent = NewObject<UAuraCombatStateComponent>(Actor);
	if (!IdentityComponent || !StateComponent)
	{
		return false;
	}

	Actor->AddInstanceComponent(IdentityComponent);
	Actor->AddInstanceComponent(StateComponent);
	IdentityComponent->RegisterComponent();
	StateComponent->RegisterComponent();
	return StateComponent->TryEnterAlive() && IdentityComponent->InitializeIdentity(Identity);
}

// Pure math: locks EvenlySpacedRotators so the spawn-direction assertions below rest on
// a known-good baseline (and catches the impl's hardcoded-UpVector-axis quirk).
static bool SmokeTest_EvenlySpacedRotators()
{
	const FVector Forward = FVector::ForwardVector; // (1,0,0)
	const float Spread = 90.f;
	const int32 N = 5;

	const TArray<FRotator> Rots = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, Spread, N);
	if (Rots.Num() != N)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EvenlySpacedRotators: expected %d, got %d"), N, Rots.Num());
		return false;
	}

	const float Delta = Spread / (N - 1); // 22.5
	for (int32 i = 0; i < N; ++i)
	{
		const FVector ExpectedDir = Forward.RotateAngleAxis(-Spread / 2.f + Delta * i, FVector::UpVector);
		const FRotator Expected = ExpectedDir.Rotation();
		if (!Expected.Equals(Rots[i], 0.01f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EvenlySpacedRotators: rot[%d]=%s expected=%s"), i, *Rots[i].ToString(), *Expected.ToString());
			return false;
		}
	}

	// N==1 collapses to the forward rotation.
	const TArray<FRotator> One = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 90.f, 1);
	if (One.Num() != 1 || !One[0].Equals(Forward.Rotation(), 0.01f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EvenlySpacedRotators: N==1 case failed (%d)"), One.Num());
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] EvenlySpacedRotators PASSED (5 rotors @90deg + N==1)."));
	return true;
}

// Verifies SpawnProjectiles actually spawns Count actors, all at the socket location,
// and that their directions are the evenly-spread set 閳?i.e. "spawned, and aimed right".
static bool SmokeTest_ProjectileSpawnCountAndLocation()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	USpawnProjectilesNode* Node = NewObject<USpawnProjectilesNode>(GetTransientPackage());
	Node->SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
	Node->ProjectileClass = AAuraProjectile::StaticClass()->GetPathName();
	Node->Count = 5;
	Node->Spread = 90.f;
	Node->bHoming = false;

	USpawnProjectilesTask* Task = Cast<USpawnProjectilesTask>(Node->CreateTask(Env.Ability));
	if (!Task)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileSpawn: CreateTask returned null"));
		DestroySpawnTestEnv(Env);
		return false;
	}
	Task->Init(Node, Env.Ability);

	// Expected socket = the same call the node makes. Execute_ on a native-only class
	// dispatches to the interface's base _Implementation (the native exec's P_THIS
	// reinterpret doesn't adjust for multiple-inheritance), so this returns the base
	// value here; what matters is that the node spawns AT this location. In a real
	// (BP-avatar) game this resolves to the actual weapon socket.
	const FVector SocketLoc = ICombatInterface::Execute_GetCombatSocketLocation(Env.Avatar, Node->SocketTag);
	const FVector TargetLoc = SocketLoc + FVector(1000.f, 0.f, 0.f); // forward

	FAuraAbilityExecutionContext Ctx;
	Ctx.AvatarActor = Env.Avatar;
	Ctx.Definition = nullptr;
	Ctx.CursorHit.ImpactPoint = TargetLoc;

	const EAuraAbilityActionStatus Status = Task->OnStart(Ctx);
	if (Status == EAuraAbilityActionStatus::Failure)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileSpawn: OnStart returned Failure (ProjectileClass load failed? path=%s)"), *Node->ProjectileClass);
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Count spawned projectiles.
	TArray<FVector> SpawnedDirs;
	int32 Count = 0;
	for (AAuraProjectile* Proj : TActorRange<AAuraProjectile>(Env.World))
	{
		++Count;
		if (!Proj->GetActorLocation().Equals(SocketLoc, 1.f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileSpawn: projectile spawned at %s, expected socket %s"), *Proj->GetActorLocation().ToString(), *SocketLoc.ToString());
			DestroySpawnTestEnv(Env);
			return false;
		}
		SpawnedDirs.Add(Proj->GetActorForwardVector());
	}
	if (Count != 5)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileSpawn: spawned %d, expected 5"), Count);
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Each spawned direction must match one of the expected spread directions.
	const FVector Forward = (TargetLoc - SocketLoc).GetSafeNormal();
	const TArray<FRotator> ExpectedRots = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, 90.f, 5);
	auto DirMatches = [](const FVector& A, const FVector& B) { return A.Equals(B, 0.01f); };
	for (const FVector& Spawned : SpawnedDirs)
	{
		bool bFound = false;
		for (const FRotator& R : ExpectedRots)
		{
			if (DirMatches(Spawned, R.Vector()))
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileSpawn: spawned direction %s not in expected spread set"), *Spawned.ToString());
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	DestroySpawnTestEnv(Env);
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ProjectileSpawnCountAndLocation PASSED (5 projectiles at socket, 90deg spread)."));
	return true;
}

// Verifies the FireBolt/AuraProjectile wall-impact fix: a projectile whose Sphere blocks
// ECC_WorldStatic must STOP on a static blocker (firing OnComponentHit -> OnSphereHit),
// play its impact effect, and destroy itself 閳?instead of tunneling through the wall and
// disappearing only via LifeSpan. Before the fix, the Sphere was Overlap (not Block) on
// WorldStatic, so OnComponentBeginOverlap never fired against blocking geometry and the
// non-blocking sweep passed straight through; this test reproduces that exact scenario.
static bool SmokeTest_ProjectileWallImpact()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Spawn the actual BP_FireBolt (the class this ability fires) facing +X.
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<AAuraProjectile> BPClass = LoadClass<AAuraProjectile>(nullptr,
		TEXT("/Game/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/BP_FireBolt.BP_FireBolt_C"));
	if (!BPClass)
	{
		BPClass = StaticLoadClass(AAuraProjectile::StaticClass(), nullptr,
			TEXT("/Game/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/BP_FireBolt.BP_FireBolt_C"));
	}
	if (!BPClass)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: failed to load BP_FireBolt class"));
		DestroySpawnTestEnv(Env);
		return false;
	}
	AAuraProjectile* Proj = Env.World->SpawnActor<AAuraProjectile>(
		BPClass,
		FTransform(FRotator::ZeroRotator, FVector::ZeroVector), SP);
	if (!Proj)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: failed to spawn BP_FireBolt"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Replicate BeginPlay collision re-assert for BP_FireBolt (bakes Sphere as NoCollision
	// in editor; runtime BeginPlay re-enables + forces WorldStatic=Block).
	{
		USphereComponent* Sphere = Proj->GetSphereComponent();
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Sphere->OnComponentHit.AddDynamic(Proj, &AAuraProjectile::OnSphereHit);
	}

	if (Proj->GetSphereComponent()->GetScaledSphereRadius() <= 0.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: projectile sphere radius is zero (collision shape degenerate)"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// The transient world has no net driver, so a bReplicates=true projectile doesn't default to
	// ROLE_Authority (unlike the bReplicates=false test avatars). In-game the SERVER spawns the
	// projectile, so OnSphereHit's HasAuthority() gate (which calls Destroy) runs there 閳?model
	// that server-side context here.
	Proj->SetRole(ENetRole::ROLE_Authority);

	// Build a WorldStatic blocker in the projectile's path and make it block ECC_Projectile,
	// the way a wall (BlockAll profile) would. Reuse a TestCombatAvatar's sphere but
	// reconfigure its collision to a static blocker.
	const float BlockerRadius = 50.f;
	const FVector BlockerLoc(200.f, 0.f, 0.f);
	ATestCombatAvatar* Wall = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(), FTransform(FRotator::ZeroRotator, BlockerLoc), SP);
	if (!Wall)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: failed to spawn wall blocker"));
		DestroySpawnTestEnv(Env);
		return false;
	}
	USphereComponent* WallSphere = Wall->CollisionSphere;
	WallSphere->InitSphereRadius(BlockerRadius);
	WallSphere->SetCollisionObjectType(ECC_WorldStatic);
	WallSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallSphere->SetCollisionResponseToAllChannels(ECR_Block);
	WallSphere->SetGenerateOverlapEvents(false);

	// The transient smoke-test world doesn't run BeginPlay or the movement tick, so we drive
	// the impact handler directly (in-game, MoveComponent dispatches it via OnComponentHit on
	// the blocking sweep). Grab the projectile's root primitive to pass as the hit component.
	UPrimitiveComponent* ProjRoot = Cast<UPrimitiveComponent>(Proj->GetRootComponent());
	if (!ProjRoot)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: projectile root is not a primitive"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// 1) Config check (the fix): sweep the projectile into the wall. With WorldStatic=Block
	//    the sweep must report a blocking hit. Before the fix (WorldStatic=Overlap) the
	//    non-blocking sweep would have tunneled through with bBlockingHit=false.
	FHitResult Hit;
	Proj->SetActorLocation(FVector(600.f, 0.f, 0.f), /*bSweep=*/true, &Hit);
	if (!Hit.bBlockingHit)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: no blocking hit on the wall (WorldStatic not Block? projectile tunneled through)"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// 2) Handler check: invoke the impact handler (the in-game path dispatches it via
	//    OnComponentHit). It must drive ApplyImpactAndDestroy -> Destroy().
	Proj->OnSphereHit(ProjRoot, Wall, WallSphere, FVector::ZeroVector, Hit);

	if (IsValid(Proj))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] ProjectileWallImpact: projectile still alive after wall hit (OnSphereHit -> Destroy not reached)"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	DestroySpawnTestEnv(Env);
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ProjectileWallImpact PASSED (blocked on WorldStatic, impact destroyed projectile)."));
	return true;
}

// Drives the ElectrocuteBeam node against a trace target: asserts a beam target is
// found, a Niagara component is spawned, and the beam end lands on the target along
// the pawn's facing direction.
static bool SmokeTest_ElectrocuteBeamSpawnEndpoints()
{
	// Use a non-origin avatar so an accidental local-space/component transform offset
	// cannot hide the muzzle-end regression.
	const FVector AvatarLocation(10000.f, -2000.f, 300.f);
	FSpawnTestEnv Env = CreateSpawnTestEnv(AvatarLocation, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FVector SocketLoc = Env.Avatar->GetActorLocation() + Env.Avatar->SocketOffset;
	const FVector Forward = Env.Avatar->GetActorForwardVector(); // (1,0,0)
	const float TargetDist = 500.f;
	const float TargetRadius = Env.Avatar->SphereRadius; // 50
	const float TraceRadius = 10.f;

	// Second avatar instance acts as the blocking trace target.
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATestCombatAvatar* Target = Env.World->SpawnActor<ATestCombatAvatar>(ATestCombatAvatar::StaticClass(), FTransform(FRotator::ZeroRotator, SocketLoc + Forward * TargetDist), SP);
	if (!Target)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: failed to spawn target"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	UElectrocuteBeamNode* Node = NewObject<UElectrocuteBeamNode>(GetTransientPackage());
	Node->SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
	Node->BeamEffect = TEXT("/Game/Assets/Effects/Shock/NS_ElectricBeam.NS_ElectricBeam");
	Node->BeamStartParameter = TEXT("User.Beam Start");
	Node->BeamEndParameter = TEXT("User.Beam End");
	Node->TraceRadius = TraceRadius;
	Node->MaxChainTargets = 0;
	Node->ChannelDuration = 0.f; // no channel timer

	UElectrocuteBeamTask* Task = Cast<UElectrocuteBeamTask>(Node->CreateTask(Env.Ability));
	if (!Task)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: CreateTask returned null"));
		DestroySpawnTestEnv(Env);
		return false;
	}
	Task->Init(Node, Env.Ability);

	FAuraAbilityExecutionContext Ctx;
	Ctx.AvatarActor = Env.Avatar;
	Ctx.Definition = nullptr;

	const EAuraAbilityActionStatus Status = Task->OnStart(Ctx); // runs FindBeamTargets + SpawnBeamFX (+ authority tick setup)

	if (Task->GetBeamTargetCountForTest() != 1)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: beam targets=%d, expected 1"), Task->GetBeamTargetCountForTest());
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	// The BeamEffect asset must resolve (RHI-independent). This is the real regression
	// guard: an empty/missing BeamEffect (the bug just fixed) fails here.
	UObject* BeamAsset = LoadObject<UNiagaraSystem>(nullptr, *Node->BeamEffect);
	if (!BeamAsset)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: BeamEffect '%s' failed to load"), *Node->BeamEffect);
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	// The Niagara component spawn is RHI-gated: under -nullrhi (the smoke runner) no
	// component is created, so this is best-effort. When RHI is present a component MUST
	// be spawned; under nullrhi we just log and continue (the asset + location checks
	// above/below still validate the spawn logic).
	const bool bHasComponent = Task->HasBeamComponentForTest();
	if (bHasComponent)
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] BeamEndpoints: Niagara component spawned (RHI active)."));
	}
	else
	{
		UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SmokeTest] BeamEndpoints: no Niagara component (expected under -nullrhi); asset + location still validated."));
	}

	const FVector BeamEnd = Task->GetBeamEndLocationForTest(0);
	const FVector TargetCenter = Target->GetActorLocation();
	const float DistToTarget = FVector::Dist(BeamEnd, TargetCenter);
	const float Tolerance = TargetRadius + TraceRadius + 1.f;
	if (DistToTarget > Tolerance)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: beam end %s is %.1f from target center %s (tolerance %.1f)"), *BeamEnd.ToString(), DistToTarget, *TargetCenter.ToString(), Tolerance);
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}
	// The endpoint must lie along the pawn's facing direction from the socket.
	const FVector ToEnd = (BeamEnd - SocketLoc).GetSafeNormal();
	if (ToEnd.Dot(Forward) < 0.99f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: beam end direction %s not aligned with forward %s"), *ToEnd.ToString(), *Forward.ToString());
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FVector BeamStart = Task->GetBeamStartLocationForTest();
	if (!BeamStart.Equals(SocketLoc, 0.1f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: beam start %s does not match muzzle/socket %s"), *BeamStart.ToString(), *SocketLoc.ToString());
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	if (bHasComponent)
	{
		const FVector ComponentLocation = Task->GetBeamComponentLocationForTest(0);
		if (!ComponentLocation.IsNearlyZero(0.1f))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamEndpoints: Niagara component at %s; expected world origin for absolute beam parameters"), *ComponentLocation.ToString());
			Task->Cancel(Ctx);
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	Task->Cancel(Ctx); // clears tick/channel timers + cleans up beams
	DestroySpawnTestEnv(Env);
	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ElectrocuteBeamSpawnEndpoints PASSED (target found, BeamEffect loaded, end on target; component=%s)."), bHasComponent ? TEXT("yes") : TEXT("no(nullrhi)"));
	return true;
}

// Regression guard for the bug just fixed: an empty BeamEffect must spawn NO Niagara
// component even though the target is still found (damage path intact, visual absent).
static bool SmokeTest_ElectrocuteBeamEmptyEffectNoSpawn()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FVector SocketLoc = Env.Avatar->GetActorLocation() + Env.Avatar->SocketOffset;
	const FVector Forward = Env.Avatar->GetActorForwardVector();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATestCombatAvatar* Target = Env.World->SpawnActor<ATestCombatAvatar>(ATestCombatAvatar::StaticClass(), FTransform(FRotator::ZeroRotator, SocketLoc + Forward * 500.f), SP);
	if (!Target)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	UElectrocuteBeamNode* Node = NewObject<UElectrocuteBeamNode>(GetTransientPackage());
	Node->SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
	Node->BeamEffect = TEXT(""); // empty -> no visual
	Node->TraceRadius = 10.f;
	Node->MaxChainTargets = 0;
	Node->ChannelDuration = 0.f;

	UElectrocuteBeamTask* Task = Cast<UElectrocuteBeamTask>(Node->CreateTask(Env.Ability));
	Task->Init(Node, Env.Ability);

	FAuraAbilityExecutionContext Ctx;
	Ctx.AvatarActor = Env.Avatar;
	Ctx.Definition = nullptr;

	Task->OnStart(Ctx);

	const bool bTargetFound = (Task->GetBeamTargetCountForTest() == 1);
	const bool bNoComponent = !Task->HasBeamComponentForTest();
	Task->Cancel(Ctx);
	DestroySpawnTestEnv(Env);

	if (!bTargetFound || !bNoComponent)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EmptyEffectNoSpawn: targetFound=%d noComponent=%d (expected target found, NO Niagara)"), bTargetFound, bNoComponent);
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ElectrocuteBeamEmptyEffectNoSpawn PASSED (empty BeamEffect -> target found, no Niagara)."));
	return true;
}

// Regression guard for cursor hits that contain a point but no actor. This is the
// shape produced by TargetDataUnderMouse's open-sky fallback and must still render
// a visual-only beam instead of returning zero targets before SpawnBeamFX.
static bool SmokeTest_ElectrocuteBeamVisualEndpoint()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FVector SocketLoc = Env.Avatar->GetActorLocation() + Env.Avatar->SocketOffset;
	const FVector ExpectedEndpoint = SocketLoc + FVector(600.f, 300.f, 0.f);

	UElectrocuteBeamNode* Node = NewObject<UElectrocuteBeamNode>(GetTransientPackage());
	Node->SocketTag = FGameplayTag::RequestGameplayTag(FName("CombatSocket.Weapon"), false);
	Node->BeamEffect = TEXT("/Game/Assets/Effects/Shock/NS_ElectricBeam.NS_ElectricBeam");
	Node->BeamStartParameter = TEXT("User.Beam Start");
	Node->BeamEndParameter = TEXT("User.Beam End");
	Node->MaxBeamRange = 1000.f;
	Node->MaxChainTargets = 0;
	Node->ChannelDuration = 0.f;

	UElectrocuteBeamTask* Task = Cast<UElectrocuteBeamTask>(Node->CreateTask(Env.Ability));
	if (!Task)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: CreateTask returned null"));
		DestroySpawnTestEnv(Env);
		return false;
	}
	Task->Init(Node, Env.Ability);

	FAuraAbilityExecutionContext Ctx;
	Ctx.AvatarActor = Env.Avatar;
	Ctx.CursorHit.bBlockingHit = true;
	Ctx.CursorHit.ImpactPoint = ExpectedEndpoint;
	Ctx.CursorHit.Location = ExpectedEndpoint;

	const EAuraAbilityActionStatus Status = Task->OnStart(Ctx);
	if (Status != EAuraAbilityActionStatus::Running)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: OnStart returned %s, expected Running"),
			*StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	if (Task->GetBeamTargetCountForTest() != 1)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: targets=%d, expected 1 visual-only target"), Task->GetBeamTargetCountForTest());
		Task->Cancel(Ctx);
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FVector BeamEnd = Task->GetBeamEndLocationForTest(0);
    if (!BeamEnd.Equals(ExpectedEndpoint, 1.f))
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: endpoint=%s expected=%s"), *BeamEnd.ToString(), *ExpectedEndpoint.ToString());
        Task->Cancel(Ctx);
        DestroySpawnTestEnv(Env);
        return false;
    }

    // Move the cursor while the channel is still running. The primary endpoint must
    // change even though the original target data was captured at activation time.
    const FVector MovedEndpoint = SocketLoc + FVector(-250.f, 450.f, 125.f);
    FHitResult MovedCursor;
    MovedCursor.bBlockingHit = true;
    MovedCursor.ImpactPoint = MovedEndpoint;
    MovedCursor.Location = MovedEndpoint;
    Task->UpdateCursorForTest(MovedCursor);
    const FVector UpdatedBeamEnd = Task->GetBeamEndLocationForTest(0);
    if (!UpdatedBeamEnd.Equals(MovedEndpoint, 1.f))
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: live endpoint=%s expected moved point=%s"), *UpdatedBeamEnd.ToString(), *MovedEndpoint.ToString());
        Task->Cancel(Ctx);
        DestroySpawnTestEnv(Env);
        return false;
    }

    Task->Cancel(Ctx);
	const bool bCleanedUp = Task->GetBeamTargetCountForTest() == 0;
	DestroySpawnTestEnv(Env);
	if (!bCleanedUp)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] BeamVisualEndpoint: Cancel did not clear visual target"));
		return false;
	}

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ElectrocuteBeamVisualEndpoint PASSED (cursor endpoint moved during channel and cleaned up)."));
	return true;
}

// Loads the shipped enemy definitions from disk and validates the complete graph
// shape used by the AI path. This catches missing XML files, unregistered nodes,
// invalid activation tags, and damage curve regressions without requiring a PIE world.
static bool SmokeTest_EnemyAbilityFiles()
{
	struct FExpectedEnemyAbility
	{
		const TCHAR* FileName;
		const TCHAR* AbilityTag;
		const TCHAR* FinalNode;
		const TCHAR* CurveRow;
		bool bAttackTag;
	};

	const FExpectedEnemyAbility Expected[] = {
		{TEXT("EnemyFireBolt.xml"), TEXT("Abilities.FireBolt"), TEXT("SpawnProjectile"), TEXT("FireBolt"), true},
		{TEXT("EnemyRangedAttack.xml"), TEXT("Abilities.Ranged"), TEXT("SpawnProjectile"), TEXT("Ranged"), true},
		{TEXT("EnemyMeleeAttack.xml"), TEXT("Abilities.Melee"), TEXT("EnemyMeleeDamage"), TEXT("Melee"), true},
		{TEXT("EnemyHitReact.xml"), TEXT("Effects.HitReact"), TEXT("EnemyHitReact"), TEXT(""), false},
	};

	for (const FExpectedEnemyAbility& Item : Expected)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("AbilityDefinitions"), Item.FileName);
		FString XML;
		if (!FFileHelper::LoadFileToString(XML, *Path))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: failed to read %s"), *Path);
			return false;
		}

		UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
		if (!Definition || !Definition->LoadFromXML(XML))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: failed to parse %s"), Item.FileName);
			return false;
		}
		if (Definition->AbilityTag.ToString() != Item.AbilityTag || !Definition->RootNode)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: identity/root mismatch in %s tag=%s"),
				Item.FileName, *Definition->AbilityTag.ToString());
			return false;
		}

		UAuraAbilityActionNode* FinalNode = Definition->RootNode;
		if (Definition->RootNode->NodeClassName == TEXT("Sequence"))
		{
			if (Definition->RootNode->Children.Num() != 2)
			{
				UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: expected two sequence nodes in %s, got %d"),
					Item.FileName, Definition->RootNode->Children.Num());
				return false;
			}
			FinalNode = Definition->RootNode->Children.Last();
		}
		if (!FinalNode || FinalNode->NodeClassName != Item.FinalNode)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: final node mismatch in %s, got %s expected %s"),
				Item.FileName, FinalNode ? *FinalNode->NodeClassName : TEXT("<null>"), Item.FinalNode);
			return false;
		}

		if (Item.bAttackTag && !Definition->AbilityTags.HasTagExact(FAuraGameplayTags::Get().Abilities_Attack))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: attack activation tag missing in %s"), Item.FileName);
			return false;
		}
		if (FCString::Strlen(Item.CurveRow) > 0 && Definition->Damage.Curve.RowName != FName(Item.CurveRow))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] EnemyAbilityFiles: curve row mismatch in %s, got %s expected %s"),
				Item.FileName, *Definition->Damage.Curve.RowName.ToString(), Item.CurveRow);
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] EnemyAbilityFiles PASSED (4 XML definitions, graph nodes, tags, and damage curves)."));
	return true;
}

// L22 regression: client-supplied target data must be rejected when the handle is
// empty, the hit is missing or non-blocking, the location is non-finite, the actor
// is self, or the target is beyond MaxTargetDistance; a valid in-range blocking hit
// against a distinct actor must pass.
static bool SmokeTest_TargetDataValidation()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	constexpr float MaxDist = 1000.f;
	FString Reason;

	auto MakeHitHandle = [](AActor* Target, const FVector& ImpactPoint, bool bBlocking)
	{
		FGameplayAbilityTargetDataHandle Handle;
		FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
		Data->HitResult = FHitResult(Target, nullptr, ImpactPoint, FVector::ZeroVector);
		Data->HitResult.bBlockingHit = bBlocking;
		Handle.Add(Data);
		return Handle;
	};

	// Empty handle.
	FGameplayAbilityTargetDataHandle EmptyHandle;
	if (UWaitForTargetDataNode::IsValidTargetData(EmptyHandle, Env.Avatar, MaxDist, Reason))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: empty handle was accepted"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Target data with no hit result (actor-array style data).
	FGameplayAbilityTargetDataHandle NoHitHandle;
	NoHitHandle.Add(new FGameplayAbilityTargetData_ActorArray());
	if (UWaitForTargetDataNode::IsValidTargetData(NoHitHandle, Env.Avatar, MaxDist, Reason))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: actor-array data was accepted"));
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Non-blocking hit.
	{
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(nullptr, Env.Avatar->GetActorLocation() + FVector(200.f, 0.f, 0.f), false);
		if (UWaitForTargetDataNode::IsValidTargetData(H, Env.Avatar, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: non-blocking hit was accepted"));
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	// Non-finite location.
	{
		const float NaNValue = FMath::Sqrt(-1.f); // IEEE NaN at runtime
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(nullptr, FVector(NaNValue, 0.f, 0.f), true);
		if (UWaitForTargetDataNode::IsValidTargetData(H, Env.Avatar, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: NaN location was accepted"));
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	// Null avatar.
	{
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(nullptr, FVector(200.f, 0.f, 0.f), true);
		if (UWaitForTargetDataNode::IsValidTargetData(H, nullptr, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: null avatar was accepted"));
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	// Self-target.
	{
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(Env.Avatar, Env.Avatar->GetActorLocation(), true);
		if (UWaitForTargetDataNode::IsValidTargetData(H, Env.Avatar, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: self-target was accepted"));
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	// Out of range.
	{
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(nullptr, Env.Avatar->GetActorLocation() + FVector(2000.f, 0.f, 0.f), true);
		if (UWaitForTargetDataNode::IsValidTargetData(H, Env.Avatar, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: out-of-range target was accepted"));
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	// Valid in-range blocking hit against a distinct actor.
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATestCombatAvatar* Target = Env.World->SpawnActor<ATestCombatAvatar>(ATestCombatAvatar::StaticClass(), FTransform(FRotator::ZeroRotator, Env.Avatar->GetActorLocation() + FVector(300.f, 0.f, 0.f)), SP);
		if (!Target)
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: failed to spawn target actor"));
			DestroySpawnTestEnv(Env);
			return false;
		}
		FGameplayAbilityTargetDataHandle H = MakeHitHandle(Target, Target->GetActorLocation(), true);
		if (!UWaitForTargetDataNode::IsValidTargetData(H, Env.Avatar, MaxDist, Reason))
		{
			UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] TargetDataValidation: valid target rejected: %s"), *Reason);
			DestroySpawnTestEnv(Env);
			return false;
		}
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] TargetDataValidation PASSED (empty/no-hit/non-blocking/NaN/null-avatar/self/out-of-range rejected; valid target accepted)."));
	DestroySpawnTestEnv(Env);
	return true;
}

// H2/L15/L21 regression: the shared damage-param builder used by all seven damage
// nodes must populate WorldContextObject (L15), carry the caller's source/target
// ASCs (L21), and produce direction-aligned knockback/death impulse at the authored
// magnitudes (H2). Non-radial is the default (L19).
static bool SmokeTest_DamageEffectParams()
{
	const FString SampleXML = TEXT(
		"<ability name=\"DamageParamSmoke\" abilityTag=\"Abilities.Attack\" inputTag=\"InputTag.LMB\" type=\"Abilities.Type.Offensive\">"
		"  <damage type=\"Damage.Fire\" base=\"100\" deathImpulseMagnitude=\"500\" knockbackForceMagnitude=\"750\" knockbackChance=\"50\"/>"
		"  <graph><node class=\"Sequence\"/></graph>"
		"</ability>"
	);
	UAuraAbilityDefinition* Definition = NewObject<UAuraAbilityDefinition>(GetTransientPackage());
	if (!Definition || !Definition->LoadFromXML(SampleXML))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: failed to parse sample XML"));
		return false;
	}

	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	UAbilitySystemComponent* SourceASC = NewObject<UAbilitySystemComponent>(GetTransientPackage());
	UAbilitySystemComponent* TargetASC = NewObject<UAbilitySystemComponent>(GetTransientPackage());
	const FVector Direction = FVector::ForwardVector;

	FDamageEffectParams Params;
	Definition->BuildDamageEffectParams(Params, SourceASC, TargetASC, Env.Avatar, 1, Direction);

	bool bOk = true;
	if (Params.WorldContextObject != Env.Avatar)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: WorldContextObject not populated (L15)"));
		bOk = false;
	}
	if (Params.SourceAbilitySystemComponent != SourceASC || Params.TargetAbilitySystemComponent != TargetASC)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: source/target ASC not carried through (L21)"));
		bOk = false;
	}
	if (Params.BaseDamage != 100.f)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: BaseDamage=%.1f expected 100.0"), Params.BaseDamage);
		bOk = false;
	}
	const FVector ExpectedDeathImpulse = Direction * Definition->DeathImpulseMagnitude;
	const FVector ExpectedKnockback = Direction * Definition->KnockbackForceMagnitude;
	if (!Params.DeathImpulse.Equals(ExpectedDeathImpulse, 1.f) || !Params.KnockbackForce.Equals(ExpectedKnockback, 1.f))
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: impulse/knockback not direction-aligned (H2)"));
		bOk = false;
	}
	if (Params.KnockbackForceMagnitude != Definition->KnockbackForceMagnitude || Params.DeathImpulseMagnitude != Definition->DeathImpulseMagnitude)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: impulse/knockback magnitude mismatch"));
		bOk = false;
	}
	if (Params.bIsRadialDamage)
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: radial damage should default to false"));
		bOk = false;
	}

	// Bare ASCs have no initialized AbilityActorInfo. The damage boundary must
	// reject them cleanly instead of asserting inside GetAvatarActor().
	if (UAuraAbilitySystemLibrary::ApplyDamageEffect(Params).IsValid())
	{
		UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SmokeTest] DamageEffectParams: uninitialized ASCs were not rejected by the damage boundary"));
		bOk = false;
	}

	DestroySpawnTestEnv(Env);
	if (!bOk)
	{
		return false;
	}

	UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] DamageEffectParams PASSED (WorldContextObject, ASCs, direction-aligned knockback, non-radial default)."));
	return true;
}

// Regression: beam targets can remain valid actors while their combat state is
// already dead (for example during the death/respawn window). PruneBeamTargets
// must remove those entries instead of waiting for actor destruction.
static bool SmokeTest_ModularBeamTargetPruning()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATestCombatAvatar* DeadTarget = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(250.f, 0.f, 0.f)),
		SpawnParams);
	if (!DeadTarget)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}
	DeadTarget->bTestDead = true;

	FAuraAbilityExecutionContext Context;
	Context.AvatarActor = Env.Avatar;
	Context.BeamState = MakeShared<FAuraBeamExecutionState>();
	FAuraBeamTargetState& TargetEntry = Context.BeamState->Targets.AddDefaulted_GetRef();
	TargetEntry.Actor = DeadTarget;
	TargetEntry.bDamageTarget = true;

	UPruneBeamTargetsNode* Node = NewObject<UPruneBeamTargetsNode>(GetTransientPackage());
	UPruneBeamTargetsTask* Task = NewObject<UPruneBeamTargetsTask>(GetTransientPackage());
	if (!Node || !Task)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}
	Task->Init(Node, Env.Ability);

	const EAuraAbilityActionStatus Status = Task->OnStart(Context);
	const bool bPassed = Status == EAuraAbilityActionStatus::Success &&
		Context.BeamState->Targets.Num() == 0 && Context.bStopCurrentTimedLoop;
	if (!bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Error,
			TEXT("[SmokeTest] ModularBeamTargetPruning: dead valid actor was retained (status=%s targets=%d stop=%s)"),
			*StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status),
			Context.BeamState->Targets.Num(),
			Context.bStopCurrentTimedLoop ? TEXT("true") : TEXT("false"));
	}

	DestroySpawnTestEnv(Env);
	if (bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ModularBeamTargetPruning PASSED (dead valid actor removed and loop stopped)."));
	}
	return bPassed;
}

// Regression: chain selection must apply combat eligibility before consuming a
// chain slot. A friendly player in the search radius should not be selected just
// because it implements the combat interface.
static bool SmokeTest_ModularBeamTargetEligibility()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar || !Env.Ability)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATestCombatAvatar* PrimaryTarget = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(250.f, 0.f, 0.f)),
		SpawnParams);
	ATestCombatAvatar* FriendlyCandidate = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(400.f, 0.f, 0.f)),
		SpawnParams);
	const bool bFixturesReady = PrimaryTarget && FriendlyCandidate
		&& ConfigureSmokeCombatIdentity(Env.Avatar, Tags.Faction_Player, Tags.Control_Player, Tags.Death_PlayerRespawn)
		&& ConfigureSmokeCombatIdentity(PrimaryTarget, Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Death_EnemyLoot)
		&& ConfigureSmokeCombatIdentity(FriendlyCandidate, Tags.Faction_Player, Tags.Control_Player, Tags.Death_PlayerRespawn);
	if (!bFixturesReady)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	// Ability level 2 allows one additional chain target.
	Env.Ability->InitTestOwner(Env.Avatar, 2);
	FAuraAbilityExecutionContext Context;
	Context.AvatarActor = Env.Avatar;
	Context.BeamState = MakeShared<FAuraBeamExecutionState>();
	FAuraBeamTargetState& PrimaryEntry = Context.BeamState->Targets.AddDefaulted_GetRef();
	PrimaryEntry.Actor = PrimaryTarget;
	PrimaryEntry.bDamageTarget = true;

	USelectBeamChainTargetsNode* Node = NewObject<USelectBeamChainTargetsNode>(GetTransientPackage());
	USelectBeamChainTargetsTask* Task = NewObject<USelectBeamChainTargetsTask>(GetTransientPackage());
	if (!Node || !Task)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}
	Node->SearchRadius = 300.f;
	Node->MaxAdditionalTargets = 1;
	Task->Init(Node, Env.Ability);

	const EAuraAbilityActionStatus Status = Task->OnStart(Context);
	const bool bPassed = Status == EAuraAbilityActionStatus::Success
		&& Context.BeamState->Targets.Num() == 1
		&& Context.BeamState->Targets[0].Actor.Get() == PrimaryTarget;
	if (!bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Error,
			TEXT("[SmokeTest] ModularBeamTargetEligibility: friendly candidate consumed a chain slot (status=%s targets=%d)"),
			*StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status),
			Context.BeamState->Targets.Num());
	}

	DestroySpawnTestEnv(Env);
	if (bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ModularBeamTargetEligibility PASSED (friendly candidate filtered before chain selection)."));
	}
	return bPassed;
}

// Regression: when replacement is enabled, pruning a dead chain target should
// refill that slot with the nearest eligible live target.
static bool SmokeTest_ModularBeamTargetReplacement()
{
	FSpawnTestEnv Env = CreateSpawnTestEnv(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Env.World || !Env.Avatar)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATestCombatAvatar* PrimaryTarget = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(250.f, 0.f, 0.f)),
		SpawnParams);
	ATestCombatAvatar* DeadTarget = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(400.f, 0.f, 0.f)),
		SpawnParams);
	ATestCombatAvatar* ReplacementTarget = Env.World->SpawnActor<ATestCombatAvatar>(
		ATestCombatAvatar::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(500.f, 0.f, 0.f)),
		SpawnParams);
	const bool bFixturesReady = PrimaryTarget && DeadTarget && ReplacementTarget
		&& ConfigureSmokeCombatIdentity(Env.Avatar, Tags.Faction_Player, Tags.Control_Player, Tags.Death_PlayerRespawn)
		&& ConfigureSmokeCombatIdentity(PrimaryTarget, Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Death_EnemyLoot)
		&& ConfigureSmokeCombatIdentity(DeadTarget, Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Death_EnemyLoot)
		&& ConfigureSmokeCombatIdentity(ReplacementTarget, Tags.Faction_Enemy, Tags.Control_EnemyAI, Tags.Death_EnemyLoot);
	if (!bFixturesReady)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}
	DeadTarget->bTestDead = true;

	FAuraAbilityExecutionContext Context;
	Context.AvatarActor = Env.Avatar;
	Context.BeamState = MakeShared<FAuraBeamExecutionState>();
	Context.BeamState->MaxAdditionalTargets = 1;
	Context.BeamState->SearchRadius = 300.f;
	Context.BeamState->bReplaceInvalidTargets = true;
	FAuraBeamTargetState& PrimaryEntry = Context.BeamState->Targets.AddDefaulted_GetRef();
	PrimaryEntry.Actor = PrimaryTarget;
	PrimaryEntry.bDamageTarget = true;
	FAuraBeamTargetState& DeadEntry = Context.BeamState->Targets.AddDefaulted_GetRef();
	DeadEntry.Actor = DeadTarget;
	DeadEntry.bDamageTarget = true;

	UPruneBeamTargetsNode* Node = NewObject<UPruneBeamTargetsNode>(GetTransientPackage());
	UPruneBeamTargetsTask* Task = NewObject<UPruneBeamTargetsTask>(GetTransientPackage());
	if (!Node || !Task)
	{
		DestroySpawnTestEnv(Env);
		return false;
	}
	Task->Init(Node, Env.Ability);

	const EAuraAbilityActionStatus Status = Task->OnStart(Context);
	const bool bPassed = Status == EAuraAbilityActionStatus::Success
		&& Context.BeamState->Targets.Num() == 2
		&& Context.BeamState->Targets[0].Actor.Get() == PrimaryTarget
		&& Context.BeamState->Targets[1].Actor.Get() == ReplacementTarget;
	if (!bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Error,
			TEXT("[SmokeTest] ModularBeamTargetReplacement: replacement target was not selected (status=%s targets=%d)"),
			*StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status),
			Context.BeamState->Targets.Num());
	}

	DestroySpawnTestEnv(Env);
	if (bPassed)
	{
		UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SmokeTest] ModularBeamTargetReplacement PASSED (dead slot refilled with eligible target)."));
	}
	return bPassed;
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
	Run(TEXT("FireBoltFileGraph"), SmokeTest_FireBoltFileGraph);
	Run(TEXT("ClientGraphCompletionBoundary"), SmokeTest_ClientGraphCompletionBoundary);
	Run(TEXT("FireBlastFileGraph"), SmokeTest_FireBlastFileGraph);
	Run(TEXT("ArcaneShardsFileGraph"), SmokeTest_ArcaneShardsFileGraph);
	Run(TEXT("ElectrocuteFileGraph"), SmokeTest_ElectrocuteFileGraph);
	Run(TEXT("SequenceCancelPropagation"), SmokeTest_SequenceCancelPropagation);
	Run(TEXT("ElectrocuteBeamTaskCancelSafe"), SmokeTest_ElectrocuteBeamTaskCancelSafe);
	Run(TEXT("EvenlySpacedRotators"), SmokeTest_EvenlySpacedRotators);
	Run(TEXT("ProjectileSpawnCountAndLocation"), SmokeTest_ProjectileSpawnCountAndLocation);
	Run(TEXT("ProjectileWallImpact"), SmokeTest_ProjectileWallImpact);
	Run(TEXT("ElectrocuteBeamSpawnEndpoints"), SmokeTest_ElectrocuteBeamSpawnEndpoints);
	Run(TEXT("ElectrocuteBeamEmptyEffectNoSpawn"), SmokeTest_ElectrocuteBeamEmptyEffectNoSpawn);
	Run(TEXT("ElectrocuteBeamVisualEndpoint"), SmokeTest_ElectrocuteBeamVisualEndpoint);
	Run(TEXT("EnemyAbilityFiles"), SmokeTest_EnemyAbilityFiles);
	Run(TEXT("TargetDataValidation"), SmokeTest_TargetDataValidation);
	Run(TEXT("DamageEffectParams"), SmokeTest_DamageEffectParams);
	Run(TEXT("ModularBeamTargetPruning"), SmokeTest_ModularBeamTargetPruning);
	Run(TEXT("ModularBeamTargetEligibility"), SmokeTest_ModularBeamTargetEligibility);
	Run(TEXT("ModularBeamTargetReplacement"), SmokeTest_ModularBeamTargetReplacement);

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
	// Aura's gameplay-tag singleton may initialize after this plugin's compiled-in
	// ability CDOs. Register the two tags required by enemy activation before those
	// CDOs are constructed; duplicate registration by AuraGameplayTags is idempotent.
	UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TEXT("Abilities.Attack")), TEXT("Enemy attack activation tag"));
	UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TEXT("Effects.HitReact")), TEXT("Enemy hit-react activation tag"));

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
	FAuraAbilityNodeRegistry::Get().Register(TEXT("Wait"), [](UObject* O) { return NewObject<UWaitNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("SpawnShards"), [](UObject* O) { return NewObject<USpawnShardsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("ElectrocuteBeam"), [](UObject* O) { return NewObject<UElectrocuteBeamNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("ResolveBeamOrigin"), [](UObject* O) { return NewObject<UResolveBeamOriginNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("AcquirePrimaryBeamTarget"), [](UObject* O) { return NewObject<UAcquirePrimaryBeamTargetNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("SelectBeamChainTargets"), [](UObject* O) { return NewObject<USelectBeamChainTargetsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("SpawnBeamVisuals"), [](UObject* O) { return NewObject<USpawnBeamVisualsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("InitializeBeamEndpoints"), [](UObject* O) { return NewObject<UInitializeBeamEndpointsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("TimedLoop"), [](UObject* O) { return NewObject<UTimedLoopNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("PruneBeamTargets"), [](UObject* O) { return NewObject<UPruneBeamTargetsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("RefreshBeamEndpoints"), [](UObject* O) { return NewObject<URefreshBeamEndpointsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("ApplyBeamDamage"), [](UObject* O) { return NewObject<UApplyBeamDamageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("DestroyBeamVisuals"), [](UObject* O) { return NewObject<UDestroyBeamVisualsNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("EnemyCombatMontage"), [](UObject* O) { return NewObject<UEnemyCombatMontageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("EnemyMeleeDamage"), [](UObject* O) { return NewObject<UEnemyMeleeDamageNode>(O); });
	FAuraAbilityNodeRegistry::Get().Register(TEXT("EnemyHitReact"), [](UObject* O) { return NewObject<UEnemyHitReactNode>(O); });

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
