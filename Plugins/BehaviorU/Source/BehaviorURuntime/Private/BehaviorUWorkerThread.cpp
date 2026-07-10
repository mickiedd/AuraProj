// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorUWorkerThread.h"
#include "BehaviorUAgent.h"
#include "BehaviorUTypes.h"
#include "BehaviorUStats.h"

// ===================================================================
// 构造 / 析构
// ===================================================================

FBehaviorUWorkerThread::FBehaviorUWorkerThread(int32 InThreadIndex)
	: bShouldStop(false)
{
	// 从平台事件池获取两个 AutoReset 事件（Wait() 后自动复位，无需手动 Reset()）
	WorkReadyEvent = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/false);
	WorkDoneEvent  = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/false);

	// 创建底层线程，优先级低于普通（AI 不是延迟敏感任务，避免抢占游戏线程）
	const FString ThreadName = FString::Printf(TEXT("BehaviorUWorker_%d"), InThreadIndex);
	Thread = FRunnableThread::Create(this, *ThreadName, 0, TPri_BelowNormal);

	BEHAVIORU_VLOG(TEXT("[BehaviorU] 工作线程 %s 已创建"), *ThreadName);
}

FBehaviorUWorkerThread::~FBehaviorUWorkerThread()
{
	Shutdown();

	// 归还事件至平台事件池
	if (WorkReadyEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WorkReadyEvent);
		WorkReadyEvent = nullptr;
	}
	if (WorkDoneEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WorkDoneEvent);
		WorkDoneEvent = nullptr;
	}
}

// ===================================================================
// FRunnable 接口
// ===================================================================

bool FBehaviorUWorkerThread::Init()
{
	return true;
}

uint32 FBehaviorUWorkerThread::Run()
{
	while (true)
	{
		// 挂起等待游戏线程发送工作信号（AutoReset，无 busy-wait）
		WorkReadyEvent->Wait();

		// 检测停止标志（Stop() 会触发 WorkReadyEvent 以唤醒本线程）
		if (bShouldStop)
		{
			break;
		}

		// 对分配的 Agent 切片依次执行行为树 Tick
		// 各 Agent 只读写自身独立数据，无跨线程共享写，天然线程安全。
		// bWorkerTickEnabled 由游戏线程在 Phase 1 写入，FEvent 屏障保证此处读取可见。
		{
			SCOPE_CYCLE_COUNTER(STAT_BehaviorU_WorkerTickAgents);
			for (int32 i = 0; i < AgentCount; ++i)
			{
				UBehaviorUAgentComponent* Agent = Agents[i];
				if (Agent && Agent->bAutoTick && Agent->HasBehaviorTree() && Agent->IsWorkerTickEnabled())
				{
					Agent->TickBehaviorTree();
				}
			}
		}

		// 通知游戏线程本帧 Tick 已全部完成
		WorkDoneEvent->Trigger();
	}

	return 0;
}

void FBehaviorUWorkerThread::Stop()
{
	bShouldStop = true;
	// 触发事件以唤醒 Run() 中的 Wait()，使线程能检测到停止标志并退出
	if (WorkReadyEvent)
	{
		WorkReadyEvent->Trigger();
	}
}

// ===================================================================
// 工作调度（游戏线程调用）
// ===================================================================

void FBehaviorUWorkerThread::AssignWork(UBehaviorUAgentComponent** InAgents, int32 InCount)
{
	// 无需加锁：游戏线程在 StartWork() 之前写入，工作线程在 WorkReadyEvent 之后读取
	// FEvent 的 Trigger/Wait 提供了必要的内存屏障保证
	Agents     = InAgents;
	AgentCount = InCount;
}

void FBehaviorUWorkerThread::StartWork()
{
	WorkReadyEvent->Trigger();
}

void FBehaviorUWorkerThread::WaitDone()
{
	WorkDoneEvent->Wait();
}

// ===================================================================
// 生命周期
// ===================================================================

void FBehaviorUWorkerThread::Shutdown()
{
	if (Thread)
	{
		// 设置停止标志并唤醒线程（Stop() 内部已 Trigger WorkReadyEvent）
		Stop();
		// 等待线程自然退出 Run()
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}
