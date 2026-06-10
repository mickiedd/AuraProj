// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMiscDefines.h"

class UBehaviacAgentComponent;

/**
 * FBehaviacWorkerThread — 持久化行为树工作线程
 *
 * 线程池中的单个工作单元。每帧由 UBehaviacWorldSubsystem 分配一批连续的 Agent
 * 切片，通过 FEvent 事件对驱动执行：
 *
 *   游戏线程:                        工作线程:
 *   AssignWork(slice)
 *   StartWork() → WorkReadyEvent →  唤醒，对切片内每个 Agent 调用 TickBehaviorTree()
 *   WaitDone()  ←  WorkDoneEvent ←  完成，触发 DoneEvent
 *
 * 设计约束：
 *   - 每帧内 AssignWork + StartWork + WaitDone 必须成对调用（游戏线程）
 *   - 工作线程只读写各 Agent 自身独立数据，无跨线程共享写状态
 *   - UObject 访问满足 UE5 非压缩 GC 地址稳定性要求
 */
class BEHAVIACRUNTIME_API FBehaviacWorkerThread : public FRunnable
{
public:
	/**
	 * @param InThreadIndex 线程序号，用于命名（0..N-1）
	 */
	explicit FBehaviacWorkerThread(int32 InThreadIndex);
	virtual ~FBehaviacWorkerThread();

	// --- FRunnable 接口 ---

	virtual bool   Init() override;
	virtual uint32 Run()  override;
	virtual void   Stop() override;

	// --- 工作调度（游戏线程调用） ---

	/**
	 * 设置本帧要处理的 Agent 指针切片。
	 * 指针指向 ActiveAgents 快照内的连续段，生命周期由调用方保证。
	 * 必须在 StartWork() 之前调用。
	 */
	void AssignWork(UBehaviacAgentComponent** InAgents, int32 InCount);

	/** 触发 WorkReadyEvent，唤醒工作线程开始本帧 Tick */
	void StartWork();

	/** 阻塞游戏线程直到工作线程完成本帧 Tick */
	void WaitDone();

	/** 请求线程优雅退出并等待完成（析构时自动调用） */
	void Shutdown();

private:
	/** 底层 UE 线程句柄 */
	FRunnableThread* Thread = nullptr;

	/** 游戏线程 → 工作线程：有新任务（AutoReset，Wait 后自动复位） */
	FEvent* WorkReadyEvent = nullptr;

	/** 工作线程 → 游戏线程：本帧完成（AutoReset） */
	FEvent* WorkDoneEvent = nullptr;

	/**
	 * 本帧 Agent 切片起始指针（不拥有，指向 ActiveAgents 内部）。
	 * 生命周期：AssignWork() 写入，Run() 读取，WaitDone() 返回后失效。
	 */
	UBehaviacAgentComponent** Agents = nullptr;

	/** 本帧切片长度 */
	int32 AgentCount = 0;

	/** 停止标志：设置后工作线程在下次唤醒时退出 Run() */
	FThreadSafeBool bShouldStop;
};
