// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Composites/BehaviacComposites.h"
#include "BehaviorTree/Decorators/BehaviacDecorators.h"
#include "BehaviacAgent.h"
#include "BehaviorTree/BehaviacBehaviorTree.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ===================================================================
// SELECTOR
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSelector::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSelectorTask>(Outer);
}

void UBehaviacSelector::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
}

bool UBehaviacSelector::Evaluate(UBehaviacAgentComponent* Agent) const
{
	for (UBehaviacBehaviorNode* Child : Children)
	{
		const UBehaviacSelector* ChildSelector = Cast<UBehaviacSelector>(Child);
		if (ChildSelector && ChildSelector->Evaluate(Agent))
		{
			return true;
		}
	}
	return false;
}

bool UBehaviacSelector::CheckIfInterrupted(UBehaviacAgentComponent* Agent) const
{
	return false;
}

bool UBehaviacSelectorTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

void UBehaviacSelectorTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
}

EBehaviacStatus UBehaviacSelectorTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// If the current child succeeded or is still running, propagate that.
	// Do NOT fall through to the while-loop when the child is Running — that
	// would re-execute the same child a second time in the same tick.
	if (ChildStatus == EBehaviacStatus::Success)
	{
		return EBehaviacStatus::Success;
	}
	if (ChildStatus == EBehaviacStatus::Running)
	{
		// Continue ticking the currently active child.
		if (ChildTasks.IsValidIndex(ActiveChildIndex))
		{
			return ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviacStatus::Running);
		}
		return EBehaviacStatus::Running;
	}

	// Current child failed, try next
	if (ChildStatus == EBehaviacStatus::Failure)
	{
		ActiveChildIndex++;
	}

	// Try children from current index
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviacStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviacStatus::Invalid);

		if (Result == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}

		if (Result == EBehaviacStatus::Success)
		{
			return EBehaviacStatus::Success;
		}

		// Failure: move to next child
		ActiveChildIndex++;
	}

	// All children failed
	return EBehaviacStatus::Failure;
}

// ===================================================================
// SEQUENCE
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSequence::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSequenceTask>(Outer);
}

void UBehaviacSequence::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
}

bool UBehaviacSequenceTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

void UBehaviacSequenceTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
}

EBehaviacStatus UBehaviacSequenceTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// If current child is running, continue ticking it with Running status.
	// Previously this passed Invalid, which confused composite children that
	// rely on ChildStatus to distinguish first-entry from continuation.
	if (ChildStatus == EBehaviacStatus::Running)
	{
		if (ChildTasks.IsValidIndex(ActiveChildIndex))
		{
			return ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviacStatus::Running);
		}
		// ActiveChildIndex out of range — treat as completion.
		return EBehaviacStatus::Success;
	}

	// If current child failed, propagate failure
	if (ChildStatus == EBehaviacStatus::Failure)
	{
		return EBehaviacStatus::Failure;
	}

	// Current child succeeded, move to next
	if (ChildStatus == EBehaviacStatus::Success)
	{
		ActiveChildIndex++;
	}

	// Try children from current index
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviacStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, EBehaviacStatus::Invalid);

		if (Result == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}

		if (Result == EBehaviacStatus::Failure)
		{
			return EBehaviacStatus::Failure;
		}

		// Success: move to next child
		ActiveChildIndex++;
	}

	// All children succeeded
	return EBehaviacStatus::Success;
}

// ===================================================================
// PARALLEL
// ===================================================================

UBehaviacParallel::UBehaviacParallel()
	: FailurePolicy(EBehaviacParallelPolicy::RequireAll)
	, SuccessPolicy(EBehaviacParallelPolicy::RequireAll)
	, ChildFinishPolicy(EBehaviacChildFinishPolicy::Once)
{
}

UBehaviacBehaviorTask* UBehaviacParallel::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacParallelTask>(Outer);
}

void UBehaviacParallel::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("FailurePolicy"))
		{
			// 支持旧枚举值和新枚举值
			if (Prop.Value == TEXT("FAIL_ON_ONE") || Prop.Value == TEXT("FailOnOne_SucceedOnAll") || Prop.Value == TEXT("RequireAll"))
				FailurePolicy = EBehaviacParallelPolicy::RequireAll;
			else if (Prop.Value == TEXT("FAIL_ON_ALL") || Prop.Value == TEXT("FailOnAll_SucceedOnOne") || Prop.Value == TEXT("RequireOne"))
				FailurePolicy = EBehaviacParallelPolicy::RequireOne;
			else if (Prop.Value == TEXT("FailOnOne_SucceedOnOne") || Prop.Value == TEXT("FirstCompletes"))
				FailurePolicy = EBehaviacParallelPolicy::FirstCompletes;
		}
		else if (Prop.Name == TEXT("SuccessPolicy"))
		{
			// 支持旧枚举值和新枚举值
			if (Prop.Value == TEXT("FAIL_ON_ONE") || Prop.Value == TEXT("FailOnOne_SucceedOnAll") || Prop.Value == TEXT("RequireAll"))
				SuccessPolicy = EBehaviacParallelPolicy::RequireAll;
			else if (Prop.Value == TEXT("FAIL_ON_ALL") || Prop.Value == TEXT("FailOnAll_SucceedOnOne") || Prop.Value == TEXT("RequireOne"))
				SuccessPolicy = EBehaviacParallelPolicy::RequireOne;
			else if (Prop.Value == TEXT("FailOnOne_SucceedOnOne") || Prop.Value == TEXT("FirstCompletes"))
				SuccessPolicy = EBehaviacParallelPolicy::FirstCompletes;
		}
		else if (Prop.Name == TEXT("ChildFinishPolicy"))
		{
			if (Prop.Value == TEXT("CHILDFINISH_LOOP") || Prop.Value == TEXT("Loop"))
				ChildFinishPolicy = EBehaviacChildFinishPolicy::Loop;
			else
			{
				ChildFinishPolicy = EBehaviacChildFinishPolicy::Once;
				BEHAVIAC_VLOG(TEXT("[Parallel] ChildFinishPolicy='%s' → Once (not Loop). UpdateAIState will only run once!"), *Prop.Value);
			}
		}
	}

	UE_LOG(LogBehaviac, Log,
		TEXT("[Parallel] Loaded — FailurePolicy=%d SuccessPolicy=%d ChildFinishPolicy=%d"),
		(int32)FailurePolicy, (int32)SuccessPolicy, (int32)ChildFinishPolicy);
}

UBehaviacParallelTask::UBehaviacParallelTask()
{
}

void UBehaviacParallelTask::Init(UBehaviacBehaviorNode* InNode)
{
	Super::Init(InNode);
	ChildStatuses.SetNum(ChildTasks.Num());
	for (int32 i = 0; i < ChildStatuses.Num(); i++)
	{
		ChildStatuses[i] = EBehaviacStatus::Invalid;
	}
}

void UBehaviacParallelTask::Reset(UBehaviacAgentComponent* Agent)
{
	Super::Reset(Agent);
	for (int32 i = 0; i < ChildStatuses.Num(); i++)
	{
		ChildStatuses[i] = EBehaviacStatus::Invalid;
	}
}

bool UBehaviacParallelTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacParallel* ParallelNode = Cast<UBehaviacParallel>(Node);
	UE_LOG(LogBehaviac, Verbose,
		TEXT("[Parallel] ENTER — FailurePolicy=%d SuccessPolicy=%d ChildFinishPolicy=%d Children=%d"),
		ParallelNode ? (int32)ParallelNode->FailurePolicy : -1,
		ParallelNode ? (int32)ParallelNode->SuccessPolicy : -1,
		ParallelNode ? (int32)ParallelNode->ChildFinishPolicy : -1,
		ChildTasks.Num());
	return true;
}

EBehaviacStatus UBehaviacParallelTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacParallel* ParallelNode = Cast<UBehaviacParallel>(Node);
	if (!ParallelNode)
	{
		return EBehaviacStatus::Failure;
	}

	int32 SuccessCount = 0;
	int32 FailCount = 0;
	int32 RunningCount = 0;

	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		// Once 策略：子节点完成后本帧继续记录其结果，不再重新执行
		bool bSkipping = (ParallelNode->ChildFinishPolicy == EBehaviacChildFinishPolicy::Once &&
			ChildStatuses[i] != EBehaviacStatus::Invalid &&
			ChildStatuses[i] != EBehaviacStatus::Running);

		// Loop 策略：子节点已完成（非 Running）时，先 Reset 使其下帧从头执行。
		// 本帧仍计入上一次的结果，避免同一帧内立刻重入 OnEnter 导致 Wait 等节点计时被重置。
		bool bNeedsLoopReset = (ParallelNode->ChildFinishPolicy == EBehaviacChildFinishPolicy::Loop &&
			ChildStatuses[i] != EBehaviacStatus::Invalid &&
			ChildStatuses[i] != EBehaviacStatus::Running);

		BEHAVIAC_VLOG(TEXT("[Parallel] Child[%d] FinishPolicy=%s CachedStatus=%d → %s"),
			i,
			ParallelNode->ChildFinishPolicy == EBehaviacChildFinishPolicy::Loop ? TEXT("Loop") : TEXT("Once"),
			(int32)ChildStatuses[i],
			bSkipping ? TEXT("SKIP") : (bNeedsLoopReset ? TEXT("LOOP_RESET") : TEXT("EXECUTE")));

		if (bSkipping)
		{
			if (ChildStatuses[i] == EBehaviacStatus::Success) SuccessCount++;
			else if (ChildStatuses[i] == EBehaviacStatus::Failure) FailCount++;
			continue;
		}

		if (bNeedsLoopReset)
		{
			// 本帧仍计入上次结果，同时 Reset 子节点，使其下帧重新 OnEnter
			if (ChildStatuses[i] == EBehaviacStatus::Success) SuccessCount++;
			else if (ChildStatuses[i] == EBehaviacStatus::Failure) FailCount++;
			ChildTasks[i]->Reset(Agent);
			ChildStatuses[i] = EBehaviacStatus::Invalid;
			continue;
		}

		EBehaviacStatus Result = ChildTasks[i]->Execute(Agent, EBehaviacStatus::Invalid);
		ChildStatuses[i] = Result;

		BEHAVIAC_VLOG(TEXT("[Parallel] Child[%d] executed → %d"), i, (int32)Result);

		switch (Result)
		{
		case EBehaviacStatus::Success:	SuccessCount++; break;
		case EBehaviacStatus::Failure:	FailCount++; break;
		case EBehaviacStatus::Running:	RunningCount++; break;
		default: break;
		}
	}

	// 首先判断失败条件
	switch (ParallelNode->FailurePolicy)
	{
	case EBehaviacParallelPolicy::RequireAll:
		if (FailCount > 0) return EBehaviacStatus::Failure;
		break;
	case EBehaviacParallelPolicy::RequireOne:
		if (FailCount == ChildTasks.Num()) return EBehaviacStatus::Failure;
		break;
	case EBehaviacParallelPolicy::FirstCompletes:
		if (FailCount > 0) return EBehaviacStatus::Failure;
		break;
	}

	// 然后判断成功条件
	switch (ParallelNode->SuccessPolicy)
	{
	case EBehaviacParallelPolicy::RequireAll:
		if (SuccessCount == ChildTasks.Num()) return EBehaviacStatus::Success;
		break;
	case EBehaviacParallelPolicy::RequireOne:
		if (SuccessCount > 0) return EBehaviacStatus::Success;
		break;
	case EBehaviacParallelPolicy::FirstCompletes:
		if (SuccessCount > 0) return EBehaviacStatus::Success;
		break;
	}

	return EBehaviacStatus::Running;
}

// ===================================================================
// IF-ELSE
// ===================================================================

UBehaviacBehaviorTask* UBehaviacIfElse::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacIfElseTask>(Outer);
}

bool UBehaviacIfElseTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	return ChildTasks.Num() >= 2;
}

EBehaviacStatus UBehaviacIfElseTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTasks.Num() < 2)
	{
		return EBehaviacStatus::Failure;
	}

	// First child is the condition
	if (ActiveChildIndex == 0)
	{
		EBehaviacStatus CondResult = ChildTasks[0]->Execute(Agent, EBehaviacStatus::Invalid);

		if (CondResult == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}

		// Select branch: index 1 for true, index 2 for false
		ActiveChildIndex = (CondResult == EBehaviacStatus::Success) ? 1 : 2;
	}

	// Execute selected branch
	if (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		return ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);
	}

	return EBehaviacStatus::Failure;
}

// ===================================================================
// SELECTOR LOOP
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSelectorLoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSelectorLoopTask>(Outer);
}

bool UBehaviacSelectorLoopTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	ActiveChildIndex = 0;
	return true;
}

EBehaviacStatus UBehaviacSelectorLoopTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	BEHAVIAC_VLOG(TEXT("[SelectorLoop] OnUpdate — ActiveChild=%d, ChildCount=%d"), ActiveChildIndex, ChildTasks.Num());

	// Re-evaluate from the beginning to check if higher priority child is valid
	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		// If a higher-priority child can now run, interrupt current
		if (i < ActiveChildIndex)
		{
			UBehaviacBehaviorNode* ChildNode = Node ? Node->GetChild(i) : nullptr;
			if (ChildNode)
			{
				// Reset and try this child
				EBehaviacStatus Result = ChildTasks[i]->Execute(Agent, EBehaviacStatus::Invalid);
				BEHAVIAC_VLOG(TEXT("[SelectorLoop] High-priority check child[%d] → %d"), i, (int32)Result);
				if (Result != EBehaviacStatus::Failure)
				{
					// Interrupt current child
					BEHAVIAC_VLOG(TEXT("[SelectorLoop] Interrupting child[%d] → switching to child[%d]"), ActiveChildIndex, i);
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
			EBehaviacStatus Result = ChildTasks[i]->Execute(Agent, ChildStatus);
			BEHAVIAC_VLOG(TEXT("[SelectorLoop] Active child[%d] → %d"), i, (int32)Result);

			if (Result == EBehaviacStatus::Running)
			{
				return EBehaviacStatus::Running;
			}

			if (Result == EBehaviacStatus::Success)
			{
				return EBehaviacStatus::Success;
			}

			ActiveChildIndex++;
		}
	}

	BEHAVIAC_VLOG(TEXT("[SelectorLoop] All children exhausted → Failure"));
	return EBehaviacStatus::Failure;
}

// ===================================================================
// SELECTOR PROBABILITY
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSelectorProbability::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSelectorProbabilityTask>(Outer);
}

bool UBehaviacSelectorProbabilityTask::OnEnter(UBehaviacAgentComponent* Agent)
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
			const UBehaviacDecoratorWeight* WeightNode = Cast<UBehaviacDecoratorWeight>(Node->GetChild(i));
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

EBehaviacStatus UBehaviacSelectorProbabilityTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTasks.Num() == 0)
	{
		return EBehaviacStatus::Failure;
	}

	// ActiveChildIndex was chosen by weighted random in OnEnter.
	// Execute it; if it returns Running, keep running.
	if (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviacStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);
		if (Result != EBehaviacStatus::Failure)
		{
			return Result;
		}
	}

	// The selected child failed — fall back to trying the remaining children
	// in order, so the SelectorProbability still makes progress instead of
	// returning Failure immediately. This matches the original behaviac
	// behaviour where a probability selector is still a selector.
	for (int32 i = 0; i < ChildTasks.Num(); ++i)
	{
		if (i == ActiveChildIndex)
		{
			continue;
		}
		EBehaviacStatus Result = ChildTasks[i]->Execute(Agent, EBehaviacStatus::Invalid);
		if (Result != EBehaviacStatus::Failure)
		{
			// Update ActiveChildIndex so the next tick continues THIS child
			// instead of re-executing the originally-weighted (failed) child.
			ActiveChildIndex = i;
			return Result;
		}
	}

	return EBehaviacStatus::Failure;
}

// ===================================================================
// SELECTOR STOCHASTIC
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSelectorStochastic::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSelectorStochasticTask>(Outer);
}

UBehaviacSelectorStochasticTask::UBehaviacSelectorStochasticTask()
{
}

void UBehaviacSelectorStochasticTask::Reset(UBehaviacAgentComponent* Agent)
{
	Super::Reset(Agent);
	ShuffledOrder.Empty();
}

bool UBehaviacSelectorStochasticTask::OnEnter(UBehaviacAgentComponent* Agent)
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

EBehaviacStatus UBehaviacSelectorStochasticTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildStatus == EBehaviacStatus::Success)
	{
		return EBehaviacStatus::Success;
	}

	if (ChildStatus == EBehaviacStatus::Failure)
	{
		ActiveChildIndex++;
	}

	while (ActiveChildIndex < ShuffledOrder.Num())
	{
		int32 ChildIdx = ShuffledOrder[ActiveChildIndex];
		if (ChildTasks.IsValidIndex(ChildIdx))
		{
			EBehaviacStatus Result = ChildTasks[ChildIdx]->Execute(Agent, EBehaviacStatus::Invalid);
			if (Result != EBehaviacStatus::Failure)
			{
				return Result;
			}
		}
		ActiveChildIndex++;
	}

	return EBehaviacStatus::Failure;
}

// ===================================================================
// SEQUENCE STOCHASTIC
// ===================================================================

UBehaviacBehaviorTask* UBehaviacSequenceStochastic::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacSequenceStochasticTask>(Outer);
}

UBehaviacSequenceStochasticTask::UBehaviacSequenceStochasticTask()
{
}

void UBehaviacSequenceStochasticTask::Reset(UBehaviacAgentComponent* Agent)
{
	Super::Reset(Agent);
	ShuffledOrder.Empty();
}

bool UBehaviacSequenceStochasticTask::OnEnter(UBehaviacAgentComponent* Agent)
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

EBehaviacStatus UBehaviacSequenceStochasticTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildStatus == EBehaviacStatus::Failure)
	{
		return EBehaviacStatus::Failure;
	}

	if (ChildStatus == EBehaviacStatus::Success)
	{
		ActiveChildIndex++;
	}

	while (ActiveChildIndex < ShuffledOrder.Num())
	{
		int32 ChildIdx = ShuffledOrder[ActiveChildIndex];
		if (ChildTasks.IsValidIndex(ChildIdx))
		{
			EBehaviacStatus Result = ChildTasks[ChildIdx]->Execute(Agent, EBehaviacStatus::Invalid);
			if (Result != EBehaviacStatus::Success)
			{
				return Result;
			}
		}
		ActiveChildIndex++;
	}

	return EBehaviacStatus::Success;
}

// ===================================================================
// REFERENCE BEHAVIOR
// ===================================================================

UBehaviacBehaviorTask* UBehaviacReferenceBehavior::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacReferenceBehaviorTask>(Outer);
}

void UBehaviacReferenceBehavior::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("ReferenceFilename") || Prop.Name == TEXT("ReferenceBehavior"))
		{
			ReferencedTreePath = Prop.Value;
		}
	}
}

bool UBehaviacReferenceBehaviorTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	// Load the referenced sub-tree from the XML file path stored on the node.
	// This is deferred to OnEnter (rather than Init) because the agent component
	// (needed for path resolution context) is available here, and it allows
	// hot-reloading the sub-tree file between runs.
	const UBehaviacReferenceBehavior* RefNode = Cast<UBehaviacReferenceBehavior>(Node);
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
		UE_LOG(LogBehaviac, Warning, TEXT("[ReferenceBehavior] ReferencedTreePath is empty."));
		return false;
	}

	// Resolve the path (same logic as UBehaviacAgentComponent::LoadBehaviorTreeFromXMLFile).
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
		UE_LOG(LogBehaviac, Error, TEXT("[ReferenceBehavior] Failed to read sub-tree file: %s"), *ResolvedPath);
		return false;
	}

	UBehaviacBehaviorTree* SubTreeAsset = NewObject<UBehaviacBehaviorTree>(GetTransientPackage());
	SubTreeAsset->SourceFilePath = ResolvedPath;
	SubTreeAsset->TreeName = FPaths::GetBaseFilename(ResolvedPath);

	if (!SubTreeAsset->LoadFromXML(FileContent))
	{
		UE_LOG(LogBehaviac, Error, TEXT("[ReferenceBehavior] Failed to parse sub-tree XML: %s"), *ResolvedPath);
		return false;
	}

	UBehaviacBehaviorNode* SubRootNode = SubTreeAsset->GetRootNode();
	if (!SubRootNode)
	{
		UE_LOG(LogBehaviac, Error, TEXT("[ReferenceBehavior] Sub-tree has no root node: %s"), *ResolvedPath);
		return false;
	}

	// Create a BehaviorTreeTask wrapping the sub-tree's root node.
	SubTreeTask = NewObject<UBehaviacBehaviorTreeTask>(this);
	SubTreeTask->Init(SubRootNode);

	UE_LOG(LogBehaviac, Log, TEXT("[ReferenceBehavior] Loaded sub-tree: %s"), *ResolvedPath);
	return true;
}

void UBehaviacReferenceBehaviorTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
	// Reset the sub-tree so it starts fresh on re-entry.
	if (SubTreeTask && Agent)
	{
		SubTreeTask->Reset(Agent);
	}

	Super::OnExit(Agent, InStatus);
}

EBehaviacStatus UBehaviacReferenceBehaviorTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (SubTreeTask)
	{
		return SubTreeTask->Tick(Agent);
	}

	return EBehaviacStatus::Failure;
}

// ===================================================================
// WITH PRECONDITION
// ===================================================================

UBehaviacBehaviorTask* UBehaviacWithPrecondition::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWithPreconditionTask>(Outer);
}

EBehaviacStatus UBehaviacWithPreconditionTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTasks.Num() < 2)
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[WithPrecondition] 需要2个子节点，当前只有 %d 个"), ChildTasks.Num());
		return EBehaviacStatus::Failure;
	}

	// 如果第二个子节点正在运行，继续执行它，不重新检查条件
	if (ChildStatus == EBehaviacStatus::Running)
	{
		UE_LOG(LogBehaviac, Verbose, TEXT("[WithPrecondition] 子节点正在运行，继续执行"));
		return ChildTasks[1]->Execute(Agent, ChildStatus);
	}

	// 否则，检查条件
	EBehaviacStatus PrecondResult = ChildTasks[0]->Execute(Agent, EBehaviacStatus::Invalid);

	UE_LOG(LogBehaviac, Verbose, TEXT("[WithPrecondition] 条件检查 → %s"),
		PrecondResult == EBehaviacStatus::Success ? TEXT("通过") : TEXT("失败"));

	if (PrecondResult != EBehaviacStatus::Success)
	{
		return EBehaviacStatus::Failure;
	}

	// 条件通过，执行第二个子节点
	return ChildTasks[1]->Execute(Agent, EBehaviacStatus::Invalid);
}
