// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Composites/BehaviorUComposites.h"
#include "BehaviorTree/Decorators/BehaviorUDecorators.h"
#include "BehaviorUAgent.h"
#include "BehaviorTree/BehaviorUBehaviorTree.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ===================================================================
// SELECTOR
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSelector::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSelectorTask>(Outer);
}

void UBehaviorUSelector::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
}

bool UBehaviorUSelector::Evaluate(UBehaviorUAgentComponent* Agent) const
{
	for (UBehaviorUBehaviorNode* Child : Children)
	{
		const UBehaviorUSelector* ChildSelector = Cast<UBehaviorUSelector>(Child);
		if (ChildSelector && ChildSelector->Evaluate(Agent))
		{
			return true;
		}
	}
	return false;
}

bool UBehaviorUSelector::CheckIfInterrupted(UBehaviorUAgentComponent* Agent) const
{
	return false;
}

bool UBehaviorUSelectorTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

void UBehaviorUSelectorTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
}

EBehaviorUStatus UBehaviorUSelectorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// If the current child succeeded or is still running, propagate that.
	// Do NOT fall through to the while-loop when the child is Running — that
	// would re-execute the same child a second time in the same tick.
	if (ChildStatus == EBehaviorUStatus::Success)
	{
		return EBehaviorUStatus::Success;
	}
	if (ChildStatus == EBehaviorUStatus::Running)
	{
		// Continue ticking the currently active child.
		if (ChildTasks.IsValidIndex(ActiveChildIndex))
		{
			return ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviorUStatus::Running);
		}
		return EBehaviorUStatus::Running;
	}

	// Current child failed, try next
	if (ChildStatus == EBehaviorUStatus::Failure)
	{
		ActiveChildIndex++;
	}

	// Try children from current index
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviorUStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviorUStatus::Invalid);

		if (Result == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}

		if (Result == EBehaviorUStatus::Success)
		{
			return EBehaviorUStatus::Success;
		}

		// Failure: move to next child
		ActiveChildIndex++;
	}

	// All children failed
	return EBehaviorUStatus::Failure;
}

// ===================================================================
// SEQUENCE
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSequence::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSequenceTask>(Outer);
}

void UBehaviorUSequence::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
}

bool UBehaviorUSequenceTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

void UBehaviorUSequenceTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
}

EBehaviorUStatus UBehaviorUSequenceTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// If current child is running, continue ticking it with Running status.
	// Previously this passed Invalid, which confused composite children that
	// rely on ChildStatus to distinguish first-entry from continuation.
	if (ChildStatus == EBehaviorUStatus::Running)
	{
		if (ChildTasks.IsValidIndex(ActiveChildIndex))
		{
			return ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviorUStatus::Running);
		}
		// ActiveChildIndex out of range — treat as completion.
		return EBehaviorUStatus::Success;
	}

	// If current child failed, propagate failure
	if (ChildStatus == EBehaviorUStatus::Failure)
	{
		return EBehaviorUStatus::Failure;
	}

	// Current child succeeded, move to next
	if (ChildStatus == EBehaviorUStatus::Success)
	{
		ActiveChildIndex++;
	}

	// Try children from current index
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviorUStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviorUStatus::Invalid);

		if (Result == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}

		if (Result == EBehaviorUStatus::Failure)
		{
			return EBehaviorUStatus::Failure;
		}

		// Success: move to next child
		ActiveChildIndex++;
	}

	// All children succeeded
	return EBehaviorUStatus::Success;
}

// ===================================================================
// PARALLEL
// ===================================================================

UBehaviorUParallel::UBehaviorUParallel()
	: FailurePolicy(EBehaviorUParallelPolicy::RequireAll)
	, SuccessPolicy(EBehaviorUParallelPolicy::RequireAll)
	, ChildFinishPolicy(EBehaviorUChildFinishPolicy::Once)
{
}

UBehaviorUBehaviorTask* UBehaviorUParallel::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUParallelTask>(Outer);
}

void UBehaviorUParallel::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("FailurePolicy"))
		{
			// 支持旧枚举值和新枚举值
			if (Prop.Value == TEXT("FAIL_ON_ONE") || Prop.Value == TEXT("FailOnOne_SucceedOnAll") || Prop.Value == TEXT("RequireAll"))
				FailurePolicy = EBehaviorUParallelPolicy::RequireAll;
			else if (Prop.Value == TEXT("FAIL_ON_ALL") || Prop.Value == TEXT("FailOnAll_SucceedOnOne") || Prop.Value == TEXT("RequireOne"))
				FailurePolicy = EBehaviorUParallelPolicy::RequireOne;
			else if (Prop.Value == TEXT("FailOnOne_SucceedOnOne") || Prop.Value == TEXT("FirstCompletes"))
				FailurePolicy = EBehaviorUParallelPolicy::FirstCompletes;
		}
		else if (Prop.Name == TEXT("SuccessPolicy"))
		{
			// 支持旧枚举值和新枚举值
			if (Prop.Value == TEXT("FAIL_ON_ONE") || Prop.Value == TEXT("FailOnOne_SucceedOnAll") || Prop.Value == TEXT("RequireAll"))
				SuccessPolicy = EBehaviorUParallelPolicy::RequireAll;
			else if (Prop.Value == TEXT("FAIL_ON_ALL") || Prop.Value == TEXT("FailOnAll_SucceedOnOne") || Prop.Value == TEXT("RequireOne"))
				SuccessPolicy = EBehaviorUParallelPolicy::RequireOne;
			else if (Prop.Value == TEXT("FailOnOne_SucceedOnOne") || Prop.Value == TEXT("FirstCompletes"))
				SuccessPolicy = EBehaviorUParallelPolicy::FirstCompletes;
		}
		else if (Prop.Name == TEXT("ChildFinishPolicy"))
		{
			if (Prop.Value == TEXT("CHILDFINISH_LOOP") || Prop.Value == TEXT("Loop"))
				ChildFinishPolicy = EBehaviorUChildFinishPolicy::Loop;
			else
			{
				ChildFinishPolicy = EBehaviorUChildFinishPolicy::Once;
				BEHAVIORU_VLOG(TEXT("[Parallel] ChildFinishPolicy='%s' → Once (not Loop). UpdateAIState will only run once!"), *Prop.Value);
			}
		}
	}

	UE_LOG(LogBehaviorU, Log,
		TEXT("[Parallel] Loaded — FailurePolicy=%d SuccessPolicy=%d ChildFinishPolicy=%d"),
		(int32)FailurePolicy, (int32)SuccessPolicy, (int32)ChildFinishPolicy);
}

UBehaviorUParallelTask::UBehaviorUParallelTask()
{
}

void UBehaviorUParallelTask::Init(UBehaviorUBehaviorNode* InNode)
{
	Super::Init(InNode);
	ChildStatuses.SetNum(ChildTasks.Num());
	for (int32 i = 0; i < ChildStatuses.Num(); i++)
	{
		ChildStatuses[i] = EBehaviorUStatus::Invalid;
	}
}

void UBehaviorUParallelTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Super::Reset(Agent);
	for (int32 i = 0; i < ChildStatuses.Num(); i++)
	{
		ChildStatuses[i] = EBehaviorUStatus::Invalid;
	}
}

bool UBehaviorUParallelTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUParallel* ParallelNode = Cast<UBehaviorUParallel>(Node);
	UE_LOG(LogBehaviorU, Verbose,
		TEXT("[Parallel] ENTER — FailurePolicy=%d SuccessPolicy=%d ChildFinishPolicy=%d Children=%d"),
		ParallelNode ? (int32)ParallelNode->FailurePolicy : -1,
		ParallelNode ? (int32)ParallelNode->SuccessPolicy : -1,
		ParallelNode ? (int32)ParallelNode->ChildFinishPolicy : -1,
		ChildTasks.Num());
	return true;
}

EBehaviorUStatus UBehaviorUParallelTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUParallel* ParallelNode = Cast<UBehaviorUParallel>(Node);
	if (!ParallelNode)
	{
		return EBehaviorUStatus::Failure;
	}

	int32 SuccessCount = 0;
	int32 FailCount = 0;
	int32 RunningCount = 0;

	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		// Once 策略：子节点完成后本帧继续记录其结果，不再重新执行
		bool bSkipping = (ParallelNode->ChildFinishPolicy == EBehaviorUChildFinishPolicy::Once &&
			ChildStatuses[i] != EBehaviorUStatus::Invalid &&
			ChildStatuses[i] != EBehaviorUStatus::Running);

		// Loop 策略：子节点已完成（非 Running）时，先 Reset 使其下帧从头执行。
		// 本帧仍计入上一次的结果，避免同一帧内立刻重入 OnEnter 导致 Wait 等节点计时被重置。
		bool bNeedsLoopReset = (ParallelNode->ChildFinishPolicy == EBehaviorUChildFinishPolicy::Loop &&
			ChildStatuses[i] != EBehaviorUStatus::Invalid &&
			ChildStatuses[i] != EBehaviorUStatus::Running);

		BEHAVIORU_VLOG(TEXT("[Parallel] Child[%d] FinishPolicy=%s CachedStatus=%d → %s"),
			i,
			ParallelNode->ChildFinishPolicy == EBehaviorUChildFinishPolicy::Loop ? TEXT("Loop") : TEXT("Once"),
			(int32)ChildStatuses[i],
			bSkipping ? TEXT("SKIP") : (bNeedsLoopReset ? TEXT("LOOP_RESET") : TEXT("EXECUTE")));

		if (bSkipping)
		{
			if (ChildStatuses[i] == EBehaviorUStatus::Success) SuccessCount++;
			else if (ChildStatuses[i] == EBehaviorUStatus::Failure) FailCount++;
			continue;
		}

		if (bNeedsLoopReset)
		{
			// 本帧仍计入上次结果，同时 Reset 子节点，使其下帧重新 OnEnter
			if (ChildStatuses[i] == EBehaviorUStatus::Success) SuccessCount++;
			else if (ChildStatuses[i] == EBehaviorUStatus::Failure) FailCount++;
			ChildTasks[i]->Reset(Agent);
			ChildStatuses[i] = EBehaviorUStatus::Invalid;
			continue;
		}

		EBehaviorUStatus Result = ChildTasks[i]->Execute(Agent, EBehaviorUStatus::Invalid);
		ChildStatuses[i] = Result;

		BEHAVIORU_VLOG(TEXT("[Parallel] Child[%d] executed → %d"), i, (int32)Result);

		switch (Result)
		{
		case EBehaviorUStatus::Success:	SuccessCount++; break;
		case EBehaviorUStatus::Failure:	FailCount++; break;
		case EBehaviorUStatus::Running:	RunningCount++; break;
		default: break;
		}
	}

	// 首先判断失败条件
	switch (ParallelNode->FailurePolicy)
	{
	case EBehaviorUParallelPolicy::RequireAll:
		if (FailCount > 0) return EBehaviorUStatus::Failure;
		break;
	case EBehaviorUParallelPolicy::RequireOne:
		if (FailCount == ChildTasks.Num()) return EBehaviorUStatus::Failure;
		break;
	case EBehaviorUParallelPolicy::FirstCompletes:
		if (FailCount > 0) return EBehaviorUStatus::Failure;
		break;
	}

	// 然后判断成功条件
	switch (ParallelNode->SuccessPolicy)
	{
	case EBehaviorUParallelPolicy::RequireAll:
		if (SuccessCount == ChildTasks.Num()) return EBehaviorUStatus::Success;
		break;
	case EBehaviorUParallelPolicy::RequireOne:
		if (SuccessCount > 0) return EBehaviorUStatus::Success;
		break;
	case EBehaviorUParallelPolicy::FirstCompletes:
		if (SuccessCount > 0) return EBehaviorUStatus::Success;
		break;
	}

	return EBehaviorUStatus::Running;
}

// ===================================================================
// IF-ELSE
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUIfElse::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUIfElseTask>(Outer);
}

bool UBehaviorUIfElseTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	return ChildTasks.Num() >= 2;
}

EBehaviorUStatus UBehaviorUIfElseTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTasks.Num() < 2)
	{
		return EBehaviorUStatus::Failure;
	}

	// First child is the condition
	if (ActiveChildIndex == 0)
	{
		EBehaviorUStatus CondResult = ChildTasks[0]->Execute(Agent, EBehaviorUStatus::Invalid);

		if (CondResult == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}

		// Select branch: index 1 for true, index 2 for false
		ActiveChildIndex = (CondResult == EBehaviorUStatus::Success) ? 1 : 2;
	}

	// Execute selected branch
	if (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		return ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);
	}

	return EBehaviorUStatus::Failure;
}

// ===================================================================
// SELECTOR LOOP
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSelectorLoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSelectorLoopTask>(Outer);
}

bool UBehaviorUSelectorLoopTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

EBehaviorUStatus UBehaviorUSelectorLoopTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	BEHAVIORU_VLOG(TEXT("[SelectorLoop] OnUpdate — ActiveChild=%d, ChildCount=%d"), ActiveChildIndex, ChildTasks.Num());

	// Re-evaluate from the beginning to check if higher priority child is valid
	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		// If a higher-priority child can now run, interrupt current
		if (i < ActiveChildIndex)
		{
			UBehaviorUBehaviorNode* ChildNode = Node ? Node->GetChild(i) : nullptr;
			if (ChildNode)
			{
				// Reset and try this child
				EBehaviorUStatus Result = ChildTasks[i]->Execute(Agent, EBehaviorUStatus::Invalid);
				BEHAVIORU_VLOG(TEXT("[SelectorLoop] High-priority check child[%d] → %d"), i, (int32)Result);
				if (Result != EBehaviorUStatus::Failure)
				{
					// Interrupt current child
					BEHAVIORU_VLOG(TEXT("[SelectorLoop] Interrupting child[%d] → switching to child[%d]"), ActiveChildIndex, i);
					if (ChildTasks.IsValidIndex(ActiveChildIndex))
					{
						ChildTasks[ActiveChildIndex]->Reset(Agent);
					}
					ActiveChildIndex = i;
					return Result;
				}
			}
		}
		else if (i == ActiveChildIndex)
		{
			EBehaviorUStatus Result = ChildTasks[i]->Execute(Agent, ChildStatus);
			BEHAVIORU_VLOG(TEXT("[SelectorLoop] Active child[%d] → %d"), i, (int32)Result);

			if (Result == EBehaviorUStatus::Running)
			{
				return EBehaviorUStatus::Running;
			}

			if (Result == EBehaviorUStatus::Success)
			{
				return EBehaviorUStatus::Success;
			}

			ActiveChildIndex++;
		}
	}

	BEHAVIORU_VLOG(TEXT("[SelectorLoop] All children exhausted → Failure"));
	return EBehaviorUStatus::Failure;
}

// ===================================================================
// SELECTOR PROBABILITY
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSelectorProbability::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSelectorProbabilityTask>(Outer);
}

bool UBehaviorUSelectorProbabilityTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	if (ChildTasks.Num() == 0) return false;

	// Collect weights from DecoratorWeight children; default weight = 1.0
	TArray<float> Weights;
	Weights.Reserve(ChildTasks.Num());
	float TotalWeight = 0.0f;

	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		float W = 1.0f;
		if (Node && Node->GetChild(i))
		{
			const UBehaviorUDecoratorWeight* WeightNode = Cast<UBehaviorUDecoratorWeight>(Node->GetChild(i));
			if (WeightNode)
			{
				W = FMath::Max(0.0f, WeightNode->Weight);
			}
		}
		Weights.Add(W);
		TotalWeight += W;
	}

	// Pick weighted random child
	ActiveChildIndex = 0;
	if (TotalWeight > 0.0f)
	{
		float Pick = FMath::FRandRange(0.0f, TotalWeight);
		float Cumulative = 0.0f;
		for (int32 i = 0; i < Weights.Num(); i++)
		{
			Cumulative += Weights[i];
			if (Pick <= Cumulative)
			{
				ActiveChildIndex = i;
				break;
			}
		}
	}
	else
	{
		ActiveChildIndex = FMath::RandRange(0, ChildTasks.Num() - 1);
	}

	return true;
}

EBehaviorUStatus UBehaviorUSelectorProbabilityTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTasks.Num() == 0)
	{
		return EBehaviorUStatus::Failure;
	}

	// ActiveChildIndex was chosen by weighted random in OnEnter.
	// Execute it; if it returns Running, keep running.
	if (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviorUStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);
		if (Result != EBehaviorUStatus::Failure)
		{
			return Result;
		}
	}

	// The selected child failed — fall back to trying the remaining children
	// in order, so the SelectorProbability still makes progress instead of
	// returning Failure immediately. This matches the original behavioru
	// behaviour where a probability selector is still a selector.
	for (int32 i = 0; i < ChildTasks.Num(); ++i)
	{
		if (i == ActiveChildIndex)
		{
			continue;
		}
		EBehaviorUStatus Result = ChildTasks[i]->Execute(Agent, EBehaviorUStatus::Invalid);
		if (Result != EBehaviorUStatus::Failure)
		{
			// Update ActiveChildIndex so the next tick continues THIS child
			// instead of re-executing the originally-weighted (failed) child.
			ActiveChildIndex = i;
			return Result;
		}
	}

	return EBehaviorUStatus::Failure;
}

// ===================================================================
// SELECTOR STOCHASTIC
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSelectorStochastic::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSelectorStochasticTask>(Outer);
}

UBehaviorUSelectorStochasticTask::UBehaviorUSelectorStochasticTask()
{
}

void UBehaviorUSelectorStochasticTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Super::Reset(Agent);
	ShuffledOrder.Empty();
}

bool UBehaviorUSelectorStochasticTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	ActiveChildIndex = 0;

	// Create shuffled order
	ShuffledOrder.SetNum(ChildTasks.Num());
	for (int32 i = 0; i < ShuffledOrder.Num(); i++)
	{
		ShuffledOrder[i] = i;
	}

	// Fisher-Yates shuffle
	for (int32 i = ShuffledOrder.Num() - 1; i > 0; i--)
	{
		int32 j = FMath::RandRange(0, i);
		ShuffledOrder.Swap(i, j);
	}

	return true;
}

EBehaviorUStatus UBehaviorUSelectorStochasticTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildStatus == EBehaviorUStatus::Success)
	{
		return EBehaviorUStatus::Success;
	}

	if (ChildStatus == EBehaviorUStatus::Failure)
	{
		ActiveChildIndex++;
	}

	while (ActiveChildIndex < ShuffledOrder.Num())
	{
		int32 ChildIdx = ShuffledOrder[ActiveChildIndex];
		if (ChildTasks.IsValidIndex(ChildIdx))
		{
			EBehaviorUStatus Result = ChildTasks[ChildIdx]->Execute(Agent, EBehaviorUStatus::Invalid);
			if (Result != EBehaviorUStatus::Failure)
			{
				return Result;
			}
		}
		ActiveChildIndex++;
	}

	return EBehaviorUStatus::Failure;
}

// ===================================================================
// SEQUENCE STOCHASTIC
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUSequenceStochastic::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUSequenceStochasticTask>(Outer);
}

UBehaviorUSequenceStochasticTask::UBehaviorUSequenceStochasticTask()
{
}

void UBehaviorUSequenceStochasticTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Super::Reset(Agent);
	ShuffledOrder.Empty();
}

bool UBehaviorUSequenceStochasticTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	ActiveChildIndex = 0;

	ShuffledOrder.SetNum(ChildTasks.Num());
	for (int32 i = 0; i < ShuffledOrder.Num(); i++)
	{
		ShuffledOrder[i] = i;
	}

	for (int32 i = ShuffledOrder.Num() - 1; i > 0; i--)
	{
		int32 j = FMath::RandRange(0, i);
		ShuffledOrder.Swap(i, j);
	}

	return true;
}

EBehaviorUStatus UBehaviorUSequenceStochasticTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildStatus == EBehaviorUStatus::Failure)
	{
		return EBehaviorUStatus::Failure;
	}

	if (ChildStatus == EBehaviorUStatus::Success)
	{
		ActiveChildIndex++;
	}

	while (ActiveChildIndex < ShuffledOrder.Num())
	{
		int32 ChildIdx = ShuffledOrder[ActiveChildIndex];
		if (ChildTasks.IsValidIndex(ChildIdx))
		{
			EBehaviorUStatus Result = ChildTasks[ChildIdx]->Execute(Agent, EBehaviorUStatus::Invalid);
			if (Result != EBehaviorUStatus::Success)
			{
				return Result;
			}
		}
		ActiveChildIndex++;
	}

	return EBehaviorUStatus::Success;
}

// ===================================================================
// REFERENCE BEHAVIOR
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUReferenceBehavior::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUReferenceBehaviorTask>(Outer);
}

void UBehaviorUReferenceBehavior::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("ReferenceFilename") || Prop.Name == TEXT("ReferenceBehavior"))
		{
			ReferencedTreePath = Prop.Value;
		}
	}
}

bool UBehaviorUReferenceBehaviorTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	// Load the referenced sub-tree from the XML file path stored on the node.
	// This is deferred to OnEnter (rather than Init) because the agent component
	// (needed for path resolution context) is available here, and it allows
	// hot-reloading the sub-tree file between runs.
	const UBehaviorUReferenceBehavior* RefNode = Cast<UBehaviorUReferenceBehavior>(Node);
	if (!RefNode || !Agent)
	{
		return false;
	}

	// Clean up any previous sub-tree task.
	if (SubTreeTask)
	{
		SubTreeTask->Reset(Agent);
		SubTreeTask = nullptr;
	}

	const FString& TreePath = RefNode->ReferencedTreePath;
	if (TreePath.IsEmpty())
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[ReferenceBehavior] ReferencedTreePath is empty."));
		return false;
	}

	// Resolve the path (same logic as UBehaviorUAgentComponent::LoadBehaviorTreeFromXMLFile).
	FString ResolvedPath = TreePath;
	if (ResolvedPath.StartsWith(TEXT("/Game/")))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath.Mid(6));
	}
	else if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath);
	}
	ResolvedPath = FPaths::ConvertRelativePathToFull(ResolvedPath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ResolvedPath))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[ReferenceBehavior] Failed to read sub-tree file: %s"), *ResolvedPath);
		return false;
	}

	UBehaviorUBehaviorTree* SubTreeAsset = NewObject<UBehaviorUBehaviorTree>(GetTransientPackage());
	SubTreeAsset->SourceFilePath = ResolvedPath;
	SubTreeAsset->TreeName = FPaths::GetBaseFilename(ResolvedPath);

	if (!SubTreeAsset->LoadFromXML(FileContent))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[ReferenceBehavior] Failed to parse sub-tree XML: %s"), *ResolvedPath);
		return false;
	}

	UBehaviorUBehaviorNode* SubRootNode = SubTreeAsset->GetRootNode();
	if (!SubRootNode)
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[ReferenceBehavior] Sub-tree has no root node: %s"), *ResolvedPath);
		return false;
	}

	// Create a BehaviorTreeTask wrapping the sub-tree's root node.
	SubTreeTask = NewObject<UBehaviorUBehaviorTreeTask>(this);
	SubTreeTask->Init(SubRootNode);

	UE_LOG(LogBehaviorU, Log, TEXT("[ReferenceBehavior] Loaded sub-tree: %s"), *ResolvedPath);
	return true;
}

void UBehaviorUReferenceBehaviorTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	// Reset the sub-tree so it starts fresh on re-entry.
	if (SubTreeTask && Agent)
	{
		SubTreeTask->Reset(Agent);
	}

	Super::OnExit(Agent, InStatus);
}

EBehaviorUStatus UBehaviorUReferenceBehaviorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (SubTreeTask)
	{
		return SubTreeTask->Tick(Agent);
	}

	return EBehaviorUStatus::Failure;
}

// ===================================================================
// WITH PRECONDITION
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUWithPrecondition::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWithPreconditionTask>(Outer);
}

EBehaviorUStatus UBehaviorUWithPreconditionTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTasks.Num() < 2)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[WithPrecondition] 需要2个子节点，当前只有 %d 个"), ChildTasks.Num());
		return EBehaviorUStatus::Failure;
	}

	// 如果第二个子节点正在运行，继续执行它，不重新检查条件
	if (ChildStatus == EBehaviorUStatus::Running)
	{
		UE_LOG(LogBehaviorU, Verbose, TEXT("[WithPrecondition] 子节点正在运行，继续执行"));
		return ChildTasks[1]->Execute(Agent, ChildStatus);
	}

	// 否则，检查条件
	EBehaviorUStatus PrecondResult = ChildTasks[0]->Execute(Agent, EBehaviorUStatus::Invalid);

	UE_LOG(LogBehaviorU, Verbose, TEXT("[WithPrecondition] 条件检查 → %s"),
		PrecondResult == EBehaviorUStatus::Success ? TEXT("通过") : TEXT("失败"));

	if (PrecondResult != EBehaviorUStatus::Success)
	{
		return EBehaviorUStatus::Failure;
	}

	// 条件通过，执行第二个子节点
	return ChildTasks[1]->Execute(Agent, EBehaviorUStatus::Invalid);
}
