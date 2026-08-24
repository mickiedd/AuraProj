// Copyright Druid Mechanics

#include "Battle/AuraBattleDirector.h"

#include "Aura/AuraLogChannels.h"
#include "Battle/AuraBattleZoneConfig.h"
#include "Combat/AuraDeathPolicyDispatcher.h"
#include "Game/AuraGameModeBase.h"
#include "Net/UnrealNetwork.h"

AAuraBattleDirector::AAuraBattleDirector()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = true;
	SetNetUpdateFrequency(2.f);
	PrimaryActorTick.bCanEverTick = false;
}

void AAuraBattleDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		UE_LOG(LogAura, Display, TEXT("[BattleDirector][Client] Replicated director observed phase=%d event=%s."),
			static_cast<int32>(CurrentPhase), *ActiveBattleEventId.ToString());
		return;
	}
	ZoneConfig = NewObject<UAuraBattleZoneConfig>(this, TEXT("BattleZoneConfig"));
	FString Error;
	if (!ZoneConfig || !ZoneConfig->Load(Error))
	{
		UE_LOG(LogAura, Error, TEXT("[BattleDirector] Battle zone config rejected: %s"), *Error);
		ZoneConfig = nullptr;
		return;
	}
	ConfigVersion = ZoneConfig->GetSchemaVersion();
	ConfigHash = ZoneConfig->GetConfigHash();
	if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
	{
		if (UAuraDeathPolicyDispatcher* Dispatcher = GameMode->GetDeathPolicyDispatcherMutable())
		{
			Dispatcher->OnAuthoritativeDeath.AddUObject(this, &AAuraBattleDirector::HandleAuthoritativeDeath);
		}
	}
	ForceNetUpdate();
	UE_LOG(LogAura, Display, TEXT("[BattleDirector] Ready phase=Peace configVersion=%d hash=%s zones=%d."), ConfigVersion, *ConfigHash, ZoneConfig->GetZones().Num());
}

void AAuraBattleDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraBattleDirector, CurrentPhase);
	DOREPLIFETIME(AAuraBattleDirector, ActiveBattleEventId);
	DOREPLIFETIME(AAuraBattleDirector, ConfigVersion);
	DOREPLIFETIME(AAuraBattleDirector, ConfigHash);
}

bool AAuraBattleDirector::TransitionTo(EAuraBattlePhase NewPhase)
{
	if (!HasAuthority() || NewPhase == CurrentPhase) return false;
	const EAuraBattlePhase PreviousPhase = CurrentPhase;
	const bool bValidTransition =
		(PreviousPhase == EAuraBattlePhase::Peace && (NewPhase == EAuraBattlePhase::Alert || NewPhase == EAuraBattlePhase::Conflict))
		|| (PreviousPhase == EAuraBattlePhase::Alert && (NewPhase == EAuraBattlePhase::Conflict || NewPhase == EAuraBattlePhase::Cleanup))
		|| (PreviousPhase == EAuraBattlePhase::Conflict && NewPhase == EAuraBattlePhase::Cleanup)
		|| (PreviousPhase == EAuraBattlePhase::Cleanup && NewPhase == EAuraBattlePhase::Peace);
	if (!bValidTransition) return false;
	const bool bLeavingPeace = PreviousPhase == EAuraBattlePhase::Peace
		&& (NewPhase == EAuraBattlePhase::Alert || NewPhase == EAuraBattlePhase::Conflict);
	const bool bReturningToPeace = PreviousPhase == EAuraBattlePhase::Cleanup && NewPhase == EAuraBattlePhase::Peace;
	CurrentPhase = NewPhase;
	const UEnum* PhaseEnum = StaticEnum<EAuraBattlePhase>();
	const FString PreviousName = PhaseEnum ? PhaseEnum->GetNameStringByValue(static_cast<int64>(PreviousPhase)) : TEXT("Unknown");
	const FString NewName = PhaseEnum ? PhaseEnum->GetNameStringByValue(static_cast<int64>(NewPhase)) : TEXT("Unknown");
	if (bLeavingPeace)
	{
		++BattleEventSequence;
		ActiveBattleEventId = FName(*FString::Printf(TEXT("%sTo%s_%d"), *PreviousName, *NewName, BattleEventSequence));
	}
	else if (bReturningToPeace)
	{
		ActiveBattleEventId = NAME_None;
	}
	ForceNetUpdate();
	OnPhaseChanged.Broadcast(CurrentPhase);
	UE_LOG(LogAura, Display, TEXT("[BattleDirector] Phase transition %s -> %s event=%s."), *PreviousName, *NewName, *ActiveBattleEventId.ToString());
	return true;
}

bool AAuraBattleDirector::ResolveCombatRuleContext(const AActor* SourceActor, const AActor* TargetActor, FAuraCombatRuleContext& OutContext) const
{
	if (!HasAuthority() || !ZoneConfig || !IsValid(SourceActor) || !IsValid(TargetActor)) return false;
	OutContext = FAuraCombatRuleContext();
	OutContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	OutContext.TrustedWorldContext = this;
	OutContext.SourceActor = SourceActor;
	OutContext.TargetActor = TargetActor;
	OutContext.ImpactLocation = TargetActor->GetActorLocation();
	const FAuraCombatPolicySnapshot Snapshot = ZoneConfig->BuildPolicySnapshot(this, OutContext.ImpactLocation, CurrentPhase, ActiveBattleEventId);
	if (!Snapshot.IsValid()) return false;
	OutContext.BattleZoneId = Snapshot.GetBattleZoneId();
	OutContext.BattleEventId = Snapshot.GetBattleEventId();
	OutContext.SetPolicySnapshot(Snapshot);
	return true;
}

void AAuraBattleDirector::HandleAuthoritativeDeath(const FAuraDeathEvent& Event)
{
	UE_LOG(LogAura, Verbose, TEXT("[BattleDirector] Observed neutral death event victim=%s policy=%s phase=%d."),
		*GetNameSafe(Event.VictimActor), *Event.DeathPolicyTag.ToString(), static_cast<int32>(CurrentPhase));
}

void AAuraBattleDirector::OnRep_CurrentPhase()
{
	OnPhaseChanged.Broadcast(CurrentPhase);
}

void AAuraBattleDirector::OnRep_ActiveBattleEventId()
{
	UE_LOG(LogAura, Verbose, TEXT("[BattleDirector] Replicated event=%s phase=%d."), *ActiveBattleEventId.ToString(), static_cast<int32>(CurrentPhase));
}
