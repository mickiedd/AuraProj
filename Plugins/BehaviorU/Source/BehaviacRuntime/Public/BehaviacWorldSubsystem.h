// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BehaviacWorkerThread.h"
#include "BehaviacDebugServer.h"
#include "BehaviacWorldSubsystem.generated.h"

class UBehaviacAgentComponent;

/**
 * UBehaviacWorldSubsystem — 行为树并行 Tick 子系统
 *
 * 将每帧的行为树执行拆分为两个严格有序的阶段，实现多 Agent 并行调度：
 *
 *   Phase 1（游戏线程，串行）
 *     对所有 Agent 顺序调用 ProcessCommandQueue()。
 *     ExecuteMethod 内部会触发 TS / Blueprint / C++ 回调，必须在游戏线程执行。
 *
 *   Phase 2（FRunnable 持久工作线程池，并行）
 *     将 Agent 切片均匀分配给固定线程池（线程数 = 逻辑核心数 - 1），
 *     各线程独立对自己的切片调用 TickBehaviorTree()，通过 FEvent 同步。
 *     各 Agent 只读写自身独立数据，无跨 Agent 共享写状态，天然线程安全。
 *
 * 与 ParallelFor 的区别：
 *   - 持久线程，无每帧任务创建/销毁开销
 *   - 优先级可控（TPri_BelowNormal），不抢占游戏线程
 *   - 行为确定，不依赖 TaskGraph 调度策略
 *
 * Agent 在 BeginPlay 时自动注册，EndPlay 时自动注销。
 * 注册后 Agent 的 bManagedBySubsystem=true，ComponentTick 被禁用。
 * 若子系统不存在（如单元测试），Agent 回退至自身单线程 TickComponent。
 */
UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- 子系统生命周期 ---

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 帧驱动 ---

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// --- Agent 注册管理 ---

	/**
	 * 注册一个 Agent，由 UBehaviacAgentComponent::BeginPlay 自动调用。
	 * 注册后设置 bManagedBySubsystem=true 并禁用 ComponentTick。
	 */
	void RegisterAgent(UBehaviacAgentComponent* Agent);

	/**
	 * 注销一个 Agent，由 UBehaviacAgentComponent::EndPlay 自动调用。
	 */
	void UnregisterAgent(UBehaviacAgentComponent* Agent);

	/** 当前注册的 Agent 数量（调试/性能统计） */
	UFUNCTION(BlueprintPure, Category = "Behaviac|Subsystem")
	int32 GetRegisteredAgentCount() const;

	/** 当前活跃工作线程数量（调试用） */
	UFUNCTION(BlueprintPure, Category = "Behaviac|Subsystem")
	int32 GetWorkerThreadCount() const { return NumWorkerThreads; }

	// --- 运行时调试服务器 ---

	/** 获取调试服务器实例（用于 Blueprint 查询状态，运行时可能为 nullptr） */
	UFUNCTION(BlueprintPure, Category = "Behaviac|Debug")
	bool IsDebugServerRunning() const { return DebugServer.IsValid() && DebugServer->IsRunning(); }

private:
	// --- 运行时调试 WebSocket 服务器 ---

	/** 调试服务器实例（按需通过控制台命令启动） */
	TUniquePtr<FBehaviacDebugServer> DebugServer;

	/** 已注册的控制台命令名称，在 Deinitialize() 中注销（使用名称注销可避免持有已失效的指针） */
	TArray<FName> DebugConsoleCommandNames;

	/** 注册所有 Behaviac.Debug.* 控制台命令 */
	void RegisterDebugConsoleCommands();

	/** 注销所有已注册的控制台命令 */
	void UnregisterDebugConsoleCommands();

	// 控制台命令处理函数
	void Cmd_StartServer(const TArray<FString>& Args);
	void Cmd_StopServer(const TArray<FString>& Args);
	void Cmd_SetBroadcastInterval(const TArray<FString>& Args);

private:
	/** 已注册的所有 Agent（弱引用，Agent 销毁时不阻止 GC） */
	TArray<TWeakObjectPtr<UBehaviacAgentComponent>> RegisteredAgents;

	/**
	 * 行为树 Tick 频率限制累积器。
	 * 每帧累加 DeltaTime，超过目标间隔才执行一次行为树 Tick，
	 * 与 Behaviac.TickRate 控制台变量配合使用。
	 */
	float BtTickAccumulator = 0.0f;

	// --- FRunnable 线程池 ---

	/** 持久工作线程数组，子系统初始化时创建，Deinitialize 时销毁 */
	TArray<TUniquePtr<FBehaviacWorkerThread>> WorkerThreads;

	/** 实际创建的工作线程数（= min(逻辑核心数-1, MaxWorkerThreads)） */
	int32 NumWorkerThreads = 0;

	/** 创建固定大小的工作线程池 */
	void InitializeThreadPool();

	/** 优雅关闭所有工作线程 */
	void ShutdownThreadPool();

	// --- 两阶段 Tick ---

	/**
	 * Phase 1：游戏线程串行处理命令队列。
	 * 同时过滤弱引用，输出当前帧有效 Agent 列表。
	 */
	void Phase1_ProcessCommandQueues(TArray<UBehaviacAgentComponent*>& OutActiveAgents);

	/**
	 * Phase 2：将 Agent 切片分发给工作线程池并等待全部完成。
	 */
	void Phase2_TickBehaviorTrees(TArray<UBehaviacAgentComponent*>& ActiveAgents);
};
