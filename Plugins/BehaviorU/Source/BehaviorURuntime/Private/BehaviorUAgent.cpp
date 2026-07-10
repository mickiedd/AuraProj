// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUAgent.h"
#include "BehaviorUWorldSubsystem.h"
#include "BehaviorUStats.h"
#include "BehaviorTree/BehaviorUBehaviorTree.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UBehaviorUAgentComponent::UBehaviorUAgentComponent()
	: bAutoTick(true)
	, CurrentTreeTask(nullptr)
	, CurrentTreeAsset(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBehaviorUAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	// 向并行子系统注册，由子系统统一驱动两阶段 Tick。
	// RegisterAgent() 内部会设置 bManagedBySubsystem=true 并禁用 ComponentTick。
	if (UWorld* World = GetWorld())
	{
		if (UBehaviorUWorldSubsystem* Subsystem = World->GetSubsystem<UBehaviorUWorldSubsystem>())
		{
			Subsystem->RegisterAgent(this);
		}
	}

	// Auto-load from XML file if a path is configured, skipping the UAsset import step.
	if (!AutoLoadXMLFilePath.IsEmpty())
	{
		LoadBehaviorTreeFromXMLFile(AutoLoadXMLFilePath);
	}
}

void UBehaviorUAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 已被 UBehaviorUWorldSubsystem 管理时直接返回，防止游戏线程执行树 Tick。
	// 即使 ComponentTick 被 Blueprint/C++ 意外重新启用，此处也能阻断双重执行。
	if (bManagedBySubsystem)
	{
		return;
	}

	// 回退路径：子系统不存在时（编辑器预览、单元测试等），在游戏线程单线程执行。
	ProcessCommandQueue();

	if (bAutoTick && CurrentTreeTask)
	{
		TickBehaviorTree();
	}
}

void UBehaviorUAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 从并行子系统注销
	if (UWorld* World = GetWorld())
	{
		if (UBehaviorUWorldSubsystem* Subsystem = World->GetSubsystem<UBehaviorUWorldSubsystem>())
		{
			Subsystem->UnregisterAgent(this);
		}
	}

	StopBehaviorTree();
	Super::EndPlay(EndPlayReason);
}

// --- Behavior Tree Management ---

bool UBehaviorUAgentComponent::LoadBehaviorTree(UBehaviorUBehaviorTree* TreeAsset)
{
	if (!TreeAsset)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] Cannot load null behavior tree"));
		return false;
	}

	StopBehaviorTree();

	CurrentTreeAsset = TreeAsset;

	UBehaviorUBehaviorNode* RootNode = TreeAsset->GetRootNode();
	if (!RootNode)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] Behavior tree has no root node: %s"), *TreeAsset->GetName());
		return false;
	}

	// Create the root task (BehaviorTreeTask wrapping the root node)
	CurrentTreeTask = NewObject<UBehaviorUBehaviorTreeTask>(this);
	
	BEHAVIORU_VLOG(TEXT("[BehaviorU] Creating tasks: RootNode=%d, NodeType=%s, ChildCount=%d"), 
		RootNode != nullptr, 
		RootNode ? *RootNode->GetName() : TEXT("NULL"),
		RootNode ? RootNode->GetChildCount() : -1);
	
	CurrentTreeTask->Init(RootNode);
	
	BEHAVIORU_VLOG(TEXT("[BehaviorU] After Init: CurrentTreeTask->HasChildTask=%d"), 
		CurrentTreeTask->HasChildTask());

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorU] Loaded behavior tree: %s"), *TreeAsset->GetName());
	return true;
}

bool UBehaviorUAgentComponent::LoadBehaviorTreeFromXMLFile(const FString& FilePath)
{
	// Resolve the path to a real filesystem path.
	// Handles three forms:
	//   /Game/Foo/Bar.xml  → <ProjectContentDir>/Foo/Bar.xml  (UE virtual mount)
	//   relative/path.xml  → <ProjectContentDir>/relative/path.xml
	//   C:/absolute/path.xml → used as-is
	FString ResolvedPath = FilePath;
	if (ResolvedPath.StartsWith(TEXT("/Game/")))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath.Mid(6 /*len("/Game/")*/));
	}
	else if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectContentDir(), ResolvedPath);
	}
	ResolvedPath = FPaths::ConvertRelativePathToFull(ResolvedPath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ResolvedPath))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] LoadBehaviorTreeFromXMLFile: failed to read '%s'"), *ResolvedPath);
		return false;
	}

	// Create a transient tree object (not saved to asset registry; held alive by CurrentTreeAsset UPROPERTY)
	UBehaviorUBehaviorTree* Tree = NewObject<UBehaviorUBehaviorTree>(GetTransientPackage());
	Tree->SourceFilePath = ResolvedPath;
	Tree->TreeName = FPaths::GetBaseFilename(ResolvedPath);

	if (!Tree->LoadFromXML(FileContent))
	{
		UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] LoadBehaviorTreeFromXMLFile: XML parse failed for '%s'"), *ResolvedPath);
		return false;
	}

	UE_LOG(LogBehaviorU, Log, TEXT("[BehaviorU] Loaded behavior tree from XML file: %s"), *ResolvedPath);
	return LoadBehaviorTree(Tree);
}

EBehaviorUStatus UBehaviorUAgentComponent::TickBehaviorTree()
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_TickBT);

	if (!CurrentTreeTask)
	{
		UE_LOG(LogBehaviorU, Warning, TEXT("[BehaviorU] TickBehaviorTree: CurrentTreeTask is NULL!"));
		return EBehaviorUStatus::Invalid;
	}

	EBehaviorUStatus Result = CurrentTreeTask->Tick(this);
	return Result;
}

void UBehaviorUAgentComponent::StopBehaviorTree()
{
	if (CurrentTreeTask)
	{
		CurrentTreeTask->Reset(this);
		CurrentTreeTask = nullptr;
	}

	CurrentTreeAsset = nullptr;
}

void UBehaviorUAgentComponent::ResetBehaviorTree()
{
	if (CurrentTreeTask)
	{
		CurrentTreeTask->Reset(this);
	}
}

EBehaviorUStatus UBehaviorUAgentComponent::GetBehaviorTreeStatus() const
{
	if (CurrentTreeTask)
	{
		return CurrentTreeTask->GetTreeStatus();
	}
	return EBehaviorUStatus::Invalid;
}

// --- Property System ---

void UBehaviorUAgentComponent::SetPropertyValue(const FString& PropertyName, const FString& Value)
{
	FScopeLock Lock(&PropertyLock);

	// Strip "Self." prefix if present
	FString CleanName = PropertyName;
	if (CleanName.StartsWith(TEXT("Self.")))
	{
		CleanName = CleanName.Mid(5);
	}

	Properties.Add(CleanName, Value);
}

FString UBehaviorUAgentComponent::GetPropertyValue(const FString& PropertyName) const
{
	FScopeLock Lock(&PropertyLock);

	FString CleanName = PropertyName;
	if (CleanName.StartsWith(TEXT("Self.")))
	{
		CleanName = CleanName.Mid(5);
	}

	const FString* Found = Properties.Find(CleanName);
	return Found ? *Found : FString();
}

bool UBehaviorUAgentComponent::HasProperty(const FString& PropertyName) const
{
	FScopeLock Lock(&PropertyLock);

	FString CleanName = PropertyName;
	if (CleanName.StartsWith(TEXT("Self.")))
	{
		CleanName = CleanName.Mid(5);
	}

	return Properties.Contains(CleanName);
}

void UBehaviorUAgentComponent::SetIntProperty(const FString& PropertyName, int32 Value)
{
	SetPropertyValue(PropertyName, FString::FromInt(Value));
}

int32 UBehaviorUAgentComponent::GetIntProperty(const FString& PropertyName) const
{
	return FCString::Atoi(*GetPropertyValue(PropertyName));
}

void UBehaviorUAgentComponent::SetFloatProperty(const FString& PropertyName, float Value)
{
	SetPropertyValue(PropertyName, FString::SanitizeFloat(Value));
}

float UBehaviorUAgentComponent::GetFloatProperty(const FString& PropertyName) const
{
	return FCString::Atof(*GetPropertyValue(PropertyName));
}

void UBehaviorUAgentComponent::SetBoolProperty(const FString& PropertyName, bool Value)
{
	SetPropertyValue(PropertyName, Value ? TEXT("true") : TEXT("false"));
}

bool UBehaviorUAgentComponent::GetBoolProperty(const FString& PropertyName) const
{
	FString Val = GetPropertyValue(PropertyName);
	return Val.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Val == TEXT("1");
}

void UBehaviorUAgentComponent::SetInt64Property(const FString& PropertyName, int64 Value)
{
	SetPropertyValue(PropertyName, FString::Printf(TEXT("%lld"), Value));
}

int64 UBehaviorUAgentComponent::GetInt64Property(const FString& PropertyName) const
{
	return FCString::Atoi64(*GetPropertyValue(PropertyName));
}

void UBehaviorUAgentComponent::SetVectorProperty(const FString& PropertyName, FVector Value)
{
	// 使用 UE 标准 FVector::ToString() 格式："X=1.000000 Y=2.000000 Z=3.000000"
	SetPropertyValue(PropertyName, Value.ToString());
}

FVector UBehaviorUAgentComponent::GetVectorProperty(const FString& PropertyName) const
{
	FVector Result = FVector::ZeroVector;
	Result.InitFromString(GetPropertyValue(PropertyName));
	return Result;
}

void UBehaviorUAgentComponent::SetObjectProperty(const FString& PropertyName, UObject* Value)
{
	FScopeLock Lock(&PropertyLock);

	// 去除 "Self." 前缀，与其他属性保持一致
	FString CleanName = PropertyName;
	if (CleanName.StartsWith(TEXT("Self.")))
	{
		CleanName = CleanName.Mid(5);
	}

	if (Value)
	{
		ObjectProperties.Add(CleanName, Value);
	}
	else
	{
		// Value 为 nullptr 时视为清除该属性
		ObjectProperties.Remove(CleanName);
	}
}

UObject* UBehaviorUAgentComponent::GetObjectProperty(const FString& PropertyName) const
{
	FScopeLock Lock(&PropertyLock);

	FString CleanName = PropertyName;
	if (CleanName.StartsWith(TEXT("Self.")))
	{
		CleanName = CleanName.Mid(5);
	}

	// TObjectPtr 在访问时会通过 UObject 内部机制确认对象是否仍然有效
	const TObjectPtr<UObject>* Found = ObjectProperties.Find(CleanName);
	return Found ? Found->Get() : nullptr;
}

// --- Method System ---

/**
 * Normalize a method name so that handler lookups and TypeScript broadcasts are
 * convention-independent. Native behaviac editor exports use the form
 * "Self::Agent::PickWanderTarget()" or "Self.PickWanderTarget()"; hand-authored
 * trees (and the documented TypeScript bridge) use the bare "PickWanderTarget".
 *
 * Strips a leading "Self." / "Self::" prefix and a trailing "()"/"(...)" so that
 * all three forms resolve to the same handler key and the same OnMethodNameCalled
 * payload. Without this, a tree exported by the behaviac editor would silently fail
 * to match a bare-name C++/TS handler (ExecuteMethod would fall through to
 * EBehaviorUStatus::Invalid and the Action would never produce its real result).
 */
static FString NormalizeBehaviorUMethodName(const FString& InMethodName)
{
	FString Normalized = InMethodName;

	// Strip leading "Self." or "Self::" prefix.
	if (Normalized.StartsWith(TEXT("Self::")))
	{
		Normalized = Normalized.Mid(6);
	}
	else if (Normalized.StartsWith(TEXT("Self.")))
	{
		Normalized = Normalized.Mid(5);
	}

	// Strip a trailing "()" (with optional whitespace), e.g. "PickWanderTarget()" -> "PickWanderTarget".
	Normalized.TrimEndInline();
	if (Normalized.EndsWith(TEXT("()")))
	{
		Normalized = Normalized.LeftChop(2);
	}

	return Normalized;
}

EBehaviorUStatus UBehaviorUAgentComponent::ExecuteMethod(const FString& MethodName)
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_ExecuteMethod);

	// Work with the normalized (bare) name everywhere downstream so that handler
	// registration, the TypeScript bridge, and result storage all agree on a key
	// regardless of how the tree author wrote the method reference.
	const FString NormalizedName = NormalizeBehaviorUMethodName(MethodName);

	BEHAVIORU_VLOG(TEXT("[BehaviorU] ExecuteMethod called for: '%s' (normalized: '%s')"), *MethodName, *NormalizedName);

#if STATS
	// Dynamic stat ID.
	static TMap<FString, TStatId> FunctionStatIds;
	FString StatName = FString::Printf(TEXT("BehaviorUMethodCall_%s"), *NormalizedName);
	TStatId& StatId = FunctionStatIds.FindOrAdd(StatName);
	if (!StatId.IsValidStat())
	{
		// Create a new stat ID with detailed name
		StatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_TaskGraphTasks>(StatName);
	}
	FScopeCycleCounter FunctionCycleCounter(StatId);
#endif

	// Try TypeScript handler: fire OnMethodNameCalled synchronously, then read the result
	// that TS deposited via SetTSMethodResult() during the broadcast.
	if (OnMethodNameCalled.IsBound())
	{
		MethodNameResults.Remove(NormalizedName);
		OnMethodNameCalled.Broadcast(NormalizedName);
		if (EBehaviorUStatus* TSResult = MethodNameResults.Find(NormalizedName))
		{
			EBehaviorUStatus Result = *TSResult;
			MethodNameResults.Remove(NormalizedName);
			return Result;
		}
	}

	// Then check registered C++ handlers
	if (TFunction<EBehaviorUStatus()>* Handler = MethodHandlers.Find(NormalizedName))
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU] Found C++ handler for '%s', calling it..."), *NormalizedName);
		return (*Handler)();
	}
	else
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU] No C++ handler found for '%s' (have %d handlers registered)"), *NormalizedName, MethodHandlers.Num());

		// Debug: List all registered handlers
		for (const auto& Pair : MethodHandlers)
		{
			BEHAVIORU_VLOG(TEXT("[BehaviorU]    - Registered: '%s'"), *Pair.Key);
		}
	}

	// Try Blueprint delegate
	if (OnMethodCalled.IsBound())
	{
		EBehaviorUStatus Result = EBehaviorUStatus::Invalid;
		OnMethodCalled.Broadcast(NormalizedName, Result);
		if (Result != EBehaviorUStatus::Invalid)
		{
			return Result;
		}
	}

	// Fall back to Blueprint implementable event
	EBehaviorUStatus BlueprintResult = OnExecuteMethod(NormalizedName);
	if (BlueprintResult != EBehaviorUStatus::Invalid)
	{
		return BlueprintResult;
	}

	UE_LOG(LogBehaviorU, Verbose, TEXT("[BehaviorU] No handler for method: %s"), *NormalizedName);
	return EBehaviorUStatus::Invalid;
}

void UBehaviorUAgentComponent::RegisterMethodHandler(const FString& MethodName, TFunction<EBehaviorUStatus()> Handler)
{
	MethodHandlers.Add(MethodName, MoveTemp(Handler));
}

void UBehaviorUAgentComponent::SetTSMethodResult(const FString& MethodName, EBehaviorUStatus Result)
{
	MethodNameResults.Add(MethodName, Result);
}

// --- Signal System ---

void UBehaviorUAgentComponent::SendSignal(const FString& SignalName)
{
	{
		FScopeLock Lock(&SignalLock);
		ActiveSignals.Add(SignalName);
	}
	OnSignalReceived.Broadcast(SignalName);
}

bool UBehaviorUAgentComponent::IsSignalSet(const FString& SignalName) const
{
	FScopeLock Lock(&SignalLock);
	return ActiveSignals.Contains(SignalName);
}

void UBehaviorUAgentComponent::ClearSignal(const FString& SignalName)
{
	FScopeLock Lock(&SignalLock);
	ActiveSignals.Remove(SignalName);
}

void UBehaviorUAgentComponent::ClearAllSignals()
{
	FScopeLock Lock(&SignalLock);
	ActiveSignals.Empty();
}

// --- Event System ---

void UBehaviorUAgentComponent::FireEvent(const FString& EventName)
{
	FScopeLock Lock(&EventLock);
	PendingEvents.Add(EventName);
}

bool UBehaviorUAgentComponent::HasPendingEvent(const FString& EventName) const
{
	FScopeLock Lock(&EventLock);
	return PendingEvents.Contains(EventName);
}

void UBehaviorUAgentComponent::ConsumeEvent(const FString& EventName)
{
	FScopeLock Lock(&EventLock);
	PendingEvents.Remove(EventName);
}

// --- 黑板快照 ---

FBehaviorUBlackboardSnapshot UBehaviorUAgentComponent::GetBlackboardSnapshot() const
{
	FBehaviorUBlackboardSnapshot Snap;

	// AgentId = 组件指针地址（十进制字符串，Web 端用作 agent_id）
	Snap.AgentId = FString::Printf(TEXT("%llu"), static_cast<uint64>(reinterpret_cast<uintptr_t>(this)));

	// AgentName = Owner Actor 名称
	if (const AActor* Owner = GetOwner())
	{
		Snap.AgentName = Owner->GetName();
	}

	// TreeName & TreeStatus
	if (CurrentTreeAsset)
	{
		Snap.TreeName = CurrentTreeAsset->GetName();
	}

	const EBehaviorUStatus Status = CurrentTreeTask ? CurrentTreeTask->GetTreeStatus() : EBehaviorUStatus::Invalid;
	switch (Status)
	{
		case EBehaviorUStatus::Running: Snap.TreeStatus = TEXT("Running"); break;
		case EBehaviorUStatus::Success: Snap.TreeStatus = TEXT("Success"); break;
		case EBehaviorUStatus::Failure: Snap.TreeStatus = TEXT("Failure"); break;
		default:                       Snap.TreeStatus = TEXT("Invalid"); break;
	}

	// 在锁保护下拷贝两块黑板数据
	{
		FScopeLock Lock(&PropertyLock);

		Snap.Properties = Properties;

		for (const TPair<FString, TObjectPtr<UObject>>& Pair : ObjectProperties)
		{
			const UObject* Obj = Pair.Value.Get();
			Snap.ObjectPropertyNames.Add(Pair.Key, Obj ? Obj->GetName() : FString());
		}
	}

	// 遍历任务树，收集所有非 Invalid 节点快照（父节点先序，反映执行路径）
	// Traverse(false=父节点先) 递归访问整棵任务树，无需修改 Task 类。
	if (CurrentTreeTask)
	{
		CurrentTreeTask->Traverse(false, [&](UBehaviorUBehaviorTask* Task) -> bool
		{
			if (!Task || Task->GetStatus() == EBehaviorUStatus::Invalid)
			{
				return true; // 跳过 Invalid 节点，继续向下遍历
			}

			const UBehaviorUBehaviorNode* BNode = Task->GetNode();
			if (!BNode || BNode->NodeId < 0)
			{
				// NodeId < 0 为合成包装节点（如 BehaviorTreeTask 本身），跳过
				return true;
			}

			FBehaviorUNodeSnapshot NodeSnap;
			NodeSnap.NodeId    = BNode->NodeId;
			NodeSnap.NodeClass = BNode->NodeClassName;
			switch (Task->GetStatus())
			{
				case EBehaviorUStatus::Running: NodeSnap.Status = TEXT("Running"); break;
				case EBehaviorUStatus::Success: NodeSnap.Status = TEXT("Success"); break;
				case EBehaviorUStatus::Failure: NodeSnap.Status = TEXT("Failure"); break;
				default:                       NodeSnap.Status = TEXT("Invalid"); break;
			}
			Snap.ActiveNodes.Add(NodeSnap);
			return true; // 继续遍历子节点
		});
	}

	return Snap;
}

// --- 命令队列 ---

uint64 UBehaviorUAgentComponent::EnqueueMethodCommand(const FString& MethodName, bool bNeedResult)
{
	const uint64 CmdId = ++CommandIdCounter;

	FBehaviorUCommand Cmd;
	Cmd.Type        = EBehaviorUCommandType::ExecuteMethod;
	Cmd.MethodName  = MethodName;
	Cmd.CommandId   = CmdId;
	Cmd.bNeedResult = bNeedResult;

	PendingCommandQueue.Add(MoveTemp(Cmd));

	BEHAVIORU_VLOG(TEXT("[BehaviorU] EnqueueMethodCommand: id=%llu method='%s' needResult=%d"),
		CmdId, *MethodName, (int32)bNeedResult);

	return CmdId;
}

void UBehaviorUAgentComponent::ProcessCommandQueue()
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_ProcessCmdQueue);

	if (PendingCommandQueue.Num() == 0)
	{
		return;
	}

	for (const FBehaviorUCommand& Cmd : PendingCommandQueue)
	{
		switch (Cmd.Type)
		{
		case EBehaviorUCommandType::ExecuteMethod:
		{
			BEHAVIORU_VLOG(TEXT("[BehaviorU] ProcessCommandQueue: executing id=%llu method='%s'"),
				Cmd.CommandId, *Cmd.MethodName);

			EBehaviorUStatus Result = ExecuteMethod(Cmd.MethodName);

			if (Cmd.bNeedResult)
			{
				CommandResults.Add(Cmd.CommandId, Result);
			}
			break;
		}
		default:
			break;
		}
	}

	PendingCommandQueue.Reset();
}

bool UBehaviorUAgentComponent::GetCommandResult(uint64 CommandId, EBehaviorUStatus& OutStatus)
{
	if (EBehaviorUStatus* Found = CommandResults.Find(CommandId))
	{
		OutStatus = *Found;
		CommandResults.Remove(CommandId);
		return true;
	}
	return false;
}

void UBehaviorUAgentComponent::ClearCommandResult(uint64 CommandId)
{
	CommandResults.Remove(CommandId);
}

// --- 调试辅助 ---

FString UBehaviorUAgentComponent::GetCurrentTreeName() const
{
	return CurrentTreeAsset ? CurrentTreeAsset->TreeName : FString();
}

FString UBehaviorUAgentComponent::GetCurrentTreeSourceXML() const
{
	if (!CurrentTreeAsset) { return FString(); }

	// 优先使用已缓存的原始 XML
	if (!CurrentTreeAsset->SourceXML.IsEmpty())
	{
		return CurrentTreeAsset->SourceXML;
	}

	// 回退：从 SourceFilePath 读取文件内容，并写入缓存供后续使用
	if (!CurrentTreeAsset->SourceFilePath.IsEmpty())
	{
		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *CurrentTreeAsset->SourceFilePath))
		{
			UE_LOG(LogBehaviorU, Log,
				TEXT("[BehaviorUDebug] 从文件读取行为树 XML 并缓存：%s（%d 字节）"),
				*CurrentTreeAsset->SourceFilePath, FileContent.Len());
			// 写入缓存，避免下次再读文件（const_cast 仅用于此处的惰性初始化，无并发风险）
			const_cast<UBehaviorUBehaviorTree*>(CurrentTreeAsset)->SourceXML = FileContent;
			return FileContent;
		}

		UE_LOG(LogBehaviorU, Warning,
			TEXT("[BehaviorUDebug] 无法读取行为树文件：%s"),
			*CurrentTreeAsset->SourceFilePath);
	}

	return FString();
}

FString UBehaviorUAgentComponent::GetCurrentTreeSourceFilePath() const
{
	if (!CurrentTreeAsset) { return FString(); }
	return CurrentTreeAsset->SourceFilePath;
}
