// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BehaviacTypes.h"
#include "BehaviacCommand.h"
#include <atomic>
#include "BehaviacAgent.generated.h"


class UBehaviacBehaviorTree;
class UBehaviacBehaviorTreeTask;
class UBehaviacBehaviorNode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBehaviacMethodDelegate, const FString&, MethodName, EBehaviacStatus&, OutResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBehaviacSignalDelegate, const FString&, SignalName);

/**
 * 单个节点调试快照（仅包含 Running / Success / Failure 状态节点）。
 * 按父节点先序（pre-order）排列，反映树的执行路径。
 */
struct FBehaviacNodeSnapshot
{
	/** 节点 ID（来自 UBehaviacBehaviorNode::NodeId，< 0 表示无效包装节点） */
	int32   NodeId    = -2;
	/** 节点类型名，如 "Selector"、"Sequence"、"Action" 等 */
	FString NodeClass;
	/** 执行状态字符串："Running" / "Success" / "Failure" */
	FString Status;
};

/**
 * 黑板全量快照：供调试服务器逐帧读取 Agent 状态，不含任何 UObject 引用，线程安全拷贝。
 * AgentId 使用组件指针地址（uint64 字符串），与 Agent 生命周期绑定，唯一且稳定。
 */
struct FBehaviacBlackboardSnapshot
{
	/** 组件指针地址（uint64 十进制字符串），作为 Web 端的 agent_id */
	FString AgentId;
	/** 所属 Actor 的显示名称 */
	FString AgentName;
	/** 当前加载的行为树资源名称，未加载时为空 */
	FString TreeName;
	/** 行为树执行状态："Running" / "Success" / "Failure" / "Invalid" */
	FString TreeStatus;
	/** 字符串黑板（Properties TMap 的值拷贝） */
	TMap<FString, FString> Properties;
	/** UObject 黑板：value 为对象 GetName() 结果，对象为 nullptr 时为空字符串 */
	TMap<FString, FString> ObjectPropertyNames;
	/** 本帧所有非 Invalid 节点快照，按父节点先序排列（反映执行路径） */
	TArray<FBehaviacNodeSnapshot> ActiveNodes;
};

/**
 * Single-parameter delegate used for TypeScript method handlers.
 * TS binds to OnMethodNameCalled, executes its logic, then calls SetTSMethodResult()
 * to store the return value — all synchronously on the game thread before Broadcast returns.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBehaviacMethodNameDelegate, const FString&, MethodName);

/**
 * UBehaviacAgentComponent: The central AI agent component for Unreal Engine 5.
 *
 * Attach this to any Actor to give it behavior tree / FSM / HTN capabilities.
 * This replaces the original behaviac Agent class, integrated as a UActorComponent.
 *
 * Features:
 * - Load and execute behavior trees by asset path
 * - Property system (blackboard-like key/value store)
 * - Method binding via delegates and Blueprint events
 * - Signal system for WaitForSignal nodes
 * - Multiple behavior tree support (stack)
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent), DisplayName = "Behaviac Agent")
class BEHAVIACRUNTIME_API UBehaviacAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBehaviacAgentComponent();

	// --- Lifecycle ---

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Behavior Tree Management ---

	/**
	 * Load a behavior tree directly from an XML file on disk.
	 * Accepts: absolute path, path relative to Content/, or /Game/-prefixed virtual path.
	 * No UAsset import step required — edit the XML and call again to hot-reload.
	 */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Agent")
	bool LoadBehaviorTreeFromXMLFile(const FString& FilePath);

	/** Execute one tick of the current behavior tree */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Agent")
	EBehaviacStatus TickBehaviorTree();

	/** Stop and unload the current behavior tree */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Agent")
	void StopBehaviorTree();

	/** Reset the current behavior tree to its initial state */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Agent")
	void ResetBehaviorTree();

	/** Get the current behavior tree status */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Agent")
	EBehaviacStatus GetBehaviorTreeStatus() const;

	/** Whether automatic ticking is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Agent")
	bool bAutoTick;

	/**
	 * XML file to load on BeginPlay.
	 * Accepts absolute paths, paths relative to Content/, or /Game/-prefixed virtual paths.
	 * Leave empty to call LoadBehaviorTreeFromXMLFile manually at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Agent")
	FString AutoLoadXMLFilePath;

	// --- Property System (Blackboard) ---

	/** Set a property value by name */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetPropertyValue(const FString& PropertyName, const FString& Value);

	/** Get a property value by name */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	FString GetPropertyValue(const FString& PropertyName) const;

	/** Check if a property exists */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	bool HasProperty(const FString& PropertyName) const;

	/** Set an integer property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetIntProperty(const FString& PropertyName, int32 Value);

	/** Get an integer property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	int32 GetIntProperty(const FString& PropertyName) const;

	/** Set a float property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetFloatProperty(const FString& PropertyName, float Value);

	/** Get a float property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	float GetFloatProperty(const FString& PropertyName) const;

	/** Set a boolean property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetBoolProperty(const FString& PropertyName, bool Value);

	/** Get a boolean property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	bool GetBoolProperty(const FString& PropertyName) const;

	/** Set an int64 property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetInt64Property(const FString& PropertyName, int64 Value);

	/** Get an int64 property */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	int64 GetInt64Property(const FString& PropertyName) const;

	/** 设置 FVector 属性（序列化为 "X=... Y=... Z=..."）*/
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetVectorProperty(const FString& PropertyName, FVector Value);

	/** 获取 FVector 属性（解析失败时返回 FVector::ZeroVector）*/
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	FVector GetVectorProperty(const FString& PropertyName) const;

	/** 设置 UObject 指针属性（底层以指针形式存储，受 GC 保护）*/
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	void SetObjectProperty(const FString& PropertyName, UObject* Value);

	/** 获取 UObject 指针属性（属性不存在或对象已被销毁时返回 nullptr）*/
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Properties")
	UObject* GetObjectProperty(const FString& PropertyName) const;

	// --- Method System ---

	/** Execute a named method on this agent. Override in Blueprints or bind delegates. */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Methods")
	EBehaviacStatus ExecuteMethod(const FString& MethodName);

	/** Blueprint event called when a method needs to be executed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Behaviac|Methods")
	EBehaviacStatus OnExecuteMethod(const FString& MethodName);

	/** Delegate for binding method implementations from C++ */
	UPROPERTY(BlueprintAssignable, Category = "Behaviac|Methods")
	FBehaviacMethodDelegate OnMethodCalled;

	/** Register a method handler (C++ callback) */
	void RegisterMethodHandler(const FString& MethodName, TFunction<EBehaviacStatus()> Handler);

	/**
	 * TypeScript method handler bridge.
	 *
	 * TypeScript usage:
	 *   agent.OnMethodNameCalled.Add((methodName) => {
	 *       if (methodName === "PickWanderTarget") {
	 *           // ... do work ...
	 *           agent.SetTSMethodResult(methodName, 1); // Success
	 *       }
	 *   });
	 *
	 * Fired synchronously by ExecuteMethod when no C++ handler is registered for
	 * the method. Because Puerts runs on the game thread, the TS callback completes
	 * before Broadcast() returns, so SetTSMethodResult() is always called before
	 * ExecuteMethod reads the stored value.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Behaviac|Methods")
	FBehaviacMethodNameDelegate OnMethodNameCalled;

	/**
	 * Called by TypeScript inside an OnMethodNameCalled handler to return a status.
	 * @param MethodName  Must match the name received in OnMethodNameCalled.
	 * @param Result      The EBehaviacStatus value to return from ExecuteMethod.
	 */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Methods")
	void SetTSMethodResult(const FString& MethodName, EBehaviacStatus Result);

	// --- Signal System ---

	/** Send a signal to this agent */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Signals")
	void SendSignal(const FString& SignalName);

	/** Check if a signal has been set */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Signals")
	bool IsSignalSet(const FString& SignalName) const;

	/** Clear a signal */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Signals")
	void ClearSignal(const FString& SignalName);

	/** Clear all signals */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Signals")
	void ClearAllSignals();

	/** Delegate fired when a signal is received */
	UPROPERTY(BlueprintAssignable, Category = "Behaviac|Signals")
	FBehaviacSignalDelegate OnSignalReceived;

	// --- Event System ---

	/** Fire a named event on this agent (for event attachments) */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Events")
	void FireEvent(const FString& EventName);

	/** Check if an event has been triggered */
	bool HasPendingEvent(const FString& EventName) const;

	/** Consume a pending event */
	void ConsumeEvent(const FString& EventName);

protected:
	/** 字符串黑板：存储所有基础类型属性（int/float/bool/int64/FVector 序列化为字符串）*/
	UPROPERTY()
	TMap<FString, FString> Properties;

	/** UObject 指针黑板：底层以指针存储，UPROPERTY 保证 GC 不回收引用对象 */
	UPROPERTY()
	TMap<FString, TObjectPtr<UObject>> ObjectProperties;

	/** Active signals */
	UPROPERTY()
	TSet<FString> ActiveSignals;

	/** Pending events */
	UPROPERTY()
	TSet<FString> PendingEvents;

	/** Current behavior tree task (runtime execution) */
	UPROPERTY()
	UBehaviacBehaviorTreeTask* CurrentTreeTask;

	/** Loaded behavior tree definition (transient, not a UAsset) */
	UPROPERTY()
	UBehaviacBehaviorTree* CurrentTreeAsset;

	/** Initialize the runtime task tree from a parsed tree definition */
	bool LoadBehaviorTree(UBehaviacBehaviorTree* TreeAsset);

	/** Registered C++ method handlers */
	TMap<FString, TFunction<EBehaviacStatus()>> MethodHandlers;

	/**
	 * Pending results written by TypeScript via SetTSMethodResult().
	 * Consumed immediately by ExecuteMethod() after OnMethodNameCalled fires.
	 */
	TMap<FString, EBehaviacStatus> MethodNameResults;

	/** Critical section for thread safety */
	mutable FCriticalSection PropertyLock;

	/** Critical section protecting ActiveSignals (written on game thread,
	 *  read on worker thread during Phase 2). */
	mutable FCriticalSection SignalLock;

	/** Critical section protecting PendingEvents (written on game thread,
	 *  read on worker thread during Phase 2). */
	mutable FCriticalSection EventLock;

	/**
	 * 是否已向 UBehaviacWorldSubsystem 注册。
	 * 注册后由子系统统一驱动两阶段 Tick，TickComponent 不再执行业务逻辑。
	 * 用于防止 Blueprint/C++ 意外重新启用 ComponentTick 后产生游戏线程双重执行。
	 */
	bool bManagedBySubsystem = false;

	/**
	 * 工作线程 Tick 许可标志（原子操作）。
	 * 游戏线程在 Phase 1（ProcessCommandQueue）写入，工作线程在 Phase 2 读取。
	 * Phase 1/2 之间由 FEvent::Trigger/Wait 提供完整内存屏障，relaxed 语义安全。
	 * 默认为 true（允许 Tick），子类可在 ProcessCommandQueue 中按需关闭。
	 */
	std::atomic<bool> bWorkerTickEnabled{true};

	// --- 命令队列 ---

	/** 自增命令 ID 计数器，每次 EnqueueMethodCommand 调用后递增 */
	uint64 CommandIdCounter = 0;

	/**
	 * Phase 2（工作线程）中由 Action 节点压入的命令队列。
	 * 在下一帧 Phase 1（游戏线程）由 ProcessCommandQueue() 批量执行。
	 */
	TArray<FBehaviacCommand> PendingCommandQueue;

	/**
	 * 命令执行结果表：CommandId → EBehaviacStatus。
	 * ProcessCommandQueue 写入，GetCommandResult 消费（读取即删除）。
	 */
	TMap<uint64, EBehaviacStatus> CommandResults;

public:
	/**
	 * 由 Action 节点在树 Tick 期间调用，把一条 ExecuteMethod 命令压入队列。
	 * @param MethodName  要执行的方法名
	 * @param bNeedResult 是否需要等待结果（false = 即发即忘）
	 * @return 分配给该命令的唯一 CommandId
	 */
	uint64 EnqueueMethodCommand(const FString& MethodName, bool bNeedResult = true);

	/**
	 * 由 Action 节点在下一帧 Tick 时查询执行结果。
	 * 结果被读取后立即从表中移除（消费语义）。
	 * @param CommandId  EnqueueMethodCommand 返回的 ID
	 * @param OutStatus  结果写入此参数
	 * @return 若结果已就绪返回 true，否则返回 false
	 */
	bool GetCommandResult(uint64 CommandId, EBehaviacStatus& OutStatus);

	/**
	 * 由 Action 节点在 OnExit 中调用，清理因树被中断而残留的结果条目。
	 * @param CommandId  要清理的命令 ID
	 */
	void ClearCommandResult(uint64 CommandId);

	/**
	 * 执行 PendingCommandQueue 中的所有命令并把结果写入 CommandResults。
	 * 必须在游戏线程调用（ExecuteMethod 会触发 TS/Blueprint 回调）。
	 * 由 UBehaviacWorldSubsystem 在 Phase 1 统一调用；
	 * 无子系统时由 TickComponent 回退调用。
	 */
	virtual void ProcessCommandQueue();

	/** 当前是否已加载行为树（供子系统在 Phase 2 快速判断） */
	bool HasBehaviorTree() const { return CurrentTreeTask != nullptr; }

	/**
	 * 读取黑板全量快照，供调试服务器逐帧广播使用。
	 * 内部持有 PropertyLock，线程安全；返回值为纯数据拷贝，无 UObject 引用。
	 */
	FBehaviacBlackboardSnapshot GetBlackboardSnapshot() const;

	/** 获取当前加载的行为树名称（来自 UBehaviacBehaviorTree::TreeName），未加载时返回空字符串 */
	FString GetCurrentTreeName() const;

	/** 获取当前行为树的原始 XML 内容（由 LoadFromXML 缓存），未加载或无缓存时返回空字符串 */
	FString GetCurrentTreeSourceXML() const;

	/** 获取当前行为树的 XML 源文件路径（未加载或无源路径时返回空字符串） */
	FString GetCurrentTreeSourceFilePath() const;

	/**
	 * 由 UBehaviacWorldSubsystem 在注册/注销时调用，标记当前管理状态。
	 * 设置为 true 后，TickComponent 将直接返回，防止游戏线程双重执行。
	 */
	void SetManagedBySubsystem(bool bManaged) { bManagedBySubsystem = bManaged; }

	/**
	 * 工作线程在 Phase 2 调用，判断本帧是否允许执行 TickBehaviorTree。
	 * 由 ProcessCommandQueue 在游戏线程写入，此处仅读取（relaxed 即可）。
	 */
	bool IsWorkerTickEnabled() const { return bWorkerTickEnabled.load(std::memory_order_relaxed); }

	/**
	 * 在游戏线程写入工作线程 Tick 许可。
	 * 子类在 ProcessCommandQueue 中调用，Phase 1/2 间的 FEvent 屏障保证可见性。
	 */
	void SetWorkerTickEnabled(bool bEnabled) { bWorkerTickEnabled.store(bEnabled, std::memory_order_relaxed); }
};
