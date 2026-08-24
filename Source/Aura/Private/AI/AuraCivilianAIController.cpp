// Copyright Druid Mechanics

#include "AI/AuraCivilianAIController.h"

#include "AI/BTTask_CivilianMoveTo.h"
#include "AI/BTTask_FindCivilianDestination.h"
#include "AI/BTService_FindNearestThreat.h"
#include "AI/BTService_UpdateCivilianContext.h"
#include "AI/BTDecorator_CivilianAlive.h"
#include "Aura/AuraLogChannels.h"
#include "Character/AuraCivilian.h"
#include "Combat/AuraCombatStateComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "GameFramework/Pawn.h"
#include "Game/AuraGameModeBase.h"
#include "Battle/AuraBattleDirector.h"
#include "World/AuraPopulationManager.h"

AAuraCivilianAIController::AAuraCivilianAIController()
{
}

void AAuraCivilianAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogAura, Log, TEXT("[CivilianAI][Controller] Possessed controller=%s pawn=%s authority=%s."),
		*GetNameSafe(this), *GetNameSafe(InPawn), HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AAuraCivilianAIController::OnUnPossess()
{
	StopCivilianBehavior();
	UE_LOG(LogAura, Log, TEXT("[CivilianAI][Controller] Unpossessed controller=%s pawn=%s."),
		*GetNameSafe(this), *GetNameSafe(GetPawn()));
	Super::OnUnPossess();
}

void AAuraCivilianAIController::BuildCivilianBehaviorTree()
{
	if (CivilianBehaviorTree && CivilianBlackboard)
	{
		return;
	}

	CivilianBehaviorTree = NewObject<UBehaviorTree>(this, TEXT("BT_Civilian_Runtime"));
	CivilianBlackboard = NewObject<UBlackboardData>(CivilianBehaviorTree, TEXT("BB_Civilian_Runtime"));
	if (!CivilianBehaviorTree || !CivilianBlackboard)
	{
		return;
	}

	const auto AddKey = [this](FName KeyName, UBlackboardKeyType* KeyType)
	{
		FBlackboardEntry Entry;
		Entry.EntryName = KeyName;
		Entry.KeyType = KeyType;
		CivilianBlackboard->Keys.Add(Entry);
	};

	AddKey(TEXT("ThreatActor"), NewObject<UBlackboardKeyType_Object>(CivilianBlackboard, TEXT("ThreatActorType")));
	if (UBlackboardKeyType_Object* ThreatType = Cast<UBlackboardKeyType_Object>(CivilianBlackboard->Keys.Last().KeyType))
	{
		ThreatType->BaseClass = AActor::StaticClass();
	}
	UBlackboardKeyType_Float* ThreatDistanceType = NewObject<UBlackboardKeyType_Float>(CivilianBlackboard, TEXT("ThreatDistanceType"));
	ThreatDistanceType->DefaultValue = TNumericLimits<float>::Max();
	AddKey(TEXT("ThreatDistance"), ThreatDistanceType);
	AddKey(TEXT("Destination"), NewObject<UBlackboardKeyType_Vector>(CivilianBlackboard, TEXT("DestinationType")));
	UBlackboardKeyType_Enum* ActivityType = NewObject<UBlackboardKeyType_Enum>(CivilianBlackboard, TEXT("ActivityType"));
	ActivityType->EnumType = StaticEnum<EAuraCivilianActivity>();
	ActivityType->DefaultValue = static_cast<uint8>(EAuraCivilianActivity::Idle);
	AddKey(TEXT("Activity"), ActivityType);
	AddKey(TEXT("bHasSafeDestination"), NewObject<UBlackboardKeyType_Bool>(CivilianBlackboard, TEXT("SafeDestinationType")));
	AddKey(TEXT("HomeLocation"), NewObject<UBlackboardKeyType_Vector>(CivilianBlackboard, TEXT("HomeLocationType")));
	for (const FName MarkerKey : { FName(TEXT("WorkMarker")), FName(TEXT("ObservationMarker")), FName(TEXT("ShelterMarker")) })
	{
		UBlackboardKeyType_Object* MarkerType = NewObject<UBlackboardKeyType_Object>(CivilianBlackboard, *FString::Printf(TEXT("%sType"), *MarkerKey.ToString()));
		MarkerType->BaseClass = AActor::StaticClass();
		AddKey(MarkerKey, MarkerType);
	}
	UBlackboardKeyType_Float* LastFailureType = NewObject<UBlackboardKeyType_Float>(CivilianBlackboard, TEXT("LastMoveFailureTimeType"));
	LastFailureType->DefaultValue = 0.f;
	AddKey(TEXT("LastMoveFailureTime"), LastFailureType);

	CivilianBehaviorTree->BlackboardAsset = CivilianBlackboard;
	UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(CivilianBehaviorTree, TEXT("CivilianSchedule"));
	UBTService_FindNearestThreat* FindThreat = NewObject<UBTService_FindNearestThreat>(Root, TEXT("FindNearestThreat"));
	UBTService_UpdateCivilianContext* UpdateContext = NewObject<UBTService_UpdateCivilianContext>(Root, TEXT("UpdateCivilianContext"));
	Root->Services.Add(FindThreat);
	Root->Services.Add(UpdateContext);
	UBTTask_FindCivilianDestination* FindDestination = NewObject<UBTTask_FindCivilianDestination>(Root, TEXT("FindDestination"));
	UBTTask_CivilianMoveTo* MoveTo = NewObject<UBTTask_CivilianMoveTo>(Root, TEXT("MoveToDestination"));
	UBTTask_Wait* Wait = NewObject<UBTTask_Wait>(Root, TEXT("WaitAtDestination"));
	Wait->WaitTime = FValueOrBBKey_Float(1.5f);
	Wait->RandomDeviation = FValueOrBBKey_Float(0.5f);
	FBTCompositeChild FindDestinationChild;
	FindDestinationChild.ChildTask = FindDestination;
	FindDestinationChild.Decorators.Add(NewObject<UBTDecorator_CivilianAlive>(Root, TEXT("CivilianAliveDecorator")));
	FindDestinationChild.DecoratorOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 0));
	Root->Children.Add(FindDestinationChild);
	Root->Children.Add({ nullptr, MoveTo, {}, {} });
	Root->Children.Add({ nullptr, Wait, {}, {} });
	CivilianBehaviorTree->RootNode = Root;
}

FAuraCombatRuleContext AAuraCivilianAIController::BuildThreatRuleContext(const AActor* Candidate, const AActor* Civilian) const
{
	FAuraCombatRuleContext Context;
	Context.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	Context.TrustedWorldContext = this;
	Context.SourceActor = Candidate;
	Context.TargetActor = Civilian;
	Context.ImpactLocation = Civilian ? Civilian->GetActorLocation() : FVector::ZeroVector;
	if (const AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
	{
		if (const AAuraBattleDirector* BattleDirector = GameMode->GetBattleDirector())
		{
			FAuraCombatRuleContext AuthoritativeContext;
			if (BattleDirector->ResolveCombatRuleContext(Candidate, Civilian, AuthoritativeContext))
			{
				return AuthoritativeContext;
			}
		}
	}
	return Context;
}

void AAuraCivilianAIController::StartCivilianBehavior()
{
	if (!HasAuthority() || bCivilianBehaviorStarted)
	{
		return;
	}

	AAuraCivilian* Civilian = Cast<AAuraCivilian>(GetPawn());
	if (!Civilian || !Civilian->IsCombatAlive())
	{
		UE_LOG(LogAura, Warning, TEXT("[CivilianAI] Refused to start for %s: pawn is missing or not Alive."),
			*GetNameSafe(Civilian));
		return;
	}

	BuildCivilianBehaviorTree();
	UBlackboardComponent* BlackboardComponent = Blackboard;
	if (!CivilianBehaviorTree || !CivilianBlackboard || !UseBlackboard(CivilianBlackboard, BlackboardComponent) || !RunBehaviorTree(CivilianBehaviorTree))
	{
		UE_LOG(LogAura, Error, TEXT("[CivilianAI] Failed to start BT for %s."), *GetNameSafe(Civilian));
		return;
	}

	Blackboard = BlackboardComponent;
	Blackboard->SetValueAsVector(TEXT("HomeLocation"), Civilian->GetActorLocation());
	Blackboard->SetValueAsEnum(TEXT("Activity"), static_cast<uint8>(EAuraCivilianActivity::Idle));
	Blackboard->SetValueAsBool(TEXT("bHasSafeDestination"), false);
	Civilian->GetCombatStateComponentMutable()->OnLifeStateChanged.AddUObject(this, &AAuraCivilianAIController::HandleLifeStateChanged);
	bCivilianBehaviorStarted = true;
	UE_LOG(LogAura, Display, TEXT("[CivilianAI] BT started pawn=%s controller=%s tree=BT_Civilian_Runtime blackboard=BB_Civilian_Runtime."),
		*GetNameSafe(Civilian), *GetNameSafe(this));
}

void AAuraCivilianAIController::StopCivilianBehavior()
{
	if (AAuraCivilian* Civilian = Cast<AAuraCivilian>(GetPawn()))
	{
		if (UAuraCombatStateComponent* State = Civilian->GetCombatStateComponentMutable())
		{
			State->OnLifeStateChanged.RemoveAll(this);
		}
		if (AAuraGameModeBase* GameMode = Civilian->GetWorld() ? Civilian->GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
		{
			if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable())
			{
				Manager->ReleaseAllActivityMarkerReservations(Civilian->GetPopulationMemberState().PopulationMemberId);
			}
		}
		Civilian->SetCivilianActivity(EAuraCivilianActivity::Idle);
	}

	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("Civilian AI stopped"));
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	bCivilianBehaviorStarted = false;
}

void AAuraCivilianAIController::HandleLifeStateChanged(EAuraCombatLifeState NewState)
{
	if (NewState != EAuraCombatLifeState::Alive)
	{
		UE_LOG(LogAura, Display, TEXT("[CivilianAI] stopping pawn=%s lifeState=%d."),
			*GetNameSafe(GetPawn()), static_cast<int32>(NewState));
		StopCivilianBehavior();
	}
}
