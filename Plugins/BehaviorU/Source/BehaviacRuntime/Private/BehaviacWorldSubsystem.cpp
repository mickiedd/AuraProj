// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviacWorldSubsystem.h"
#include "BehaviacAgent.h"
#include "BehaviacWorkerThread.h"
#include "BehaviacDebugServer.h"
#include "BehaviacTypes.h"
#include "BehaviacStats.h"
#include "HAL/PlatformMisc.h"

// 工作线程数量上限：超过此值后收益递减（行为树 Tick 通常 IO 轻，CPU 中等）
static constexpr int32 MaxBehaviacWorkerThreads = 8;

// 行为树每秒最大 Tick 次数（Hz）。0 表示不限制，跟随引擎帧率。
// 运行时可通过控制台命令调整：Behaviac.TickRate 10
static TAutoConsoleVariable<float> CVarBehaviacTickRate(
	TEXT("Behaviac.TickRate"),
	10.0f,
	TEXT("Behaviac 行为树每秒最大 Tick 次数（Hz）。0 = 不限制，跟随引擎帧率。"),
	ECVF_Default
);

// ===================================================================
// 子系统生命周期
// ===================================================================

void UBehaviacWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeThreadPool();
	RegisterDebugConsoleCommands();

	UE_LOG(LogBehaviac, Log,
		TEXT("[Behaviac] 并行子系统已初始化，工作线程数: %d  |  输入 Behaviac.Debug.StartServer 启动黑板调试"), NumWorkerThreads);
}

void UBehaviacWorldSubsystem::Deinitialize()
{
	// 先停止调试服务器（可能持有网络句柄，需在子系统销毁前释放）
	if (DebugServer.IsValid())
	{
		DebugServer->Stop();
		DebugServer.Reset();
	}

	UnregisterDebugConsoleCommands();
	ShutdownThreadPool();
	RegisteredAgents.Empty();
	Super::Deinitialize();
}

// ===================================================================
// 帧驱动
// ===================================================================

void UBehaviacWorldSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Tick);
	SET_DWORD_STAT(STAT_Behaviac_RegisteredAgents, (uint32)RegisteredAgents.Num());

	// 调试服务器 I/O 不受行为树 Tick 频率限制，每引擎帧都处理（保证连接/消息响应及时）
	if (DebugServer.IsValid() && DebugServer->IsRunning())
	{
		DebugServer->Tick();
	}

	// --- 行为树 Tick 频率限制 ---
	// Behaviac.TickRate 控制台变量指定每秒最大执行次数，0 表示不限制。
	// 使用时间累积器：每帧累加 DeltaTime，未达到目标间隔则跳过本帧行为树执行。
	// 防积压策略：累积量最多保留 1 个间隔，避免游戏暂停/卡顿后连续多次执行。
	const float TargetTickRate = CVarBehaviacTickRate.GetValueOnGameThread();
	if (TargetTickRate > 0.0f)
	{
		const float TickInterval = 1.0f / TargetTickRate;
		BtTickAccumulator += DeltaTime;
		if (BtTickAccumulator < TickInterval)
		{
			// 本帧跳过行为树 Tick
			return;
		}
		// 消耗一个间隔，保留余量（允许轻微漂移补偿，不积压多次执行）
		BtTickAccumulator -= TickInterval;
		BtTickAccumulator = FMath::Min(BtTickAccumulator, TickInterval);
	}

	// Phase 1：构建有效 Agent 快照，同时处理各自的命令队列
	TArray<UBehaviacAgentComponent*> ActiveAgents;
	ActiveAgents.Reserve(RegisteredAgents.Num());
	Phase1_ProcessCommandQueues(ActiveAgents);

	SET_DWORD_STAT(STAT_Behaviac_ActiveAgents, (uint32)ActiveAgents.Num());

	if (ActiveAgents.IsEmpty())
	{
		return;
	}

	// Phase 2：通过 FRunnable 线程池并行 Tick 行为树
	Phase2_TickBehaviorTrees(ActiveAgents);

	// Phase 2 完成后广播调试数据（反映本次 BT Tick 的最终状态）
	if (DebugServer.IsValid() && DebugServer->IsRunning())
	{
		// 首先向新连接的客户端推送每个 Agent 的行为树 XML 源路径（每客户端仅一次）
		DebugServer->BroadcastTreeSourcePaths(ActiveAgents);
		// 然后广播黑板快照（跟随行为树 Tick 节奏）
		DebugServer->BroadcastSnapshots(ActiveAgents);
	}
}

TStatId UBehaviacWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBehaviacWorldSubsystem, STATGROUP_Tickables);
}

// ===================================================================
// Agent 注册管理
// ===================================================================

void UBehaviacWorldSubsystem::RegisterAgent(UBehaviacAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	RegisteredAgents.AddUnique(Agent);

	// 标记为子系统管理：TickComponent 将直接返回，防止游戏线程双重执行
	Agent->SetManagedBySubsystem(true);

	// 同时禁用 ComponentTick，避免无效调度开销（bManagedBySubsystem 是最终防线）
	Agent->SetComponentTickEnabled(false);

	BEHAVIAC_VLOG(TEXT("[Behaviac] Agent 注册至并行子系统，当前总数: %d"), RegisteredAgents.Num());
}

void UBehaviacWorldSubsystem::UnregisterAgent(UBehaviacAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	// 调试服务器：通知客户端该 Agent 已移除
	if (DebugServer.IsValid() && DebugServer->IsRunning())
	{
		const uint64 AgentId = static_cast<uint64>(reinterpret_cast<uintptr_t>(Agent));
		DebugServer->SendAgentRemoved(AgentId);
	}

	RegisteredAgents.RemoveSingleSwap(Agent);

	// 清除管理标记，允许回退路径（如对象重新注册至其他上下文）
	Agent->SetManagedBySubsystem(false);

	BEHAVIAC_VLOG(TEXT("[Behaviac] Agent 从并行子系统注销，剩余总数: %d"), RegisteredAgents.Num());
}

int32 UBehaviacWorldSubsystem::GetRegisteredAgentCount() const
{
	return RegisteredAgents.Num();
}

// ===================================================================
// FRunnable 线程池管理
// ===================================================================

void UBehaviacWorldSubsystem::InitializeThreadPool()
{
	// 工作线程数 = 逻辑核心数 - 1（保留一个给游戏线程），范围 [1, MaxBehaviacWorkerThreads]
	const int32 NumLogicalCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	NumWorkerThreads = FMath::Clamp(NumLogicalCores - 1, 1, MaxBehaviacWorkerThreads);

	WorkerThreads.Reserve(NumWorkerThreads);
	for (int32 i = 0; i < NumWorkerThreads; ++i)
	{
		WorkerThreads.Add(MakeUnique<FBehaviacWorkerThread>(i));
	}
}

void UBehaviacWorldSubsystem::ShutdownThreadPool()
{
	// Shutdown() 会设置停止标志 + 触发事件唤醒线程 + 等待完成，析构顺序安全
	for (auto& Worker : WorkerThreads)
	{
		Worker->Shutdown();
	}
	WorkerThreads.Empty();
	NumWorkerThreads = 0;
}

// ===================================================================
// 两阶段 Tick 实现
// ===================================================================

void UBehaviacWorldSubsystem::Phase1_ProcessCommandQueues(TArray<UBehaviacAgentComponent*>& OutActiveAgents)
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Phase1);

	// 从后往前遍历，以 RemoveAtSwap 安全清理失效弱引用
	for (int32 i = RegisteredAgents.Num() - 1; i >= 0; --i)
	{
		UBehaviacAgentComponent* Agent = RegisteredAgents[i].Get();
		if (!Agent)
		{
			// 懒惰清理失效引用（Actor 已被销毁）
			RegisteredAgents.RemoveAtSwap(i);
			continue;
		}

		OutActiveAgents.Add(Agent);

		// 游戏线程串行执行：调用 TS/Blueprint/C++ 回调，将结果写入 CommandResults
		Agent->ProcessCommandQueue();
	}
}

void UBehaviacWorldSubsystem::Phase2_TickBehaviorTrees(TArray<UBehaviacAgentComponent*>& ActiveAgents)
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Phase2);

	const int32 AgentCount = ActiveAgents.Num();

	// 均匀切片，分发至工作线程池
	//
	// 线程安全保证：
	//   - 各 Agent 只读写自身独立数据（CommandResults/PendingCommandQueue/Properties/task状态）
	//   - Phase 1 已在游戏线程完成 CommandResults 写入，Phase 2 读取无竞争
	//   - FEvent::Trigger/Wait 提供完整内存屏障，保证 AssignWork 写入对工作线程可见
	//   - 游戏线程在 WaitDone() 阻塞期间不修改任何共享数据
	const int32 ThreadsToUse = FMath::Min(NumWorkerThreads, AgentCount);
	const int32 BatchSize    = FMath::DivideAndRoundUp(AgentCount, ThreadsToUse);

	// 分配切片并逐一唤醒工作线程（StartWork 是非阻塞的）
	for (int32 i = 0; i < ThreadsToUse; ++i)
	{
		const int32 StartIdx = i * BatchSize;
		const int32 Count    = FMath::Min(BatchSize, AgentCount - StartIdx);

		WorkerThreads[i]->AssignWork(ActiveAgents.GetData() + StartIdx, Count);
		WorkerThreads[i]->StartWork();
	}

	// 等待所有参与工作的线程完成（游戏线程在此阻塞直到所有 BT Tick 完成）
	{
		SCOPE_CYCLE_COUNTER(STAT_Behaviac_Phase2_Wait);
		for (int32 i = 0; i < ThreadsToUse; ++i)
		{
			WorkerThreads[i]->WaitDone();
		}
	}
}

// ===================================================================
// 运行时调试控制台命令
// ===================================================================

void UBehaviacWorldSubsystem::RegisterDebugConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	// Register commands and store their names instead of raw pointers. Storing names
	// avoids holding possible dangling pointers into the console manager when the
	// engine teardown order causes console objects to be destroyed earlier than
	// this subsystem's Deinitialize(). Unregistering by name is safer in that case.
	CM.RegisterConsoleCommand(
		TEXT("Behaviac.Debug.StartServer"),
		TEXT("启动黑板调试 WebSocket 服务器。用法：Behaviac.Debug.StartServer [端口号，默认 17654]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UBehaviacWorldSubsystem::Cmd_StartServer),
		ECVF_Default
	);
	DebugConsoleCommandNames.Add(FName(TEXT("Behaviac.Debug.StartServer")));

	CM.RegisterConsoleCommand(
		TEXT("Behaviac.Debug.StopServer"),
		TEXT("停止黑板调试 WebSocket 服务器。"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UBehaviacWorldSubsystem::Cmd_StopServer),
		ECVF_Default
	);
	DebugConsoleCommandNames.Add(FName(TEXT("Behaviac.Debug.StopServer")));

	CM.RegisterConsoleCommand(
		TEXT("Behaviac.Debug.BroadcastInterval"),
		TEXT("设置黑板快照广播帧间隔。0=每帧，N=每N帧一次。用法：Behaviac.Debug.BroadcastInterval [N]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UBehaviacWorldSubsystem::Cmd_SetBroadcastInterval),
		ECVF_Default
	);
	DebugConsoleCommandNames.Add(FName(TEXT("Behaviac.Debug.BroadcastInterval")));
}

void UBehaviacWorldSubsystem::UnregisterDebugConsoleCommands()
{
	// Unregister by name. Guard against missing console manager or empty names.
	IConsoleManager& CM = IConsoleManager::Get();
	for (const FName& CmdName : DebugConsoleCommandNames)
	{
		if (!CmdName.IsNone())
		{
			CM.UnregisterConsoleObject(*CmdName.ToString());
		}
	}
	DebugConsoleCommandNames.Empty();
}

void UBehaviacWorldSubsystem::Cmd_StartServer(const TArray<FString>& Args)
{
	// 懒创建调试服务器
	if (!DebugServer.IsValid())
	{
		DebugServer = MakeUnique<FBehaviacDebugServer>();
	}

	if (DebugServer->IsRunning())
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[BehaviacDebug] 服务器已在运行（端口 %u）。先执行 Behaviac.Debug.StopServer 再重启。"),
			DebugServer->GetPort());
		return;
	}

	const uint32 Port = (Args.Num() > 0) ? static_cast<uint32>(FCString::Atoi(*Args[0])) : 17654u;
	DebugServer->Start(Port);
}

void UBehaviacWorldSubsystem::Cmd_StopServer(const TArray<FString>& Args)
{
	if (!DebugServer.IsValid() || !DebugServer->IsRunning())
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[BehaviacDebug] 调试服务器未在运行。"));
		return;
	}

	DebugServer->Stop();
}

void UBehaviacWorldSubsystem::Cmd_SetBroadcastInterval(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogBehaviac, Warning, TEXT("[BehaviacDebug] 用法：Behaviac.Debug.BroadcastInterval [N]（N 为帧数，0=每帧）"));
		return;
	}

	const int32 Interval = FMath::Max(0, FCString::Atoi(*Args[0]));

	if (DebugServer.IsValid())
	{
		DebugServer->BroadcastIntervalFrames = Interval;
	}

	UE_LOG(LogBehaviac, Log, TEXT("[BehaviacDebug] 广播间隔已设置为 %d 帧（%s）"),
		Interval, Interval == 0 ? TEXT("每帧") : TEXT("间隔模式"));
}
